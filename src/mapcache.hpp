#ifndef __NODE_MAPCACHE_H__
#define __NODE_MAPCACHE_H__

/**
 * @file mapcache.hpp
 * @brief This declares the primary `MapCache` class.
 */

// Standard headers
#include <string>

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

using namespace node;
using namespace v8;

#ifdef DEBUG
#include <iostream>
using std::cout; using std::endl;
#endif

/* The following defines were adapted from the `node-mapserver`
   project. */

/// Create an `UTF8` V8 string variable from the function arguments
 #define REQ_STR_ARG(I, VAR)                              \
  if (args.Length() <= (I) || !args[I]->IsString())      \
    return ThrowException(Exception::TypeError(          \
      String::New("Argument " #I " must be a string"))); \
  String::Utf8Value VAR(args[I]->ToString());

/// Create a local V8 `Function` variable from the function arguments
#define REQ_FUN_ARG(I, VAR)                                \
  if (args.Length() <= (I) || !args[I]->IsFunction())      \
    return ThrowException(Exception::TypeError(            \
      String::New("Argument " #I " must be a function"))); \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

/// Create a local V8 `External` variable from the function arguments
#define REQ_EXT_ARG(I, VAR)                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())   \
    return ThrowException(Exception::TypeError(         \
      String::New("Argument " #I " invalid")));         \
  Local<External> VAR = Local<External>::Cast(args[I]);

/// Throwing an exception generated from a `char` string
#define THROW_CSTR_ERROR(TYPE, STR)                         \
  return ThrowException(Exception::TYPE(String::New(STR)));

/**
 * @brief The primary class in the module representing a map cache
 *
 * This class serves as a wrapper to the functionality provided by the
 * underlying `mapcache` C code. Individual instances represent
 * independent mapcache configurations and are used to return cache
 * resources.
 */
class MapCache: ObjectWrap {
public:

  /// Initialise the class
  static void Init(Handle<Object> target);

  /// Instantiate a `MapCache` instance from a configuration file
  static Handle<Value> FromConfigFileAsync(const Arguments& args);

  /// Request a resource from the cache
  static Handle<Value> GetAsync(const Arguments& args);

  /// Free up the class memory
  static void Destroy();

private:

  /// The function template for creating new `MapCache` instances.
  static Persistent<FunctionTemplate> constructor_template;

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

  /// The mapcache request context
  struct request_context {
    mapcache_context ctx;
  };

  /// The structure used when performing asynchronous operations
  struct Baton {
    /// The asynchronous request
    uv_work_t request;
    /// The function executed upon request completion
    Persistent<Function> callback;
    /// An message set when the request fails
    std::string error;
  };

  /// A Baton specifically used for cache requests
  struct RequestBaton : Baton {
    /// The cache from which the request originated
    MapCache *cache;
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
  };

  /// Intantiate a mapcache with a configuration context
  MapCache(config_context *config) :
    config(config)
  {
    // should throw an error here if !config
#ifdef DEBUG
    cout << "Instantiating MapCache instance" << endl;
#endif
  }

  /// Clear up the configuration context
  ~MapCache() {
#ifdef DEBUG
    cout << "Destroying MapCache instance" << endl;
#endif
    if (config && config->pool) {
      apr_pool_destroy(config->pool);
      config = NULL;
    }
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
  static request_context* CreateRequestContext(apr_pool_t *pool);

  /// Clone a mapcache request context
  static mapcache_context* CloneRequestContext(mapcache_context *ctx);

  /// Log information from a request context
  static void LogRequestContext(mapcache_context *c, mapcache_log_level level, char *message, ...);

};

/**
 * @def REQ_STR_ARG(I, VAR)
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

 * @def REQ_FUN_ARG(I, VAR) 
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

 * @def REQ_EXT_ARG(I, VAR)
 *
 * This throws a `TypeError` if the argument is of the wrong type.
 *
 * @param I A zero indexed integer representing the variable to
 * extract in the `args` array.
 * @param VAR The symbol name of the variable to be created.

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
 * This wraps a `mapcache_context` struct and is used when requesting
 * cached resources.

 * @struct MapCache::Baton
 *
 * This represents a standard interface used to transfer data
 * structures between threads when using libuv asynchronously. See
 * <http://kkaefer.github.com/node-cpp-modules> for details.
 */

#endif  /* __NODE_MAPCACHE_H__ */
