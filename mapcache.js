var url = require('url');
// support node 0.4 (deprecated)
var binding_path = (process.versions.node.split('.', 2).join('.') != '0.4')
    ? './build/Release/bindings'
    : './build/default/bindings';
var bindings = require(binding_path);
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

module.exports.httpCacheHandler = httpCacheHandler;
module.exports.MapCache = bindings.MapCache;
