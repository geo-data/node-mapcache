/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying 
 * LICENSE file.
 */

#include <v8.h>
#include <node.h>
#include <node_buffer.h>

#include "geocache.h"
#include <apr_strings.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_date.h>

using namespace node;
using namespace v8;

#ifdef DEBUG
#include <iostream>
using namespace std;
#endif

// These defines are adapted from node-mapserver
#define REQ_STR_ARG(I, VAR)                              \
  if (args.Length() <= (I) || !args[I]->IsString())      \
    return ThrowException(Exception::TypeError(          \
      String::New("Argument " #I " must be a string"))); \
  String::Utf8Value VAR(args[I]->ToString());

#define REQ_FUN_ARG(I, VAR)                                \
  if (args.Length() <= (I) || !args[I]->IsFunction())      \
    return ThrowException(Exception::TypeError(            \
      String::New("Argument " #I " must be a function"))); \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_EXT_ARG(I, VAR)                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())   \
    return ThrowException(Exception::TypeError(         \
      String::New("Argument " #I " invalid")));         \
  Local<External> VAR = Local<External>::Cast(args[I]);

#define THROW_CSTR_ERROR(TYPE, STR)                         \
  return ThrowException(Exception::TYPE(String::New(STR)));

typedef struct geocache_context_fcgi geocache_context_fcgi;

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
    gctx->set_error(gctx, 500, (char *)"SEVERE: fcgi recursive mutex acquire");
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

static geocache_context_fcgi* fcgi_context_create(apr_pool_t *pool) {
  geocache_context_fcgi *ctx = (geocache_context_fcgi *)apr_pcalloc(pool, sizeof(geocache_context_fcgi));
  if(!ctx) {
    return NULL;
  }
  ctx->ctx.pool = pool;
  geocache_context_init((geocache_context*)ctx);
  ctx->ctx.log = fcgi_context_log;
  ctx->mutex_fname= (char *)"/tmp/geocache.fcgi.lock";
  ctx->ctx.global_lock_aquire = geocache_fcgi_mutex_aquire;
  ctx->ctx.global_lock_release = geocache_fcgi_mutex_release;
  return ctx;
}

/* The structure used for passing cache request data asynchronously
   between threads using libeio */
class GeoCache;                 // forward declaration for this structure
struct cache_request {
  Persistent<Function> cb;
  GeoCache *cache;
  apr_pool_t* pool;
  char *baseUrl;
  char *pathInfo;
  char *queryString;
  char *err;
  geocache_http_response *response;
};

// keys for the http response object
static Persistent<String> code_symbol;
static Persistent<String> data_symbol;
static Persistent<String> mtime_symbol;
static Persistent<String> headers_symbol;

// The GeoCache class
class GeoCache: ObjectWrap
{
private:
  // a combination of a geocache_cfg and memory pool
  struct config_context {
    geocache_cfg *cfg;
    apr_pool_t *pool;
  };

  /* The structure used for passing config file loading data
     asynchronously between threads using libeio */
  struct config_request {
    Persistent<Function> cb;
    apr_pool_t *pool;
    config_context *config;
    char *conffile;
    char *err;
  };

  config_context *config;

  static int EIO_Get(eio_req *req);
  static int EIO_GetAfter(eio_req *req);
  static int EIO_FromConfigFile(eio_req *req);
  static int EIO_FromConfigFileAfter(eio_req *req);
public:

  static config_context* config_context_create(apr_pool_t *pool) {
    config_context *ctx = (config_context *) apr_pcalloc(pool, sizeof(config_context *));
    if (!ctx) {
      return NULL;
    }
    ctx->pool = pool;
    ctx->cfg = NULL;

    return ctx;
  }

  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    constructor_template->SetClassName(String::NewSymbol("GeoCache"));

    code_symbol = NODE_PSYMBOL("code");
    data_symbol = NODE_PSYMBOL("data");
    mtime_symbol = NODE_PSYMBOL("mtime");
    headers_symbol = NODE_PSYMBOL("headers");
    
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "get", GetAsync);
    NODE_SET_METHOD(constructor_template, "FromConfigFile", FromConfigFileAsync);

    target->Set(String::NewSymbol("GeoCache"), constructor_template->GetFunction());
  }

  GeoCache(config_context *config) :
    config(config)
  {
    // should throw an error here if !config
#ifdef DEBUG
    cout << "Instantiating GeoCache instance" << endl;
#endif
  }

  ~GeoCache()
  {
#ifdef DEBUG
    cout << "Destroying GeoCache instance" << endl;
#endif
    apr_pool_destroy(config->pool);
    config = NULL;
  }

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;
    if (!args.IsConstructCall()) {
      THROW_CSTR_ERROR(Error, "GeoCache() is expected to be called as a constructor with the `new` keyword");
    }
    REQ_EXT_ARG(0, config);
    (new GeoCache((config_context *)config->Value()))->Wrap(args.This());
    return args.This();
  }

  // Factory method to create a cache from a configuration file
  static Handle<Value> FromConfigFileAsync(const Arguments& args)
  {
    HandleScope scope;
    const char *usage = "usage: GeoCache.FromConfigFile(configfile)";
    if (args.Length() != 2) {
      THROW_CSTR_ERROR(Error, usage);
    }
    REQ_STR_ARG(0, conffile);
    REQ_FUN_ARG(1, cb);

    // create the global pool if it does not already exist
    if(global_pool == NULL && apr_pool_create(&global_pool, NULL) != APR_SUCCESS) {
      THROW_CSTR_ERROR(Error, "Could not create the global cache memory pool");
    }

    // create the pool for this configuration context
    apr_pool_t *config_pool = NULL;
    if (apr_pool_create(&config_pool, global_pool) != APR_SUCCESS) {
      THROW_CSTR_ERROR(Error, "Could not create the cache configuration memory pool");
    }

    config_request *config_req = (config_request *)apr_pcalloc(config_pool, sizeof(struct config_request));
    if (!config_req) {
      apr_pool_destroy(config_pool);
      THROW_CSTR_ERROR(Error, "malloc in GeoCache::FromConfigFileAsync failed");
    }

    config_req->pool = config_pool;
    config_req->config = NULL;
    config_req->cb = Persistent<Function>::New(cb);

    config_req->conffile = apr_pstrdup(config_pool, *conffile);
    if (!config_req->conffile) {
      apr_pool_destroy(config_pool);
      THROW_CSTR_ERROR(Error, "malloc in GeoCache::FromConfigFileAsync failed");
    }
    
    eio_custom(EIO_FromConfigFile, EIO_PRI_DEFAULT, EIO_FromConfigFileAfter, config_req);
    ev_ref(EV_DEFAULT_UC);
    return Undefined();
  }

  static Handle<Value> GetAsync(const Arguments& args)
  {
    HandleScope scope;
    const char *usage = "usage: cache.get(baseUrl, pathInfo, queryString, callback)";
    if (args.Length() != 4) {
      THROW_CSTR_ERROR(Error, usage);
    }
    REQ_STR_ARG(0, baseUrl);
    REQ_STR_ARG(1, pathInfo);
    REQ_STR_ARG(2, queryString);
    REQ_FUN_ARG(3, cb);

    GeoCache* cache = ObjectWrap::Unwrap<GeoCache>(args.This());
    
    // create the pool for this request
    apr_pool_t *req_pool = NULL;
    if (apr_pool_create(&req_pool, cache->config->pool) != APR_SUCCESS) {
      THROW_CSTR_ERROR(Error, "Could not create the geocache request memory pool");
    }

    cache_request *cache_req = (cache_request *)apr_pcalloc(req_pool, sizeof(struct cache_request));
    if (!cache_req) {
      apr_pool_destroy(req_pool);
      THROW_CSTR_ERROR(Error, "malloc in GeoCache::GetAsync failed");
    }

    cache_req->pool = req_pool;
    cache_req->cache = cache;
    cache_req->cb = Persistent<Function>::New(cb);

    cache_req->baseUrl = apr_pstrdup(req_pool, *baseUrl);
    if (!cache_req->baseUrl) {
      apr_pool_destroy(req_pool);
      THROW_CSTR_ERROR(Error, "malloc in GeoCache::GetAsync failed");
    }
    
    cache_req->pathInfo = apr_pstrdup(req_pool, *pathInfo);
    if (!cache_req->pathInfo) {
      apr_pool_destroy(req_pool);
      THROW_CSTR_ERROR(Error, "malloc in GeoCache::GetAsync failed");
    }
    
    cache_req->queryString = apr_pstrdup(req_pool, *queryString);
    if (!cache_req->queryString) {
      apr_pool_destroy(req_pool);
      THROW_CSTR_ERROR(Error, "malloc in GeoCache::GetAsync failed");
    }

    cache->Ref(); // increment reference count so cache is not garbage collected

    eio_custom(EIO_Get, EIO_PRI_DEFAULT, EIO_GetAfter, cache_req);

    ev_ref(EV_DEFAULT_UC);

    return Undefined();
  }    
};

// This is run in a separate thread: *No* contact should be made with
// the Node/V8 world here.
int GeoCache::EIO_Get(eio_req *req) {
  cache_request *cache_req = (cache_request *)req->data;
  geocache_context *ctx;

  apr_table_t *params;
  geocache_request *request = NULL;
  geocache_http_response *http_response = NULL;

  // set up the local context
  ctx = (geocache_context *)fcgi_context_create(cache_req->pool);
  if (!ctx) {
    cache_req->err = (char *)"Could not create the request context";
    return 0;
  }

  // point the context to our cache configuration
  ctx->config = cache_req->cache->config->cfg;

  // parse the query string and dispatch the request
  params = geocache_http_parse_param_string(ctx, cache_req->queryString);
  geocache_service_dispatch_request(ctx ,&request, cache_req->pathInfo, params, ctx->config);
  if (GC_HAS_ERROR(ctx) || !request) {
    http_response = geocache_core_respond_to_error(ctx, (request) ? request->service : NULL);
  } else {
    switch (request->type) {
    case GEOCACHE_REQUEST_GET_CAPABILITIES: {
      geocache_request_get_capabilities *req = (geocache_request_get_capabilities*)request;
      http_response = geocache_core_get_capabilities(ctx, request->service, req, cache_req->baseUrl, cache_req->pathInfo, ctx->config);
      break;
    }
    case GEOCACHE_REQUEST_GET_TILE: {
      geocache_request_get_tile *req_tile = (geocache_request_get_tile*)request;
      http_response = geocache_core_get_tile(ctx, req_tile);
      break;
    }
    case GEOCACHE_REQUEST_PROXY: {
      geocache_request_proxy *req_proxy = (geocache_request_proxy*)request;
      http_response = geocache_core_proxy_request(ctx, req_proxy);
      break;
    }
    case GEOCACHE_REQUEST_GET_MAP: {
      geocache_request_get_map *req_map = (geocache_request_get_map*)request;
      http_response = geocache_core_get_map(ctx, req_map);
      break;
    }
    case GEOCACHE_REQUEST_GET_FEATUREINFO: {
      geocache_request_get_feature_info *req_fi = (geocache_request_get_feature_info*)request;
      http_response = geocache_core_get_featureinfo(ctx, req_fi);
      break;
    }
    default:
      ctx->set_error(ctx, 500, (char*)"###BUG### unknown request type");
      break;
    }

    if (GC_HAS_ERROR(ctx)) {
      http_response = geocache_core_respond_to_error(ctx, request->service);
    } 
  }

  if (!http_response) {
    ctx->set_error(ctx, 500, (char*)"###BUG### NULL response");
    http_response = geocache_core_respond_to_error(ctx, request->service);
  }

  if (!http_response) {
    cache_req->err = (char *)"No response was received from the cache";
  }

  ctx->clear_errors(ctx);
  
  cache_req->response = http_response;

  return 0;
}

int GeoCache::EIO_GetAfter(eio_req *req) {
  HandleScope scope;

  ev_unref(EV_DEFAULT_UC);
  cache_request *cache_req = (cache_request *)req->data;
  GeoCache *gc = cache_req->cache;
  geocache_http_response *response = cache_req->response;

  Handle<Value> argv[2];

  if (cache_req->err) {
    argv[0] = Exception::Error(String::New(cache_req->err));
    argv[1] = Undefined();
  } else {
    // convert the http_response to a javascript object
    Local<Object> result = Object::New();
    result->Set(code_symbol, Integer::New(response->code)); // the HTTP response code

    // set the mtime to as a javascript date
    if (response->mtime) {
      result->Set(mtime_symbol, Date::New(apr_time_as_msec(response->mtime)));
    }

    // set the response data as a Node Buffer object
    if (response->data) {
      result->Set(data_symbol, Buffer::New((char *)response->data->buf, response->data->size)->handle_);
    }

    // Set the response headers as a javascript object with header
    // names as keys and header values as an array. Header values are
    // in an array as more than one header of the same name can be
    // set.
    if (response->headers && !apr_is_empty_table(response->headers)) {
      Local<Object> headers = Object::New();
      const apr_array_header_t *elts = apr_table_elts(response->headers);
      int i;
      for (i = 0; i < elts->nelts; i++) {
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

    argv[0] = Undefined();
    argv[1] = result;
  }

  // pass the results to the user specified callback function
  TryCatch try_catch;
  cache_req->cb->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
  
  // clean up
  cache_req->cb.Dispose();

  gc->Unref(); // decrement the cache reference so it can be garbage collected

  apr_pool_destroy(cache_req->pool); // free all memory for this request

  return 0;
}

// This is run in a separate thread: *No* contact should be made with
// the Node/V8 world here.
int GeoCache::EIO_FromConfigFile(eio_req *req) {
  config_request *config_req = (config_request *)req->data;

  // create the configuration context
  config_context *config = NULL;
  config = config_context_create(config_req->pool);
  if (!config) {
    config_req->err = (char *)"Could not create the cache configuration context";
    return 0;
  }
  config->cfg = geocache_configuration_create(config->pool);

  // create the context for loading the configuration
  geocache_context *ctx = (geocache_context*) fcgi_context_create(config->pool);
  if (!ctx) {
    config_req->err = (char *)"Could not create the context for loading the configuration file";
    return 0;
  }

  ctx->log(ctx, GEOCACHE_DEBUG, (char *)"geocache node conf file: %s", config_req->conffile);

  // parse the configuration file
  geocache_configuration_parse(ctx, config_req->conffile, config->cfg, 1);
  if(GC_HAS_ERROR(ctx)) {
    config_req->err = apr_psprintf(config->pool, "failed to parse %s: %s", config_req->conffile, ctx->get_error_message(ctx));
    ctx->clear_errors(ctx);
    return 0;
  }

  // setup the context from the configuration
  geocache_configuration_post_config(ctx, config->cfg);
  if(GC_HAS_ERROR(ctx)) {
    config_req->err = apr_psprintf(config->pool, "post-config failed for %s: %s", config_req->conffile, ctx->get_error_message(ctx));
    ctx->clear_errors(ctx);
    return 0;
  }

  config_req->config = config;
  return 0;
}

int GeoCache::EIO_FromConfigFileAfter(eio_req *req) {
  HandleScope scope;

  ev_unref(EV_DEFAULT_UC);
  config_request *config_req = (config_request *)req->data;

  Handle<Value> argv[2];

  if (config_req->err) {
    argv[0] = Exception::Error(String::New(config_req->err));
    argv[1] = Undefined();
    apr_pool_destroy(config_req->pool);
  } else {
    Local<Value> arg  = External::New(config_req->config);
    Persistent<Object> cache(GeoCache::constructor_template->GetFunction()->NewInstance(1, &arg));

    argv[0] = Undefined();
    argv[1] = scope.Close(cache);
  }

  // pass the results to the user specified callback function
  TryCatch try_catch;
  config_req->cb->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
  
  // clean up
  config_req->cb.Dispose();
  
  return 0;
}


Persistent<FunctionTemplate> GeoCache::constructor_template;

extern "C" {
  static void init (Handle<Object> target)
  {
#ifdef DEBUG
    cout << "Initialising GeoCache Module" << endl;
#endif
    apr_initialize();

    GeoCache::Init(target);
  }

  NODE_MODULE(bindings, init)
}
