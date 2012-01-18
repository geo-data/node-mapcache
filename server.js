/**
 * Set up Mapcache as a server
 *
 * This can be tested after running it using:
 *
 * curl 'http://localhost:3000/?LAYERS=test&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=800&HEIGHT=400'
 */

var path = require('path');
var http = require('http');
var mapcache = require('./mapcache');

// Callback that receives a cache after it is loaded and uses it to
// create a HTTP request handler
function handleCache(err, cache) {
    if (err) {
        throw err;
    }

    var httpCacheHandler = mapcache.httpCacheHandler(cache);
    http.createServer(httpCacheHandler).listen(3000, "localhost");
    console.log('Server running at http://localhost:3000/');
}

// load the cache from the configuration file and handle the resulting object
var conffile = path.resolve('./mapcache.xml');
mapcache.MapCache.FromConfigFile(conffile, handleCache);
