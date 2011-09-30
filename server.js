/**
 * Set up GeoCache as a server
 *
 * This can be tested after running it using:
 *
 * curl 'http://localhost:3000/?LAYERS=aerial-photos&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A27700&BBOX=431380.11079545,89627.893687851,432044.11115055,89950.632057879&WIDTH=1398&HEIGHT=679'
 */

var url = require('url');
var geocache = require('./build/default/bindings.node');
var http = require('http');
var path = require('path');

// Build a callback to handle Cache responses
function getCacheResponseHandler(httpRequest, httpResponse) {

    // Handle a response from the cache
    function handleCacheResponse(err, cacheResponse) {
        if (err) {
            httpResponse.writeHead(500);
            httpResponse.end(err.stack);
            console.error(err.stack);
            return;
        }

        var header, i, headers = {};
        for (header in cacheResponse.headers) {
            if (cacheResponse.headers.hasOwnProperty(header)) {
                values = cacheResponse.headers[header];
                headers[header] = values[0]; // get the first header value
            }
        }
        httpResponse.writeHead(cacheResponse.code, headers);
        httpResponse.end(cacheResponse.data);
        console.log('Serving ' + httpRequest.url);
    }

    return handleCacheResponse;
}
// Build a callback to handle HTTP cache requests
function getCacheRequestHandler(cache) {

    // Callback called to handle HTTP requests
    function handleRequest(req, res) {
        var baseUrl = "http://localhost:3000";
        var urlParts = url.parse(req.url);
        var params = urlParts.query || '';
        var pathInfo = urlParts.pathname || "/";
        
        cache.get(baseUrl, pathInfo, params, getCacheResponseHandler(req, res));
    }

    return handleRequest;
}

// Callback that receives a cache after it is loaded and uses it to
// create a HTTP request handler
function handleCache(err, cache) {
    if (err) {
        throw err;
    }

    http.createServer(getCacheRequestHandler(cache)).listen(3000, "localhost");
    console.log('Server running at http://localhost:3000/');
}

// load the cache from the configuration file and handle the resulting object
var conffile = path.resolve('./geocache.xml');
geocache.GeoCache.FromConfigFile(conffile, handleCache);
