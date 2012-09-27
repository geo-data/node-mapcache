#include "mapcache.hpp"

// Initialize the static member variables
apr_pool_t *MapCache::global_pool = NULL;
apr_thread_mutex_t *MapCache::thread_mutex = NULL;

// keys for the http response object
static Persistent<String> code_symbol;
static Persistent<String> data_symbol;
static Persistent<String> mtime_symbol;
static Persistent<String> headers_symbol;

Persistent<FunctionTemplate> MapCache::constructor_template;

void MapCache::Init(Handle<Object> target) { 
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("MapCache"));

  code_symbol = NODE_PSYMBOL("code");
  data_symbol = NODE_PSYMBOL("data");
  mtime_symbol = NODE_PSYMBOL("mtime");
  headers_symbol = NODE_PSYMBOL("headers");

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "get", GetAsync);
  NODE_SET_METHOD(constructor_template, "FromConfigFile", FromConfigFileAsync);

  target->Set(String::NewSymbol("MapCache"), constructor_template->GetFunction());
}

Handle<Value> MapCache::New(const Arguments& args) {
  HandleScope scope;
  if (!args.IsConstructCall()) {
    THROW_CSTR_ERROR(Error, "MapCache() is expected to be called as a constructor with the `new` keyword");
  }
  REQ_EXT_ARG(0, config);
  (new MapCache((config_context *)config->Value()))->Wrap(args.This());
  return args.This();
}

// Factory method to create a cache from a configuration file
Handle<Value> MapCache::FromConfigFileAsync(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 2) {
    THROW_CSTR_ERROR(Error, "usage: MapCache.FromConfigFile(configfile, callback)");
  }
  REQ_STR_ARG(0, conffile);
  REQ_FUN_ARG(1, callback);

  // create the global pool if it does not already exist
  if (global_pool == NULL && apr_pool_create(&global_pool, NULL) != APR_SUCCESS) {
    THROW_CSTR_ERROR(Error, "Could not create the global cache memory pool");
  }

  // ensure the thread mutex is initialised
  if (thread_mutex == NULL && apr_thread_mutex_create(&thread_mutex, APR_THREAD_MUTEX_DEFAULT, global_pool) != APR_SUCCESS) {
    THROW_CSTR_ERROR(Error, "Could not create the mapcache thread mutex");
  }

  ConfigBaton *baton = new ConfigBaton();

  // create the pool for this configuration context
  apr_pool_t *config_pool = NULL;
  if (apr_pool_create(&config_pool, global_pool) != APR_SUCCESS) {
    delete baton;
    THROW_CSTR_ERROR(Error, "Could not create the cache configuration memory pool");
  }

  // create the configuration context
  baton->config = CreateConfigContext(config_pool);
  if (!baton->config) {
    apr_pool_destroy(config_pool);
    delete baton;
    THROW_CSTR_ERROR(Error, "Could not create the cache configuration context");
  }

  baton->request.data = baton;
  baton->callback = Persistent<Function>::New(callback);
  baton->conffile = *conffile;

  uv_queue_work(uv_default_loop(), &baton->request, FromConfigFileWork, FromConfigFileAfter);
  return Undefined();
}

Handle<Value> MapCache::GetAsync(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 4) {
    THROW_CSTR_ERROR(Error, "usage: cache.get(baseUrl, pathInfo, queryString, callback)");
  }
  REQ_STR_ARG(0, baseUrl);
  REQ_STR_ARG(1, pathInfo);
  REQ_STR_ARG(2, queryString);
  REQ_FUN_ARG(3, callback);

  MapCache* cache = ObjectWrap::Unwrap<MapCache>(args.This());
  RequestBaton *baton = new RequestBaton();

  // create the pool for this request
  if (apr_pool_create(&(baton->pool), cache->config->pool) != APR_SUCCESS) {
    delete baton;
    THROW_CSTR_ERROR(Error, "Could not create the mapcache request memory pool");
  }

  baton->request.data = baton;
  baton->cache = cache;
  baton->callback = Persistent<Function>::New(callback);
  baton->baseUrl = *baseUrl;
  baton->pathInfo = *pathInfo;
  baton->queryString = *queryString;

  cache->Ref(); // increment reference count so cache is not garbage collected

  uv_queue_work(uv_default_loop(), &baton->request, GetRequestWork, GetRequestAfter);
  return Undefined();
}

