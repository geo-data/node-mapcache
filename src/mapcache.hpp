#ifndef __NODE_MAPCACHE_H__
#define __NODE_MAPCACHE_H__

/* This code is PUBLIC DOMAIN, and is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. See the accompanying
 * LICENSE file.
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

// The MapCache class
class MapCache: ObjectWrap {
public:

  static Persistent<FunctionTemplate> constructor_template;
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> FromConfigFileAsync(const Arguments& args);
  static Handle<Value> GetAsync(const Arguments& args);
  static void Destroy();

private:

  static apr_pool_t *global_pool;
  static apr_thread_mutex_t *thread_mutex;

  /* A association of a mapcache configuration and a memory pool. This
     is the key underlying Mapserver Mapcache data structure that this
     class wraps.*/
  typedef struct config_context {
    mapcache_cfg *cfg;
    apr_pool_t *pool;
  } config_context;

  config_context *config;       /* the cache context */

  typedef struct mapcache_context_node {
    mapcache_context ctx;
  } mapcache_context_node;

  /* The structure used for passing cache request data asynchronously
     between threads using libuv. See
     <http://kkaefer.github.com/node-cpp-modules> for details. */
  struct Baton {
    // standard Baton interface
    uv_work_t request;
    Persistent<Function> callback;
    std::string error;
  };

  // Baton for cache requests
  struct RequestBaton : Baton {
    // application data
    MapCache *cache;
    apr_pool_t* pool;
    std::string baseUrl;
    std::string pathInfo;
    std::string queryString;
    mapcache_http_response *response;
  };

  // Baton for configuration file creation
  struct ConfigBaton : Baton {
    // application data
    config_context *config;
    std::string conffile;
  };

  MapCache(config_context *config) :
    config(config)
  {
    // should throw an error here if !config
#ifdef DEBUG
    cout << "Instantiating MapCache instance" << endl;
#endif
  }

  ~MapCache() {
#ifdef DEBUG
    cout << "Destroying MapCache instance" << endl;
#endif
    apr_pool_destroy(config->pool);
    config = NULL;
  }

  static void GetRequestWork(uv_work_t *req);
  static void GetRequestAfter(uv_work_t *req);
  static void FromConfigFileWork(uv_work_t *req);
  static void FromConfigFileAfter(uv_work_t *req);

  static config_context* CreateConfigContext(apr_pool_t *pool);
  static mapcache_context_node* CreateRequestContext(apr_pool_t *pool);
  static mapcache_context* CloneRequestContext(mapcache_context *ctx);
  static void LogRequestContext(mapcache_context *c, mapcache_log_level level, char *message, ...);

};

#endif  /* __NODE_MAPCACHE_H__ */
