var geocache = require('./build/default/bindings.node');
console.log('geocache loaded');

var conffile = '/home/homme/checkouts/mod-geocache/test2-geocache.xml';
//var conffile = '/home/homme/checkouts/mod-geocache/test-geocache.xml';
var service = new geocache.GeoCache(conffile);

console.log('service created from ' + conffile);

var base = "http://localhost:8081";
var pathInfo = "/foo/bar";
//var params = "LAYERS=test,test3&FORMAT=image%2Fpng&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&SRS=EPSG%3A4326&BBOX=-2.8125,47.8125,0,50.625&WIDTH=256&HEIGHT=256";
var params = 'LAYERS=uk&SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&EXCEPTIONS=application%2Fvnd.ogc.se_inimage&FORMAT=image%2Fjpeg&SRS=EPSG%3A4326&BBOX=-50,0,-49.5,0.5&WIDTH=256&HEIGHT=256';

console.log(service.get(base, pathInfo, params));

//var service2 = new geocache.GeoCache(conffile);
//console.log(service2.get());

console.log('done');
