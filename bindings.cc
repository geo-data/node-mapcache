/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying 
 * LICENSE file.
 */

#include <v8.h>
#include <node.h>
#include <node_buffer.h>

extern "C" {
#include "geocache.h"
}
#include <apr_strings.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_date.h>

using namespace node;
using namespace v8;

#include <iostream>
using namespace std;

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    return ThrowException(Exception::TypeError(                         \
      String::New("Argument " #I " must be a string")));                \
  String::Utf8Value VAR(args[I]->ToString());

#define THROW_CSTR_ERROR(TYPE, STR)                             \
return ThrowException(Exception::TYPE(String::New(STR)));

typedef struct geocache_context_fcgi geocache_context_fcgi;
typedef struct geocache_context_fcgi_request geocache_context_fcgi_request;

apr_pool_t *global_pool = NULL;

struct geocache_context_fcgi {
  geocache_context ctx;
  char *mutex_fname;
  apr_file_t *mutex_file;
};

void fcgi_context_log(geocache_context *c, geocache_log_level level, char *message, ...) {
  va_list args;
  va_start(args,message);
  fprintf(stderr,"%s\n",apr_pvsprintf(c->pool,message,args));
  va_end(args);
}


void geocache_fcgi_mutex_aquire(geocache_context *gctx) {
  geocache_context_fcgi *ctx = (geocache_context_fcgi*)gctx;
  int ret;
#ifdef DEBUG
  if(ctx->mutex_file != NULL) {
    gctx->set_error(gctx, 500, "SEVERE: fcgi recursive mutex acquire");
    return; /* BUG ! */
  }
#endif
  if (apr_file_open(&ctx->mutex_file, ctx->mutex_fname,
                    APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_SHARELOCK | APR_FOPEN_BINARY,
                    APR_OS_DEFAULT, gctx->pool) != APR_SUCCESS) {
    gctx->set_error(gctx, 500, (char *)"failed to create fcgi mutex lockfile %s", ctx->mutex_fname);
    return; /* we could not create the file */
  }
  ret = apr_file_lock(ctx->mutex_file, APR_FLOCK_EXCLUSIVE);
  if (ret != APR_SUCCESS) {
    gctx->set_error(gctx, 500, (char *)"failed to lock fcgi mutex file %s", ctx->mutex_fname);
    return;
  }
}

void geocache_fcgi_mutex_release(geocache_context *gctx) {
  int ret;
  geocache_context_fcgi *ctx = (geocache_context_fcgi*)gctx;
#ifdef DEBUG
  if(ctx->mutex_file == NULL) {
    gctx->set_error(gctx, 500, (char *)"SEVERE: fcgi mutex unlock on unlocked file");
    return; /* BUG ! */
  }
#endif
  ret = apr_file_unlock(ctx->mutex_file);
  if(ret != APR_SUCCESS) {
    gctx->set_error(gctx, 500, (char *)"failed to unlock fcgi mutex file%s",ctx->mutex_fname);
  }
  ret = apr_file_close(ctx->mutex_file);
  if(ret != APR_SUCCESS) {
    gctx->set_error(gctx, 500, (char *)"failed to close fcgi mutex file %s",ctx->mutex_fname);
  }
  ctx->mutex_file = NULL;
}

static geocache_context_fcgi* fcgi_context_create() {
  geocache_context_fcgi *ctx = (geocache_context_fcgi *)apr_pcalloc(global_pool, sizeof(geocache_context_fcgi));
  if(!ctx) {
    return NULL;
  }
  ctx->ctx.pool = global_pool;
  geocache_context_init((geocache_context*)ctx);
  ctx->ctx.log = fcgi_context_log;
  ctx->mutex_fname= (char *)"/tmp/geocache.fcgi.lock";
  ctx->ctx.global_lock_aquire = geocache_fcgi_mutex_aquire;
  ctx->ctx.global_lock_release = geocache_fcgi_mutex_release;
  return ctx;
}


// keys for the http response object
static Persistent<String> code_symbol;
static Persistent<String> data_symbol;
static Persistent<String> mtime_symbol;
static Persistent<String> headers_symbol;

// The GeoCache class
class GeoCache: ObjectWrap
{
private:
  geocache_context_fcgi* globalctx;
  geocache_context* ctx;
  geocache_cfg *cfg;
public:

  static Persistent<FunctionTemplate> s_ct;
  static void Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("GeoCache"));

    code_symbol = NODE_PSYMBOL("code");
    data_symbol = NODE_PSYMBOL("data");
    mtime_symbol = NODE_PSYMBOL("mtime");
    headers_symbol = NODE_PSYMBOL("headers");
    
    NODE_SET_PROTOTYPE_METHOD(s_ct, "get", Get);

    target->Set(String::NewSymbol("GeoCache"),
                s_ct->GetFunction());
  }

  GeoCache() :
    globalctx(fcgi_context_create()),
    ctx(NULL),
    cfg(NULL)
  {
    if (globalctx) {       // should throw an error here if !globalctx
      ctx = (geocache_context*) globalctx;
      cfg = geocache_configuration_create(ctx->pool);
      ctx->config = cfg;
    }
    cout << "Instantiating" << endl;
  }

  ~GeoCache()
  {
    cout << "Destroying" << endl;
    apr_pool_destroy(ctx->pool);
    ctx->clear_errors(ctx);
  }

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    const char *usage = "usage: new GeoCache(configfile)";
    if (args.Length() != 1) {
      THROW_CSTR_ERROR(Error, usage);
    }
    REQ_STR_ARG(0, conffile);

    // create the pool if it does not already exist
    if(global_pool == NULL && apr_pool_create(&global_pool, NULL) != APR_SUCCESS) {
      THROW_CSTR_ERROR(Error, "Could not create the geocache context pool");
    }

    // instantiate our object
    GeoCache* gc = new GeoCache();
    gc->ctx->log(gc->ctx, GEOCACHE_DEBUG, (char *)"geocache node conf file: %s", *conffile);

    // parse the configuration file
    geocache_configuration_parse(gc->ctx, *conffile, gc->cfg, 1);
    if(GC_HAS_ERROR(gc->ctx)) {
      gc->ctx->log(gc->ctx, (geocache_log_level) 500, (char *)"failed to parse %s: %s", *conffile, gc->ctx->get_error_message(gc->ctx));
      THROW_CSTR_ERROR(Error, "failed to parse configuration file"); // TODO: this should include the file name and error message
    }

    // setup the context from the configuration
    geocache_configuration_post_config(gc->ctx, gc->cfg);
    if(GC_HAS_ERROR(gc->ctx)) {
      gc->ctx->log(gc->ctx, (geocache_log_level) 500, (char *)"post-config failed for %s: %s", *conffile, gc->ctx->get_error_message(gc->ctx));
      THROW_CSTR_ERROR(Error, "post-config failed"); // TODO: this should include the file name and error message
    }

    // wrap and return the geocache object
    gc->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> Get(const Arguments& args)
  {
    HandleScope scope;
    const char *usage = "usage: cache.get(baseUrl, pathInfo, queryString)";
    if (args.Length() != 3) {
      THROW_CSTR_ERROR(Error, usage);
    }
    REQ_STR_ARG(0, baseUrl);
    REQ_STR_ARG(1, pathInfo);
    REQ_STR_ARG(2, queryString);

    GeoCache* gc = ObjectWrap::Unwrap<GeoCache>(args.This());

    apr_table_t *params;
    apr_pool_create(&(gc->ctx->pool), global_pool);
    geocache_request *request = NULL;
    geocache_http_response *http_response = NULL;

    // parse the query string and dispatch the request
    params = geocache_http_parse_param_string(gc->ctx, *queryString);

    cout << "got params for " << *queryString << endl;
    
    geocache_service_dispatch_request(gc->ctx ,&request, *pathInfo, params, gc->cfg);
    if(GC_HAS_ERROR(gc->ctx) || !request) {
      http_response = geocache_core_respond_to_error(gc->ctx, (request) ? request->service : NULL);
    } else {
      if(request->type == GEOCACHE_REQUEST_GET_CAPABILITIES) {
        geocache_request_get_capabilities *req = (geocache_request_get_capabilities*)request;
        http_response = geocache_core_get_capabilities(gc->ctx, request->service, req, *baseUrl, *pathInfo, gc->cfg);
      } else if( request->type == GEOCACHE_REQUEST_GET_TILE) {
        geocache_request_get_tile *req_tile = (geocache_request_get_tile*)request;
        http_response = geocache_core_get_tile(gc->ctx, req_tile);
      } else if( request->type == GEOCACHE_REQUEST_PROXY ) {
        geocache_request_proxy *req_proxy = (geocache_request_proxy*)request;
        http_response = geocache_core_proxy_request(gc->ctx, req_proxy);
      } else if( request->type == GEOCACHE_REQUEST_GET_MAP) {
        geocache_request_get_map *req_map = (geocache_request_get_map*)request;
        http_response = geocache_core_get_map(gc->ctx, req_map);
      } else if( request->type == GEOCACHE_REQUEST_GET_FEATUREINFO) {
        geocache_request_get_feature_info *req_fi = (geocache_request_get_feature_info*)request;
        http_response = geocache_core_get_featureinfo(gc->ctx, req_fi);
      } else {
        gc->ctx->set_error(gc->ctx, 500, (char*)"###BUG### unknown request type");
      }

      if(GC_HAS_ERROR(gc->ctx)) {
        http_response = geocache_core_respond_to_error(gc->ctx, request->service);
      } else if(!http_response) {
        gc->ctx->set_error(gc->ctx, 500, (char*)"###BUG### NULL response");
        http_response = geocache_core_respond_to_error(gc->ctx, request->service);
      }
    }

    // convert the http_response to a javascript object
    Local<Object> result = Object::New();
    result->Set(code_symbol, Integer::New(http_response->code)); // the HTTP response code

    // set the mtime to as a javascript date
    if (http_response->mtime) {
      result->Set(mtime_symbol, Date::New(apr_time_as_msec(http_response->mtime)));
    }

    // set the response data as a Node Buffer object
    if (http_response->data) {
      result->Set(data_symbol, Buffer::New((char*)http_response->data->buf, http_response->data->size)->handle_);
    }

    // Set the response headers as a javascript object with header
    // names as keys and header values as an array. Header values are
    // in an array as more than one header of the same name can be
    // set.
    if(http_response->headers && !apr_is_empty_table(http_response->headers)) {
      Local<Object> headers = Object::New();
      const apr_array_header_t *elts = apr_table_elts(http_response->headers);
      int i;
      for(i = 0; i < elts->nelts; i++) {
         apr_table_entry_t entry = APR_ARRAY_IDX(elts, i, apr_table_entry_t);
         Local<Array> values;
         Local<String> key = String::New(entry.key);
         Local<String> value = String::New(entry.val);
         if (headers->Has(key)) {
           // the header exists: append the value
           values = Local<Array>::Cast(headers->Get(key));
           values->Set(values->Length() - 1, value);
         } else {
           // create a new header
           values = Array::New(1);
           values->Set(0, value);
           headers->Set(key, values);
         }
      }
      result->Set(headers_symbol, headers);
   }
    
    // clean up
    apr_pool_destroy(gc->ctx->pool);
    gc->ctx->clear_errors(gc->ctx);

    return scope.Close(result);
  }

};

Persistent<FunctionTemplate> GeoCache::s_ct;

extern "C" {
  static void init (Handle<Object> target)
  {
    cout << "Initialising" << endl;
    apr_pool_initialize();

    GeoCache::Init(target);
  }

  NODE_MODULE(bindings, init);
}
