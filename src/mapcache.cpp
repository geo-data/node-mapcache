/******************************************************************************
 * Copyright (c) 2012, GeoData Institute (www.geodata.soton.ac.uk)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

/**
 * @file mapcache.cpp
 * @brief This defines the primary `MapCache` class.
 */

#include "mapcache.hpp"

/**
 * @details This is a global pool from which the underlying mapcache C
 * code draws its memory.  It is initialised the first time a
 * `MapCache` instance is created.
 */
apr_pool_t *MapCache::global_pool = NULL;

/**
 * @details This is a global mutex used by the underlying mapcache C
 * code to serialise cache functionality that is susceptible to race
 * conditions in multi-threaded applications.  It is initialised the
 * first time a `MapCache` instance is created.
 */
apr_thread_mutex_t *MapCache::thread_mutex = NULL;

/**
 * @defgroup cache_response Properties of the cache response object
 *
 * These represent property names of the items present in the response
 * object returned from a cache request.
 *
 * @{
 */
Persistent<String> MapCache::code_symbol;
Persistent<String> MapCache::data_symbol;
Persistent<String> MapCache::mtime_symbol;
Persistent<String> MapCache::headers_symbol;
/**@}*/

Persistent<FunctionTemplate> MapCache::mapcache_template;

/**
 * @details This is called from the module initialisation function
 * when the module is first loaded by Node. It should only be called
 * once per process.
 *
 * @param target The object representing the module.
 */
void MapCache::Init(Handle<Object> target) {
  HandleScope scope;

  // create the global pool if it does not already exist
  if (apr_pool_create(&global_pool, NULL) != APR_SUCCESS) {
    ThrowException(Exception::Error(String::New("Failed to create global memory pool")));
    return;
  }

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  mapcache_template = Persistent<FunctionTemplate>::New(t);
  mapcache_template->InstanceTemplate()->SetInternalFieldCount(1);
  mapcache_template->SetClassName(String::NewSymbol("MapCache"));

  code_symbol = NODE_PSYMBOL("code");
  data_symbol = NODE_PSYMBOL("data");
  mtime_symbol = NODE_PSYMBOL("mtime");
  headers_symbol = NODE_PSYMBOL("headers");

  NODE_SET_PROTOTYPE_METHOD(mapcache_template, "get", GetAsync);
  NODE_SET_METHOD(mapcache_template, "FromConfigFile", FromConfigFileAsync);

  target->Set(String::NewSymbol("MapCache"), mapcache_template->GetFunction());
}

/**
 * @details This is a constructor method used to return a new
 * `MapCache` instance.
 *
 * `args` should contain the following parameters:
 *
 * @param config An `External` object wrapping a `config_context`
 * instance.
 *
 * @param logger An optional `EventEmitter` object
 */
Handle<Value> MapCache::New(const Arguments& args) {
  HandleScope scope;
  if (!args.IsConstructCall()) {
    THROW_CSTR_ERROR(Error, "MapCache() is expected to be called as a constructor with the `new` keyword");
  }

  Local<Object> logger;
  switch (args.Length()) {
  case 2:
    ASSIGN_OBJ_ARG(1, logger);
  case 1:
    break;
  default:
    THROW_CSTR_ERROR(Error, "usage: new MapCache(config, [logger])");
  }
  REQ_EXT_ARG(0, config);

  MapCache* cache = new MapCache((config_context *)config->Value(), logger);
  cache->Wrap(args.This());
  return scope.Close(args.This());
}

/**
 * @details This is an asynchronous factory method creating a new
 * `MapCache` instance from an mapcache XML configuration file.
 *
 * `args` should contain the following parameters:
 *
 * @param conffile A string representing the configuration file path.
 *
 * @param logger [optional] An `EventEmitter` that can be used to capture
 * mapcache log messages.
 *
 * @param callback A function that is called on error or when the
 * cache has been created. It should have the signature `callback(err,
 * cache)`.
 */
Handle<Value> MapCache::FromConfigFileAsync(const Arguments& args) {
  HandleScope scope;

  Local<Object> emitter;
  Local<Function> callback;
  
  switch (args.Length()) {
  case 3:
    ASSIGN_OBJ_ARG(1, emitter);
    ASSIGN_FUN_ARG(2, callback);
    break;
  case 2:
    ASSIGN_FUN_ARG(1, callback);
    break;
  default:
    THROW_CSTR_ERROR(Error, "usage: MapCache.FromConfigFile(configfile, [logger], callback)");
  }
  REQ_STR_ARG(0, conffile);

  ConfigBaton *baton = new ConfigBaton();

  // create the configuration context
  baton->config = CreateConfigContext();
  if (!baton->config) {
    delete baton;
    THROW_CSTR_ERROR(Error, "Could not create the cache configuration context");
  }

  if (!emitter.IsEmpty()) {
    Persistent<Object> pemitter = Persistent<Object>::New(emitter);
    baton->async_log = new AsyncLog(pemitter);
    baton->logger = pemitter;
  } else {
    baton->async_log = NULL;
  }

  baton->request.data = baton;
  baton->cache = NULL;
  baton->callback = Persistent<Function>::New(callback);
  baton->conffile = *conffile;

  uv_queue_work(uv_default_loop(),
                &baton->request,
                FromConfigFileWork,
                (uv_after_work_cb) FromConfigFileAfter);
  return Undefined();
}