// tear down the APR data structures
void MapCache::Destroy() {
  if (thread_mutex) {
    apr_thread_mutex_destroy(thread_mutex);
    thread_mutex = NULL;
  }
  if (global_pool) {
    apr_pool_destroy(global_pool);
    global_pool = NULL;
  }
}

// This is run in a separate thread: *No* contact should be made with
// the Node/V8 world here.
void MapCache::GetRequestWork(uv_work_t *req) {
  // No HandleScope!

  RequestBaton *baton =  static_cast<RequestBaton*>(req->data);
  mapcache_context *ctx;
  apr_table_t *params;
  mapcache_request *request = NULL;
  mapcache_http_response *http_response = NULL;

  // set up the local context
  ctx = (mapcache_context *)CreateRequestContext(baton->pool);
  if (!ctx) {
    baton->error = "Could not create the request context";
    return;
  }

  if (apr_pool_create(&(ctx->process_pool), ctx->pool) != APR_SUCCESS) {
    baton->error = "Could not create the request context memory pool";
  }

  // point the context to our cache configuration
  ctx->config = baton->cache->config->cfg;

#ifdef DEBUG
  cout << "Cache request: " << baton->baseUrl << baton->pathInfo << ((baton->queryString.empty()) ? "" : "?") << baton->queryString << endl;
#endif

  // parse the query string and dispatch the request
  params = mapcache_http_parse_param_string(ctx, (char*) baton->queryString.c_str());
  mapcache_service_dispatch_request(ctx, &request, (char*) baton->pathInfo.c_str(), params, ctx->config);
  if (GC_HAS_ERROR(ctx) || !request) {
    http_response = mapcache_core_respond_to_error(ctx);
  } else {
    switch (request->type) {
    case MAPCACHE_REQUEST_GET_CAPABILITIES: {
      mapcache_request_get_capabilities *req = (mapcache_request_get_capabilities*)request;
      http_response = mapcache_core_get_capabilities(ctx, request->service, req, (char*) baton->baseUrl.c_str(), (char*) baton->pathInfo.c_str(), ctx->config);
      break;
    }
    case MAPCACHE_REQUEST_GET_TILE: {
      mapcache_request_get_tile *req_tile = (mapcache_request_get_tile*)request;
      http_response = mapcache_core_get_tile(ctx, req_tile);
      break;
    }
    case MAPCACHE_REQUEST_PROXY: {
      mapcache_request_proxy *req_proxy = (mapcache_request_proxy*)request;
      http_response = mapcache_core_proxy_request(ctx, req_proxy);
      break;
    }
    case MAPCACHE_REQUEST_GET_MAP: {
      mapcache_request_get_map *req_map = (mapcache_request_get_map*)request;
      http_response = mapcache_core_get_map(ctx, req_map);
      break;
    }
    case MAPCACHE_REQUEST_GET_FEATUREINFO: {
      mapcache_request_get_feature_info *req_fi = (mapcache_request_get_feature_info*)request;
      http_response = mapcache_core_get_featureinfo(ctx, req_fi);
      break;
    }
    default:
      ctx->set_error(ctx, 500, (char*)"###BUG### unknown request type");
      break;
    }

    if (GC_HAS_ERROR(ctx)) {
      http_response = mapcache_core_respond_to_error(ctx);
    }
  }

  if (!http_response) {
    ctx->set_error(ctx, 500, (char*)"###BUG### NULL response");
    http_response = mapcache_core_respond_to_error(ctx);
  }

  if (!http_response) {
    baton->error = "No response was received from the cache";
  }

  ctx->clear_errors(ctx);

  baton->response = http_response;
  return;
}

