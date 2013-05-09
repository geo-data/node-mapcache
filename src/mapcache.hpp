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

#ifndef __NODE_MAPCACHE_H__
#define __NODE_MAPCACHE_H__

/**
 * @file mapcache.hpp
 * @brief This declares the primary `MapCache` class.
 */

/// The node-mapcache version string
#define NODE_MAPCACHE_VERSION "0.1.10"

// Standard headers
#include <string>
#include <queue>

// Node headers
#include <v8.h>
#include <node.h>
#include <node_buffer.h>

// Apache headers
#include <apr_strings.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_date.h>
#include <apr_thread_mutex.h>

// MapCache headers
extern "C" {
#include "mapcache.h"
}

/// Define a permanent, read only javascript constant
#define NODE_MAPCACHE_CONSTANT(TARGET, NAME, CONSTANT)                  \
  (TARGET)->Set(String::NewSymbol(#NAME),                               \
                Integer::New(CONSTANT),                                 \
                static_cast<PropertyAttribute>((ReadOnly|DontDelete)))

/// Throw an exception generated from a `char` string
#define THROW_CSTR_ERROR(TYPE, STR)                             \
  return ThrowException(Exception::TYPE(String::New(STR)));

/// Assign to a local V8 `Function` variable from the function arguments
#define ASSIGN_FUN_ARG(I, VAR)                              \
  if (args.Length() <= (I) || !args[I]->IsFunction())       \
    THROW_CSTR_ERROR(TypeError,                             \
                     "Argument " #I " must be a function"); \
  VAR = Local<Function>::Cast(args[I]);

/// Create a local V8 `Function` variable from the function arguments
#define REQ_FUN_ARG(I, VAR) \
  Local<Function> VAR;      \
  ASSIGN_FUN_ARG(I, VAR);

/// Create an `UTF8` V8 string variable from the function arguments
#define REQ_STR_ARG(I, VAR)                               \
  if (args.Length() <= (I) || !args[I]->IsString())       \
    THROW_CSTR_ERROR(TypeError,                           \
                     "Argument " #I " must be a string"); \
  String::Utf8Value VAR(args[I]->ToString());

/// Create a local V8 `External` variable from the function arguments
#define REQ_EXT_ARG(I, VAR)                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())   \
    THROW_CSTR_ERROR(TypeError,                         \
                     "Argument " #I " invalid");        \
  Local<External> VAR = Local<External>::Cast(args[I]);

/// Assign to a local V8 `Object` variable from the function arguments
#define ASSIGN_OBJ_ARG(I, VAR)                             \
  if (args.Length() <= (I) || !args[I]->IsObject())        \
    THROW_CSTR_ERROR(TypeError,                            \
                     "Argument " #I " must be an object"); \
  VAR = Local<Object>(args[I]->ToObject());

/// Create a local V8 `Object` variable from the function arguments
#define REQ_OBJ_ARG(I, VAR) \
  Local<Object> VAR;        \
  ASSIGN_OBJ_ARG(I, VAR);

using namespace node;
using namespace v8;

class AsyncLog;                 // forward declaration

/**
 * @brief The primary class in the module representing a map cache
 *
 * This class serves as a wrapper to the functionality provided by the
 * underlying `mapcache` C code. Individual instances represent
 * independent mapcache configurations and are used to return cache
 * resources.
 */
class MapCache: ObjectWrap {
  friend class AsyncLog;        // needs access to protected methods
public:

  /// Initialise the class
  static void Init(Handle<Object> target);

  /// Instantiate a `MapCache` instance from a configuration file
  static Handle<Value> FromConfigFileAsync(const Arguments& args);

  /// Request a resource from the cache
  static Handle<Value> GetAsync(const Arguments& args);

  /// Free up the class memory
  static void Destroy();

protected:

  /// The mapcache request context
  struct request_context {
    mapcache_context ctx;
    MapCache *cache;
    AsyncLog *async_log;
  };

private:

  /// The function template for creating new `MapCache` instances.
  static Persistent<FunctionTemplate> mapcache_template;

  /// The string "code"
  static Persistent<String> code_symbol;
  /// The string "data"
  static Persistent<String> data_symbol;
  /// The string "mtime"
  static Persistent<String> mtime_symbol;
  /// The string "headers"
  static Persistent<String> headers_symbol;

  /// The per-process cache memory pool
  static apr_pool_t *global_pool;

  /// The per-process thread mutex
  static apr_thread_mutex_t *thread_mutex;

  /// An association of a mapcache configuration and memory pool
  struct config_context {
    mapcache_cfg *cfg;
    apr_pool_t *pool;
  };

  /// The unique per-instance cache context
  config_context *config;

  /// A handle to an optional `EventEmitter` used for logging
  Persistent<Object> logger;

  /// The structure used when performing asynchronous operations
  struct Baton {
    /// The asynchronous request
    uv_work_t request;
    /// The cache from which the request originated
    MapCache *cache;
    /// The optional logger
    AsyncLog *async_log;
    /// The function executed upon request completion
    Persistent<Function> callback;
    /// A message set when the request fails
    std::string error;
   };

  /// A Baton specifically used for cache requests
  struct RequestBaton : Baton {
    /// The memory pool unique to this request
    apr_pool_t* pool;
    /// The base URL of the cache request
    std::string baseUrl;
    /// The `PATH_INFO` data for the cache request
    std::string pathInfo;
    /// The `QUERY_STRING` data for the cache request
    std::string queryString;
    /// The mapcache response to the request
    mapcache_http_response *response;
  };

  /// A Baton specifically used when instantiating from a config file
  struct ConfigBaton : Baton {
    /// The file path representing the configuration file
    std::string conffile;
    /// The cache context created from the configuration file
    config_context *config;
    /// The optional `EventEmitter` used for logging
    Persistent<Object> logger;
  };

  /// Intantiate a mapcache with a configuration context and optional logger
  MapCache(config_context *config, Local<Object> logger) :
    config(config)
  {
    // should throw an error here if !config
    if (!logger.IsEmpty())
      this->logger = Persistent<Object>::New(logger);
  }

  /// Clear up the configuration context
  ~MapCache() {
    if (config && config->pool) {
      apr_pool_destroy(config->pool);
      config = NULL;
    }
    logger.Dispose();
  }

  /// Instantiate an object
  static Handle<Value> New(const Arguments& args);

  /// Perform a query of the cache
  static void GetRequestWork(uv_work_t *req);

  /// Return the cache response to the caller
  static void GetRequestAfter(uv_work_t *req);

  /// Create a mapcache configuration context from a file path
  static void FromConfigFileWork(uv_work_t *req);

  /// Return the cache response to the caller
  static void FromConfigFileAfter(uv_work_t *req);

  /// Create a new mapcache configuration context.
  static config_context* CreateConfigContext();

  /// Create a new mapcache request context
  static request_context* CreateRequestContext(apr_pool_t *pool, MapCache *cache, AsyncLog *async_log);

  /// Clone a mapcache request context
  static mapcache_context* CloneRequestContext(mapcache_context *ctx);

  /// A logging callback that does nothing - used when there is no logger
  static void NullLogRequestContext(mapcache_context *c, mapcache_log_level level, char *message, ...) {
    return;
  }
};

/**
 * @brief The asynchronous log handler
 *
 * This is an helper class that is composed of a Node `EventEmitter` instance
 * and a callback that is assigned to the mapcache core library.  When the
 * callback is triggered by the mapcache core this class handles the log
 * message, calling the `EventEmitter.emit()` method with the log level and log
 * message.
 *
 * Most interactions with the core mapcache code are asynchronous.  However,
 * the mapcache core generates log messages which need to be made available to
 * javascript code, and these messages are generated in the asynchronous
 * threads.  `libuv` provides the `uv_async_t` mechanism to deal with these
 * events: this class is effectively a wrapper around that functionality.
 *
 * This code is inspired by the node-sqlite3 `Async` class at <https://github.com/developmentseed/node-sqlite3/blob/master/src/async.h>.
 */
class AsyncLog {
  friend class MapCache;     // needs access to the `MapCache->request_context`
protected:

  /**
   * @brief Finish with the logger
   *
   * This flushes any remaining messages that haven't yet been handled.  It
   * should *only* be called from the Node/V8 thread and the logger should
   * *not* be referenced after calling it as this deletes it.
   */
  void close() {
    // Need to call `EmitLogs` again to ensure all items have been
    // processed. Is this a bug in uv_async? Feels like uv_close should handle
    // that.
    EmitLogs(&async, 0);
    uv_close((uv_handle_t*) &async, Destroy);
  }
  
  /// Log information from a request context
  static void LogRequestContext(mapcache_context *c, mapcache_log_level level, char *message, ...);
  
private:

  /// The handle on the `EventEmitter` object
  Persistent<Object> emitter;

  /// A handle on the `emit` function from `emitter`
  Persistent<Function> callback;

  /// The asynchronous libuv data structure
  uv_async_t async;

  /// The data mutex, required as `async->data` is thread sensitive
  uv_mutex_t mutex;

  /// A combination of a message string and log level
  struct Log {
    std::string message;
    mapcache_log_level level;
  };

  /// The queue of messages
  std::queue<Log *> message_queue;

  /// Intantiate an `AsyncLog` with an `EventEmitter`
  AsyncLog(Persistent<Object> emitter);

  /// Clear up the mutex and javascript handles
  ~AsyncLog() {
    assert(message_queue.empty());
    callback.Dispose();
    emitter.Dispose();
    uv_mutex_destroy(&mutex);
  }

  /// Flush the queue of messages
  static void EmitLogs(uv_async_t *handle, int status /*UNUSED*/);

  /// Destroy the `async->data` structure upon `uv_close()`
  static void Destroy(uv_handle_t* handle) {
    assert(handle != NULL);
    assert(handle->data != NULL);
    AsyncLog* self = static_cast<AsyncLog*>(handle->data);
    delete self;
  }
};

/**
 * @def NODE_MAPCACHE_CONSTANT(TARGET, NAME, CONSTANT)
 *
 * This is the same as the `NODE_DEFINE_CONSTANT` macro with the ability to
 * specify an alternative name for the constant.
 
 * @def REQ_STR_ARG(I, VAR)
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

 * @def ASSIGN_FUN_ARG(I, VAR)
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

 * @def REQ_FUN_ARG(I, VAR)
 *
 * This defines a `Local<Function>` variable and then delegates to the
 * `ASSIGN_FUN_ARG` macro.

 * @def REQ_EXT_ARG(I, VAR)
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

 * @def ASSIGN_OBJ_ARG(I, VAR)
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

 * @def REQ_OBJ_ARG(I, VAR)
 *
 * This defines a `Local<Object>` variable and then delegates to the
 * `ASSIGN_OBJ_ARG` macro.

 * @def THROW_CSTR_ERROR(TYPE, STR)
 *
 * This returns from the containing function throwing an error of a
 * specific type.
 *
 * @param TYPE The symbol name of the exception to be thrown.
 * @param STR The `char` string to set as the error message.

 * @struct MapCache::config_context
 *
 * This represents the key underlying mapcache data structures which
 * are wrapped by the `MapCache` class.

 * @struct MapCache::request_context
 *
 * This wraps a `mapcache_context` struct and is used when requesting cached
 * resources.  It is protected as it is accessed by
 * `AsyncLog::LogRequestContext()`.

 * @struct MapCache::Baton
 *
 * This represents a standard interface used to transfer data
 * structures between threads when using libuv asynchronously. See
 * <http://kkaefer.github.com/node-cpp-modules> for details.
 */

#endif  /* __NODE_MAPCACHE_H__ */
