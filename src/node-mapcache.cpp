/**
 * @file node-mapcache.cpp
 * @brief This registers and initialises the module with Node.
 *
 * @mainpage Node %MapCache
 *
 * This represents the C++ bindings that are part of a package
 * providing access to <a
 * href="http://www.mapserver.org/trunk/mapcache/index.html">Mapserver
 * MapCache</a> functionality from <a
 * href="http://nodejs.org">Node.js</a>.
 *
 * MapCache is the primary C++ class which wraps the underlying
 * libmapcache functionality.  A thin layer of javascript is used in
 * the package to expose this class to clients along with some
 * additional features built on it.
 *
 * See the `README.md` file distributed with the package for further
 * details.
 */

#include "mapcache.hpp"

/** Clean up at module exit.
 *
 * This performs housekeeping duties when the module is
 * unloaded. Specifically it frees up static data structures used by
 * the `MapCache` class.
 *
 * The function signature is suitable for using passing it to the
 * `Node::AtExit` function.
 *
 * @param arg Not currently used.
 */
static void Cleanup(void* arg) {
  MapCache::Destroy();
}

/** Initialise the module.
 *
 * This is the entry point to the module called by Node and as such it
 * initialises various module elements:
 *
 * - The APR library used by `libmapcache`.
 * - The `MapCache` class
 * - Versioning information
 *
 * @param target The object representing the module.
 */
extern "C" {
  static void init (Handle<Object> target) {
    apr_initialize();
    atexit(apr_terminate);
    apr_pool_initialize();

    MapCache::Init(target);

    // versioning information
    Local<Object> versions = Object::New();
    versions->Set(String::NewSymbol("mapcache"), String::New(MAPCACHE_VERSION));
    versions->Set(String::NewSymbol("apr"), String::New(APR_VERSION_STRING));
    target->Set(String::NewSymbol("versions"), versions);

    AtExit(Cleanup);
  }
}

/// Register the module
NODE_MODULE(bindings, init)
