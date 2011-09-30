var path = require('path');
var geocache = require('./build/default/bindings.node');
console.log('geocache loaded');

var conffile = path.resolve('./geocache.xml');
var service = new geocache.GeoCache(conffile);

console.log('service created from ' + conffile);

var base = "http://localhost:8081";
var pathInfo = "/foo/bar";
var params = 'LAYERS=aerial-photos&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A27700&BBOX=431380.11079545,89627.893687851,432044.11115055,89950.632057879&WIDTH=1398&HEIGHT=679';

service.get(base, pathInfo, params, function (err, response) {
    if (err) {
        throw err;
    }
    console.log(response);
});

console.log('done');