/**
 * @details This is an asynchronous method used to retrieve a resource
 * from the cache. The returned resource is a javascript object
 * literal with the following properties:
 *
 * - `code`: an integer representing the HTTP status code
 * - `mtime`: a date representing the last modified time
 * - `data`: a `Buffer` object representing the cached data
 * - `headers`: the HTTP headers as an object literal
 *
 * `args` should contain the following parameters:
 *
 * @param baseUrl A string defining the base URL of the cache
 * resource.
 *
 * @param pathInfo A string with the URL `PATH_INFO` data.
 *
 * @param queryString A string with the URL `QUERY_STRING` data. This
 * should not be prefixed with a `?`.
 *
 * @param callback A function that is called on error or when the
 * resource has been created. It should have the signature
 * `callback(err, resource)`.
 */
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
  if (apr_pool_create(&(baton->pool), global_pool) != APR_SUCCESS) {
    delete baton;
    THROW_CSTR_ERROR(Error, "Could not create the mapcache request memory pool");
  }

  baton->request.data = baton;
  baton->cache = cache;
  baton->callback = Persistent<Function>::New(callback);
  baton->baseUrl = *baseUrl;
  baton->pathInfo = *pathInfo;
  baton->queryString = *queryString;
  baton->async_log = (!cache->logger.IsEmpty()) ? new AsyncLog(cache->logger) : NULL;

  cache->Ref(); // increment reference count so cache is not garbage collected

  uv_queue_work(uv_default_loop(),
                &baton->request,
                GetRequestWork,
                (uv_after_work_cb) GetRequestAfter);
  return Undefined();
}

/**
 * @details An APR memory pool and thread mutex are created when the
 * first `MapCache` instance is created. This method frees up that
 * memory and should be called when Node no longer requires this
 * module.
 *
 * @param target The object representing the module.
 */
void MapCache::Destroy() {
  if (thread_mutex) {
    apr_thread_mutex_destroy(thread_mutex);
    thread_mutex = NULL;
  }
  apr_pool_destroy(global_pool);
  global_pool = NULL;
}

/**
 * @details This is called by `GetRequestAsync` and runs in a
 * different thread to that function.
 *
 * @param req The asynchronous libuv request.
 */
