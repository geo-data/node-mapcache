#include "mapcache.hpp"

// Perform housekeeping
static void Cleanup(void* arg) {
  MapCache::Destroy();
}

extern "C" {
  static void init (Handle<Object> target) {
#ifdef DEBUG
    cout << "Initialising MapCache module" << endl;
#endif
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

NODE_MODULE(bindings, init)
