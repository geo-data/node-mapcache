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

    AtExit(Cleanup);
  }
}

NODE_MODULE(bindings, init)
