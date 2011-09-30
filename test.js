var path = require('path');
var geocache = require('./build/default/bindings.node');

console.log('geocache loaded');
var conffile = path.resolve('./geocache.xml');

// Callback that receives a cache response
function handleCacheResponse(err, response) {
    if (err) {
        throw err;
    }

    console.log(response);      // display the response
}

// Callback that receives a cache after it is loaded
function handleCache(err, cache) {
    if (err) {
        throw err;
    }

    console.log('service created from ' + conffile);

    var base = "http://localhost:8081";
    var pathInfo = "/foo/bar";
    var params = 'LAYERS=aerial-photos&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A27700&BBOX=431380.11079545,89627.893687851,432044.11115055,89950.632057879&WIDTH=1398&HEIGHT=679';

    cache.get(base, pathInfo, params, handleCacheResponse); // query the cache
}

geocache.GeoCache.FromConfigFile(conffile, handleCache); // load the cache from file
