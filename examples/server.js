/**
 * Set up MapCache as a Tile Caching HTTP server
 *
 * This provides an example of how to use the MapCache module in
 * combination with the Node HTTP module to create a tile caching
 * server.
 */

var path = require('path');     // for file path manipulations
var http = require('http');     // for the http server
var url = require('url');       // for url parsing

var mapcache = require('mapcache'); // the MapCache module

var port = 3000; // which port will the server run on?
var baseUrl = "http://localhost:" + port; // what is the server url?
var conffile = path.join(__dirname, 'mapcache.xml'); // the location of the config file

// Instantiate a MapCache cache object from the configuration file
mapcache.MapCache.FromConfigFile(conffile, function handleCache(err, cache) {
    if (err) {
        throw err;              // error loading the configuration file
    }

    // fire up a http server, handling all requests
    http.createServer(function handleCacheRequest(req, res) {
        var urlParts = url.parse(req.url); // parse the request url
        var pathInfo = urlParts.pathname || "/"; // generate the PATH_INFO
        var params = urlParts.query || '';       // generate the QUERY_STRING

        // delegate the request to the MapCache cache object, handling the response
        cache.get(baseUrl, pathInfo, params, function handleCacheResponse(err, cacheResponse) {
            console.log('Serving ' + req.url);

            if (err) {
                // the cache returned an error: handle it
                res.writeHead(500);
                res.end(err.stack);
                console.error(err.stack);
                return;
            }

            // send the cache response to the client
            res.writeHead(cacheResponse.code, cacheResponse.headers);
            if (req.method !== 'HEAD') {
                res.end(cacheResponse.data);
            } else {
                res.end();
            }
        });
    }).listen(port, "localhost");

    console.log(
        "Server running at " + baseUrl + " - try the following WMS request:\n" +
            baseUrl + '?LAYERS=test&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=800&HEIGHT=400'
    );
});