void MapCache::GetRequestWork(uv_work_t *req) {
  /* No HandleScope! This is run in a separate thread: *No* contact
     should be made with the Node/V8 world here. */

  RequestBaton *baton =  static_cast<RequestBaton*>(req->data);
  mapcache_context *ctx;
  apr_table_t *params;
  mapcache_request *request = NULL;
  mapcache_http_response *http_response = NULL;

  // set up the local context
  ctx = (mapcache_context *)CreateRequestContext(baton->pool, baton->cache, baton->async_log);
  if (!ctx) {
    baton->error = "Could not create the request context";
    return;
  }

  // point the context to our cache configuration
  ctx->config = baton->cache->config->cfg;

#ifdef DEBUG
  ctx->log(ctx, MAPCACHE_DEBUG, (char *) "cache request: %s%s%s%s",
           baton->baseUrl.c_str(),
           baton->pathInfo.c_str(),
           ((baton->queryString.empty()) ? "" : "?"),
           baton->queryString.c_str());
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

/**
 * @details This is set by `GetRequestAsync` to run after
 * `GetRequestWork` has finished, being passed the response generated
 * by the latter and running in the same thread as the former. It
 * formats the cache response into javascript datatypes and returns
 * them via the original callback.
 *
 * @param req The asynchronous libuv request.
 */
void MapCache::GetRequestAfter(uv_work_t *req) {
  HandleScope scope;

  RequestBaton *baton = static_cast<RequestBaton*>(req->data);
  MapCache *cache = baton->cache;
  mapcache_http_response *response = baton->response;

  Handle<Value> argv[2];

  if (baton->async_log) baton->async_log->close(); // finish with the logger

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

    // set the response data as a Node Buffer object. This is zero-copied from
    // mapcache and free'd when the buffer is garbage collected.
    if (response->data) {
      result->Set(data_symbol,
                  Buffer::New((char *)response->data->buf, response->data->size, FreeBuffer, (void *)baton->pool)->handle_);
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
  delete baton;
  return;
}

/**
 * @details This is called by `FromConfigFileAsync` and runs in a
 * different thread to that function.
 *
 * @param req The asynchronous libuv request.
 */
void MapCache::FromConfigFileWork(uv_work_t *req) {
  /* No HandleScope! This is run in a separate thread: *No* contact
     should be made with the Node/V8 world here. */

  ConfigBaton *baton = static_cast<ConfigBaton*>(req->data);
  config_context *config = baton->config;

  // create the context for loading the configuration
  mapcache_context *ctx = (mapcache_context*) CreateRequestContext(config->pool, baton->cache, baton->async_log);
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

/**
 * @details This is set by `FromConfigFileAsync` to run after
 * `FromConfigFileWork` has finished, being passed the response
 * generated by the latter and running in the same thread as the
 * former. It creates a `MapCache` instance and returns it via the
 * original callback.
 *
 * @param req The asynchronous libuv request.
 */
void MapCache::FromConfigFileAfter(uv_work_t *req) {
  HandleScope scope;

  ConfigBaton *baton = static_cast<ConfigBaton*>(req->data);
  Handle<Value> argv[2];

  if (baton->async_log) baton->async_log->close(); // finish with the logger

  if (!baton->error.empty()) {
    apr_pool_destroy(baton->config->pool); // free the memory
    argv[0] = Exception::Error(String::New(baton->error.c_str()));
    argv[1] = Undefined();
  } else {
    Local<Value> config = External::New(baton->config);
    Persistent<Object> cache;
    if (!baton->logger.IsEmpty()) {
      Handle<Value> args[2] = {
        config, // the configuration
        baton->logger // the log emitter
      };
      cache = Persistent<Object>(mapcache_template->GetFunction()->NewInstance(2, args));
    } else {
      cache = Persistent<Object>(mapcache_template->GetFunction()->NewInstance(1, &config));
    }

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
  baton->logger.Dispose();
  baton->callback.Dispose();
  delete baton;
  return;
}

/**
 * @details Nothing can be done before a configuration is created so
 * this method is also used to initialise the global memory pool and
 * thread mutex if that has not already happened.
 */
MapCache::config_context* MapCache::CreateConfigContext() {
  // create the pool for this configuration context
  apr_pool_t *pool = NULL;
  if (apr_pool_create(&pool, global_pool) != APR_SUCCESS) {
    return NULL; // Could not create the cache configuration memory pool
  }

  config_context *ctx = (config_context *) apr_pcalloc(pool, sizeof(config_context));
  if (!ctx) {
    return NULL;
  }
  ctx->pool = pool;
  ctx->cfg = mapcache_configuration_create(pool);
  if (ctx->cfg == NULL) {
    apr_pool_destroy(pool);
    return NULL;
  }

  return ctx;
}

/**
 * @details This creates a context that is used for a interactions with various
 * mapcache core library functions such as those that create configurations and
 * request cache resources.
 *
 * @param pool The memory pool to be used for the context.
 *
 * @param cache The optional `MapCache` instance initiating the request.
 *
 * @param async_log The optional `AsyncLog` instance used to capture log events.
 */
MapCache::request_context* MapCache::CreateRequestContext(apr_pool_t *pool, MapCache *cache, AsyncLog *async_log) {
  if (!pool) {
    return NULL;
  }

  // ensure the thread mutex is initialised
  if (thread_mutex == NULL && apr_thread_mutex_create(&thread_mutex, APR_THREAD_MUTEX_DEFAULT, global_pool) != APR_SUCCESS) {
    return NULL;         // Could not create the mapcache thread mutex
  }

  request_context *rctx;
  mapcache_context *ctx = (mapcache_context *)apr_pcalloc(pool, sizeof(request_context));
  if(!ctx) {
    return NULL;
  }

  ctx->pool = pool;
  ctx->process_pool = global_pool; // the global pool *is* a per process pool
  ctx->threadlock = thread_mutex;
  mapcache_context_init(ctx);
  ctx->log = (async_log) ? async_log->LogRequestContext : NullLogRequestContext;
  ctx->clone = CloneRequestContext;
  ctx->config = NULL;

  rctx = (request_context *)ctx;
  rctx->cache = cache;
  rctx->async_log = async_log;
  return rctx;
}

/**
 * @details This is set as a function pointer to the context created
 * by `CreateRequestContext`. It is called by the wrapped mapcache
 * library when required.
 *
 * @param ctxt The mapcache context to clone.
 */
mapcache_context* MapCache::CloneRequestContext(mapcache_context *ctx) {
  mapcache_context *newctx = (mapcache_context*)apr_pcalloc(ctx->pool,
                                                            sizeof(request_context));
  request_context *rctx = (request_context *) newctx;
  mapcache_context_copy(ctx,newctx);
  if (apr_pool_create(&newctx->pool,ctx->pool) != APR_SUCCESS) {
    return NULL;
  }
  rctx->cache = ((request_context *) ctx)->cache;
  rctx->async_log = ((request_context *) ctx)->async_log;
  return newctx;
}
