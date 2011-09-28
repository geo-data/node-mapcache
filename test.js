var geocache = require('./build/default/bindings.node');
console.log('geocache loaded');

var conffile = '/home/homme/checkouts/mod-geocache/src/geocache.xml';
//var conffile = '/home/homme/checkouts/mod-geocache/test-geocache.xml';
var service = new geocache.GeoCache(conffile);

console.log('service created from ' + conffile);

var base = "http://localhost:8081";
var pathInfo = "/foo/bar";
//var params = "LAYERS=test,test3&FORMAT=image%2Fpng&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&SRS=EPSG%3A4326&BBOX=-2.8125,47.8125,0,50.625&WIDTH=256&HEIGHT=256";
var params = 'LAYERS=aerial-photos&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A4326&BBOX=-1.56505,50.69648,-1.54599,50.71428&WIDTH=1398&HEIGHT=738';

service.get(base, pathInfo, params, function (err, response) {
    if (err) {
        throw err;
    }
    console.log(response);
});

//var service2 = new geocache.GeoCache(conffile);
//console.log(service2.get());

console.log('done');
