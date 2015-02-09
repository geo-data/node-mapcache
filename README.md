# Node.js MapCache Tile Caching Module

[![Build Status](https://secure.travis-ci.org/geo-data/node-mapcache.png)](http://travis-ci.org/geo-data/node-mapcache)

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
  API. The tests provide 88% line coverage and 91% function coverage; excluded
  code generally handles hard to replicate edge cases (e.g. memory
  exhaustion). This suite has been run through Valgrind to check for memory
  leaks.

## Usage

The `node-mapcache` API is designed to be simple but flexible.  It binds into
the underlying `libmapcache` library before the HTTP layer and around the
concept of a `MapCache` object.  A `MapCache` instance is derived from a
standard mapcache configuration file.  It is used to make requests to the
underlying tile cache and return the response:

```javascript
var mapcache = require('mapcache'), // node-mapcache
    events = require('events'),     // for the logger
    logger = new events.EventEmitter(),
    fs = require('fs');         // for filesystem operations

// Handle log messages
logger.on('log', function handleLog(logLevel, logMessage) {
    if (logLevel >= mapcache.logLevels.WARN) {
        console.error('OOPS! (%d): %s', logLevel, logMessage);
    } else {
        console.log('LOG (%d): %s', logLevel, logMessage);
    }
});

// Instantiate a MapCache cache object from the configuration file
mapcache.MapCache.FromConfigFile('mapcache.xml', logger, function handleCache(err, cache) {
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

The logger passed to `FromConfigFile` above is optional: the method accepts
just two arguments as well.  If a logger is passed in it is used for the life
of the `MapCache` instance.  The available log levels are the same as those in
the MapCache configuration file.  From the Node REPL:

```
> var mapcache = require('mapcache');
> mapcache.logLevels
{ DEBUG: 0,
  INFO: 1,
  NOTICE: 2,
  WARN: 3,
  ERROR: 4,
  CRIT: 5,
  ALERT: 6,
  EMERG: 7 }
```

Versioning information is also available:

```
> var mapcache = require('mapcache');
> mapcache.versions
{ node_mapcache: '0.1.5',
  mapcache: '0.5-dev',
  apr: '1.4.5' }
```

### Example

This provides an example of how to use the MapCache module in combination with
the Node HTTP module to create a tile caching server. It is available in the
package as `examples/server.js`.

```javascript
var path = require('path'),         // for file path manipulations
    http = require('http'),         // for the http server
    url = require('url'),           // for url parsing
    events = require('events'),     // for the logger
    logger = new events.EventEmitter(),
    mapcache = require('mapcache'), // the MapCache module
    port = 3000,                    // which port will the server run on?
    baseUrl = "http://localhost:" + port,         // what is the server url?
    conffile = path.join(__dirname, 'mapcache.xml'); // the location of the config file

// Handle log messages
logger.on('log', function handleLog(logLevel, logMessage) {
    if (logLevel >= mapcache.logLevels.WARN) {
        console.error('OOPS! (%d): %s', logLevel, logMessage);
    } else {
        console.log('LOG (%d): %s', logLevel, logMessage);
    }
});

// Instantiate a MapCache cache object from the configuration file
mapcache.MapCache.FromConfigFile(conffile, logger, function handleCache(err, cache) {
    if (err) {
        throw err;              // error loading the configuration file
    }

    // fire up a http server, handling all requests
    http.createServer(function handleCacheRequest(req, res) {
        var urlParts = url.parse(decodeURIComponent(req.url)); // parse the request url
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
```

Another example is provided as `examples/display.js` which pipes the output of
a cache request to the ImageMagick `display` program.

## Requirements

* Linux OS (although it should work on other Unices and ports to Windows and
  other platforms supported by both Node and Mapserver should be possible:
  patches welcome!)
  
* Node.js >=0.10

* Mapserver MapCache 0.5-dev >= commit 11e8509

## Installation

### Using NPM

The latest stable release of Node Mapcache is available via the Node
Package Manager:

* Ensure [Mapserver Mapcache](http://www.mapserver.org/trunk/mapcache) is
  installed.  It should be built from source as we need the build directory in
  the next step:

* Point node-mapcache to the MapCache build directory. These are not installed
  in the system by MapCache so a valid mapcache build directory must be
  provided. Assuming you built it in `/tmp/mapcache` you would use the
  following command:

    `npm config set mapcache:build_dir /tmp/mapcache`

* Get and install node-mapcache:

    `npm install mapcache`

* Optionally test that everything is working as expected (recommended):

   `npm test mapcache`

### Using Docker

Assuming you have [Docker](http://www.docker.io/) available on your
system, the following command will obtain a docker image with the
latest Node Mapcache code from git built against the latest Mapserver
Mapcache git checkout:

    docker pull homme/node-mapcache:latest

Note that the latest Mapcache git checkout is the latest **at the time
the image was built**.  If you want the absolute latest code of both
Node Mapcache *and* Mapcache, build the docker image yourself locally
along these lines:

    docker build -t homme/node-mapcache:latest https://raw.github.com/geo-data/node-mapcache/master/docker/latest/Dockerfile

By default the image runs the Node Mapcache tests:

    docker run homme/node-mapcache:latest

Running Node directly from the image allows you to `require()` and
play around with Node Mapcache interactively:

    docker run -t -i homme/node-mapcache:latest /usr/bin/node
    > var mapcache = require('mapcache');
    undefined
    > mapcache.versions
    { node_mapcache: '0.1.12',
      mapcache: '1.3dev',
      apr: '1.4.6' }

See the [Docker Index](https://index.docker.io/u/homme/node-mapcache/)
for further information.

## Recommendations

* If you want raw speed use the Apache Mapcache module or reverse proxy your
  `node-mapcache` app with a web accelerator such as Varnish.  Having said that
  `node-mapcache` shouldn't be slow: benchmarks are welcome!

* When using the Berkeley DB (BDB) as a cache backend for multiple tilesets
  stability is improved by having a separate filesystem directory for each
  tileset: this seems to be because the BDB system files (e.g. `__db.001`) are
  created on a per directory basis and organising them discretely prevents them
  having to manage more than one BDB database.

* Check out [`node-mapserv`](https://npmjs.org/package/mapserv): this can work
  well in combination with `node-mapcache` for generating tiled maps.

## Contributing

Fork the code on GitHub or clone it:

    git clone https://github.com/geo-data/node-mapcache.git
    cd node-mapcache

Build the module in Debug mode using:

    make build

By default this uses the mapcache build directory previously specified using
`npm config set mapcache:build_dir`; to override this do something along the
following lines:

    make build npm_config_mapcache_build_dir=/tmp/mapcache

You may want to ensure you're building in a clean source tree in which case:

    make clean

Add tests for your changes to `test/mapcache-test.js` and run them:

    make test

Perform code coverage analysis to ensure all code paths in your changes are
tested (this requires [`lcov`](http://ltp.sourceforge.net/coverage/lcov.php) be
installed):

    make cover

Finally run the test suite through `valgrind` to ensure you haven't introduced
any memory issues:

    make valgrind

And issue your pull request or patch...

### Documentation

Doxygen based documentation is available for the C++ bindings:

    make doc

## Bugs

Please add bugs or issues to the
[GitHub issue tracker](https://github.com/geo-data/node-mapcache).

## Licence

[BSD 2-Clause](http://opensource.org/licenses/BSD-2-Clause).

## Contact

Homme Zwaagstra <hrz@geodata.soton.ac.uk>
