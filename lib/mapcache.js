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
 * Node.js MapCache Tile Caching Module
 *
 * This module provides a map tile caching solution for Node.js using
 * Mapserver Mapcache.
 *
 * See the README for further details.
 */

var url = require('url'),
    path = require('path'),
    bindings = require('../build/Release/bindings.node');

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
        //console.log('Serving ' + httpRequest.url);
    }

    return handleCacheResponse;
}
// Build a callback to handle HTTP cache requests
function httpCacheHandler(cache) {

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

// Export the API
module.exports.httpCacheHandler = httpCacheHandler; // not documented
module.exports.MapCache = bindings.MapCache;
module.exports.versions = bindings.versions;
module.exports.logLevels = bindings.logLevels;