void MapCache::GetRequestAfter(uv_work_t *req) {
  HandleScope scope;

  RequestBaton *baton = static_cast<RequestBaton*>(req->data);
  MapCache *cache = baton->cache;
  mapcache_http_response *response = baton->response;

  Handle<Value> argv[2];

  if (!baton->error.empty()) {
    argv[0] = Exception::Error(String::New(baton->error.c_str()));
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
  baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  // clean up
  baton->callback.Dispose();
  cache->Unref(); // decrement the cache reference so it can be garbage collected
  apr_pool_destroy(baton->pool); // free all memory for this request
  delete baton;
  return;
}

// This is run in a separate thread: *No* contact should be made with
// the Node/V8 world here.
void MapCache::FromConfigFileWork(uv_work_t *req) {
  // No HandleScope!

  ConfigBaton *baton = static_cast<ConfigBaton*>(req->data);
  config_context *config = baton->config;

  // create the context for loading the configuration
  mapcache_context *ctx = (mapcache_context*) CreateRequestContext(config->pool);
  if (!ctx) {
    baton->error = "Could not create the context for loading the configuration file";
    return;
  }

#ifdef DEBUG
  ctx->log(ctx, MAPCACHE_DEBUG, (char *)"mapcache node conf file: %s", baton->conffile.c_str());
#endif

  // parse the configuration file
  mapcache_configuration_parse(ctx, baton->conffile.c_str(), config->cfg, 1);
  if(GC_HAS_ERROR(ctx)) {
    baton->error = "failed to parse " + baton->conffile + ": " + ctx->get_error_message(ctx);
    ctx->clear_errors(ctx);
    return;
  }

  // setup the context from the configuration
  mapcache_configuration_post_config(ctx, config->cfg);
  if(GC_HAS_ERROR(ctx)) {
    baton->error = "post-config failed for " + baton->conffile + ": " + ctx->get_error_message(ctx);
    ctx->clear_errors(ctx);
    return;
  }

  return;
}

void MapCache::FromConfigFileAfter(uv_work_t *req) {
  HandleScope scope;

  ConfigBaton *baton = static_cast<ConfigBaton*>(req->data);

  Handle<Value> argv[2];

  if (!baton->error.empty()) {
    apr_pool_destroy(baton->config->pool); // free the memory
    argv[0] = Exception::Error(String::New(baton->error.c_str()));
    argv[1] = Undefined();
  } else {
    Local<Value> arg  = External::New(baton->config);
    Persistent<Object> cache(MapCache::constructor_template->GetFunction()->NewInstance(1, &arg));

    argv[0] = Undefined();
    argv[1] = scope.Close(cache);
  }

  // pass the results to the user specified callback function
  TryCatch try_catch;
  baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  // clean up
  baton->callback.Dispose();
  delete baton;
  return;
}

mapcache_context* MapCache::CloneRequestContext(mapcache_context *ctx) {
  mapcache_context *newctx = (mapcache_context*)apr_pcalloc(ctx->pool,
                                                            sizeof(mapcache_context_node));
  mapcache_context_copy(ctx,newctx);
  if (apr_pool_create(&newctx->pool,ctx->pool) != APR_SUCCESS) {
    return NULL;
  }
  return newctx;
}

void MapCache::LogRequestContext(mapcache_context *c, mapcache_log_level level, char *message, ...) {
  va_list args;
  va_start(args,message);
  fprintf(stderr,"%s\n",apr_pvsprintf(c->pool,message,args));
  va_end(args);
}

MapCache::mapcache_context_node* MapCache::CreateRequestContext(apr_pool_t *pool) {
  if (!pool) {
    return NULL;
  }
  mapcache_context *ctx = (mapcache_context *)apr_pcalloc(pool, sizeof(mapcache_context_node));
  if(!ctx) {
    return NULL;
  }
  ctx->pool = pool;
  ctx->process_pool = pool;
  ctx->threadlock = thread_mutex;
  mapcache_context_init(ctx);
  ctx->log = LogRequestContext;
  ctx->clone = CloneRequestContext;
  ctx->config = NULL;
  return (mapcache_context_node *)ctx;
}

MapCache::config_context* MapCache::CreateConfigContext(apr_pool_t *pool) {
  if (!pool) {
    return NULL;
  }
  config_context *ctx = (config_context *) apr_pcalloc(pool, sizeof(config_context));
  if (!ctx) {
    return NULL;
  }
  ctx->pool = pool;
  ctx->cfg = mapcache_configuration_create(pool);
  if (ctx->cfg == NULL) {
    return NULL;
  }

  return ctx;
}
