Node.js MapCache Tile Caching Module
====================================

This module provides a map tile caching solution for Node.js using
Mapserver Mapcache (see http://www.mapserver.org/trunk/mapcache).

Mapserver MapCache is already deployable as an Apache module and
CGI/FastCGI binary. This package makes accessing the MapCache
functionality possible from Node with the following advantages:

* **Flexibility**: it makes it easy to add tile caching functionality to
  existing Node projects.
* **Control**: full control over http headers and status codes is
  available allowing fine tuning of things like HTTP caching. URL end
  points can be defined as required.
* **Speed**: The module provides thin C++ bindings to the underlying
  `libmapcache` C library so performance should be better than the
  CGI/FastCGI implementation and on par with the Apache module.
* **Robustness**: The module has a suite of tests that attempts to
  exercise the whole API.

However, the module also comes with the following caveats:

* It is immature and as such has not been widely tested, so far only
  having been used in a Linux environment.
* It requires a development version of MapCache to build.
* C/C++ is not the author's forte and as such there *will* be
  improvements to be made in the code.

Example
-------

This provides an example of how to use the MapCache module in
combination with the Node HTTP module to create a tile caching
server. It is available in the module as `examples/server.js`.

```javascript
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
            baseUrl + '?LAYERS=test&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&' +
            'STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg' +
            '&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=800&HEIGHT=400'
    );
});
```

Another example is provided as `examples/display.js` which pipes the
output of a cache request to the ImageMagick `display` program.

Requirements
------------

* Linux OS (although it should work on other Unices)
* Node.js 0.4 or 0.6
* MapCache svn revision >= 13003 (although it may work with earlier
  versions)

Installation
------------

* Ensure you have installed Mapserver MapCache.
* Tell node-mapcache where `libmapcache` resides on the
  system. Assuming it has been installed in `/usr/local/lib` you would
  use the following command:

    `npm config set mapcache:lib_dir /usr/local/lib`

* Tell node-mapcache where to find the MapCache include files. These
  are not installed in the system by MapCache so a valid mapcache
  build directory must be provided. Assuming you built it in
  `/tmp/mapserver-svn/mapcache` you would use the following command:

    `npm config set mapcache:build_dir /tmp/mapserver-svn/mapcache`

* Optionally tell node-mapcache to be built in debug mode:

    `npm config set mapcache:debug true`

* Get and install node-mapcache: 

    `npm install mapcache`

* Optionally test that everything is working as expected
  (recommended):

   `npm test mapcache`

Contact
-------

Homme Zwaagstra <hrz@geodata.soton.ac.uk>
