/******************************************************************************
 * Copyright (c) 2012, GeoData Institute (www.geodata.soton.ac.uk)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

/**
 * Set up MapCache as a Tile Caching HTTP server
 *
 * This provides an example of how to use the MapCache module in
 * combination with the Node HTTP module to create a tile caching
 * server.
 */

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
