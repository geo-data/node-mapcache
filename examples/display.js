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
 * Display the results of a cache request
 *
 * This script takes an URL representing a cache resource (generally
 * ignoring the hostname and protocol) and displays the result. If the
 * result is an image it gets piped to the ImageMagick `display`
 * program (which must be in the current user's execution path). If
 * the result is anything other than an image it gets printed to the
 * console.
 */

var path = require('path');     // for file path manipulations
var url = require('url');       // for url parsing
var child_process = require('child_process'); // for calling `display`

var mapcache = require('mapcache'); // the MapCache module

var cacheUrl = process.argv[2];
if (cacheUrl === undefined) {
    console.error(
        "usage: node display.js URL\n\ne.g.\n" +
            "node display.js '/?LAYERS=test&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=800&HEIGHT=400'"
    );
    process.exit(1);
}

var conffile = path.join(__dirname, 'mapcache.xml'); // the location of the config file

// Instantiate a MapCache cache object from the configuration file
mapcache.MapCache.FromConfigFile(conffile, function handleCache(err, cache) {
    if (err) {
        throw err;              // error loading the configuration file
    }

    var urlParts = url.parse(decodeURIComponent(cacheUrl)); // parse the requested url
    var baseUrl = '';                   // the hostname and protocol
    if (urlParts.protocol) {
        baseUrl += urlParts.protocol + '//' + urlParts.hostname;
        if (urlParts.port) {
             baseUrl += ':' + urlParts.port;
        }
    }
    var pathInfo = urlParts.pathname || "/"; // generate the PATH_INFO
    var params = urlParts.query || '';       // generate the QUERY_STRING

    // delegate the request to the MapCache cache object, handling the response
    cache.get(baseUrl, pathInfo, params, function handleCacheResponse(err, cacheResponse) {
        if (err) {
            throw err;          // there was a cache error
        }

        // display the output
        var contentType = cacheResponse.headers['Content-Type'][0]; // get the content type from the headers
        if (contentType.substr(0, 5) === 'image') {
            displayImage(cacheResponse.data);
        } else {
            // output the response as a string to the console
            console.log(cacheResponse.data.toString());
        }
    });
});

/**
 * Pipe image data to ImageMagick's `display`
 */
function displayImage(image) {
    // open display
    var display = child_process.spawn('display', ['-']);

    // pipe the image to display
    display.stdin.write(image);
    display.stdin.end();

    // redirect STDERR and STDOUT to the console
    display.stdout.on('data', function onData(data) {
        console.log(data.toString());
    });

    display.stderr.on('data', function onData(data) {
        console.error(data.toString());
    });
}
