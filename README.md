Node.js MapCache Tile Caching Module
====================================

This module provides a map tile caching solution for Node.js by creating
bindings to [Mapserver Mapcache](http://www.mapserver.org/trunk/mapcache).

Mapserver MapCache is already deployable as an Apache/Ngnix module and
CGI/FastCGI binary. This package makes accessing the MapCache functionality
possible from Node with the following advantages:

* **Flexibility**: it makes it easy to add tile caching functionality to
  existing Node projects and isn't automatically bound to an HTTP environment.
* **Control**: full control over http headers and status codes is available
  allowing fine tuning of things like HTTP caching. URL end points can be
  defined as required.
* **Robustness**: The module has a suite of tests that exercises the whole
  API. This suite has been run through Valgrind to check for memory leaks.

However, the module also comes with the following caveats:

* Although it has been used intensively on a number of internal projects the
  codebase is relatively immature.  To date it has only been developed and used
  in a Linux environment: patches porting it to other OSes are welcome!
* If you want raw speed use the Apache module or reverse proxy your
  `node-mapcache` app with a web accelerator such as Varnish.  Having said that
  `node-mapcache` shouldn't be slow: benchmarks are welcome!
* C/C++ is not the author's day-to-day programming language: code improvements
  are welcome!

API
---

The `node-mapcache` API is designed to be simple but flexible.  It binds into
the underlying `libmapcache` library before the HTTP layer and revolves around
the concept of a `MapCache` object.  A `MapCache` is derived from a mapcache
configuration file and is used to make requests to the underlying tile cache
and return a response:

```javascript
var mapcache = require('mapcache'), // node-mapcache
    fs = require('fs');         // for filesystem operations

// Instantiate a MapCache cache object from the configuration file
mapcache.MapCache.FromConfigFile('mapcache.xml', function handleCache(err, cache) {
    if (err) {
        throw err;
    }

    // The parameters determining the cache response
    var baseUrl = 'http://tiles.example.com', // an arbitrary base URL
        pathInfo = 'tms/1.0.0/test@WGS84/0/0/0.png', // a TMS cache request
        queryString = '';     // no query string for a TMS request

    // delegate the request to the MapCache cache object, handling the response
    cache.get(baseUrl, pathInfo, queryString, function handleCacheResponse(err, cacheResponse) {
        if (err) {
            throw err;
        }

        // If the response is an image, save it to a file, otherwise write it
        // to standard output.
        var contentType = cacheResponse.headers['Content-Type'][0]; // get the content type from the headers
        if (contentType.substr(0, 5) === 'image') {
            var filename = 'output.' + contentType.substr(6);
            fs.writeFile(filename, cacheResponse.data);
        } else {
            console.log(cacheResponse.data.toString());
        }
    });
});
```

The cache response (`cacheResponse` in the above snippet) is a javascript
object literal with the following properties:

* `code`: an integer representing the HTTP status code
* `mtime`: a date representing the last modified time
* `data`: a `Buffer` object representing the cached data
* `headers`: the HTTP headers as an object literal

Versioning information is also available. From the Node REPL:

```
> var mapcache = require('./mapcache')
> mapcache.versions
{ node_mapcache: '0.1.3',
  mapcache: '0.5-dev',
  apr: '1.4.5' }
```

Example
-------

This provides an example of how to use the MapCache module in combination with
the Node HTTP module to create a tile caching server. It is available in the
package as `examples/server.js`.

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

Another example is provided as `examples/display.js` which pipes the output of
a cache request to the ImageMagick `display` program.

Requirements
------------

* Linux OS (although it should work on other Unices)
* Node.js >= 0.8
* Mapserver MapCache 0.5-dev (although it *may* work with other versions)

Installation
------------

* Ensure [Mapserver Mapcache](http://www.mapserver.org/trunk/mapcache) is
  installed.
* Tell node-mapcache where `libmapcache` resides on the system. Assuming it has
  been installed in `/usr/local/lib` you would use the following command:

    `npm config set mapcache:lib_dir /usr/local/lib`

* Tell node-mapcache where to find the MapCache include files. These are not
  installed in the system by MapCache so a valid mapcache build directory must
  be provided. Assuming you built it in `/tmp/mapcache` you would use the
  following command:

    `npm config set mapcache:build_dir /tmp/mapcache`

* Optionally tell node-mapcache to be built in debug mode:

    `npm config set mapcache:debug true`

* Get and install node-mapcache:

    `npm install mapcache`

* Optionally test that everything is working as expected
  (recommended):

   `npm test mapcache`

Bugs
----

Please add bugs or issues to the
[GitHub issue tracker](https://github.com/geo-data/node-mapcache).

Documentation
-------------

Doxygen based documentation is available for the C++ bindings. See
`doc/README.md` for details.

Licence
-------

BSD 2-Clause (http://opensource.org/licenses/BSD-2-Clause).

Contact
-------

Homme Zwaagstra <hrz@geodata.soton.ac.uk>
