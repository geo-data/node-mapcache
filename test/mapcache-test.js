/**
 * MapCache test harness
 *
 * This test harness uses 'vows' (http://vowsjs.org) to exercise the
 * MapCache API. It can be run directly using vows:
 *
 *    vows --spec test/mapcache-test.js
 *
 * or indirectly using 'npm':
 *
 *    npm test
 */

var vows = require('vows');
var assert = require('assert');
var path = require('path');

var mapcache = require('../lib/mapcache');

vows.describe('mapcache').addBatch({
    // Ensure the module has the expected interface

    'The mapcache module': {
        topic: mapcache,

        'should have a `MapCache` object': {
            topic: function (mapcache) {
                return mapcache.MapCache;
            },
            'which is a function': function (MapCache) {
                assert.isFunction(MapCache);
            },
            'which has the property `FromConfigFile`': {
                topic: function (MapCache) {
                    return MapCache.FromConfigFile;
                },
                'which is a factory method': function (FromConfigFile) {
                    assert.isFunction(FromConfigFile);
                }
            },
            'which has the prototype property `get`': {
                topic: function (MapCache) {
                    return MapCache.prototype.get || false;
                },
                'which is a method': function (get) {
                    assert.isFunction(get);
                }
            },
            'which acts as a constructor': {
                'which cannot be instantiated from javascript': function (MapCache) {
                    var err;
                    try {
                        err = new MapCache();
                    } catch (e) {
                        err = e;
                    }
                    assert.instanceOf(err, TypeError);
                    assert.equal(err.message, 'Argument 0 invalid');
                },
                'which throws an error when called directly': function (MapCache) {
                    var err;
                    try {
                        err = MapCache();
                    } catch (e) {
                        err = e;
                    }
                    assert.instanceOf(err, Error);
                    assert.equal(err.message, 'MapCache() is expected to be called as a constructor with the `new` keyword');
                }
            }
        },
        'should have': {
            topic: mapcache.httpCacheHandler,
            'a `httpCacheHandler`': function (httpCacheHandler) {
                assert.isFunction(httpCacheHandler);
            }
        }
    }
}).addBatch({
    // Ensure `FromConfigFile` has the expected interface

    '`MapCache.FromConfigFile`': {
        topic: function () {
            return mapcache.MapCache.FromConfigFile;
        },

        'requires two valid arguments': {
            topic: function (FromConfigFile) {
                return typeof(FromConfigFile('non-existent-file', function(err, cache) {
                    // do nothing
                }))
            },
            'returning undefined': function (retval) {
                assert.equal(retval, 'undefined');
            }
        },
        'fails with one argument': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile('first-arg');
                } catch (e) {
                    return e;
                }
            },
            'by throwing an error': function (err) {
                assert.instanceOf(err, Error);
                assert.equal(err.message, 'usage: MapCache.FromConfigFile(configfile, callback)');
            }
        },
        'fails with three arguments': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile('first-arg', 'second-arg', 'third-arg');
                } catch (e) {
                    return e;
                }
            },
            'by throwing an error': function (err) {
                assert.instanceOf(err, Error);
                assert.equal(err.message, 'usage: MapCache.FromConfigFile(configfile, callback)');
            }
        },
        'requires a string for the first argument': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile(42, function(err, cache) {
                        // do nothing
                    });
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 0 must be a string');
            }
        },
        'requires a function for the second argument': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile('first-arg', 'second-arg');
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 1 must be a function');
            }
        }
    }
}).addBatch({
    // Ensure mapcache configuration files load as expected

    'A valid config file': {
        topic: path.join(__dirname, 'good.xml'),

        'when loaded with a valid callback': {
            topic: function (config) {
                mapcache.MapCache.FromConfigFile(config, this.callback); // load the cache from file
            },
            'results in a `MapCache`': function (err, result) {
                assert.isNull(err);
                assert.instanceOf(result, mapcache.MapCache);
            }
        }
    },
    'An config file with XML errors': {
        topic: path.join(__dirname, 'parse-error.xml'),

        'when loaded': {
            topic: function (config) {
                mapcache.MapCache.FromConfigFile(config, this.callback); // load the cache from file
            },
            'results in an error': function (err, result) {
                assert.instanceOf(err, Error);
                assert.equal(undefined, result);
                assert.equal('failed to parse', err.message.substr(0, 15));
            }
        }
    },
    'An config file with bad content': {
        topic: path.join(__dirname, 'post-parse-error.xml'),

        'when loaded': {
            topic: function (config) {
                mapcache.MapCache.FromConfigFile(config, this.callback); // load the cache from file
            },
            'results in an error': function (err, result) {
                assert.instanceOf(err, Error);
                assert.equal(undefined, result);
                assert.equal('post-config failed for', err.message.substr(0, 22));
            }
        }
    }
}).addBatch({
    // Ensure `MapCache.get` has the expected interface

    'the `MapCache.get` method': {
        topic: function () {
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), this.callback);
        },
        
        'requires four valid arguments': {
            topic: function (cache) {
                return typeof(cache.get('baseUrl', 'pathInfo', 'queryString', function(err, response) {
                    // do nothing
                }));
            },
            'returning undefined when called': function (retval) {
                assert.equal(retval, 'undefined');
            }
        },
        'fails with one argument': {
            topic: function (cache) {
                try {
                    return cache.get('first-arg');
                } catch (e) {
                    return e;
                }
            },
            'throwing an error': function (err) {
                assert.instanceOf(err, Error);
                assert.equal(err.message, 'usage: cache.get(baseUrl, pathInfo, queryString, callback)');
            }
        },
        'fails with five arguments': {
            topic: function (cache) {
                try {
                    return cache.get('1st', '2nd', '3rd', '4th', '5th');
                } catch (e) {
                    return e;
                }
            },
            'throwing an error': function (err) {
                assert.instanceOf(err, Error);
                assert.equal(err.message, 'usage: cache.get(baseUrl, pathInfo, queryString, callback)');
            }
        },
        'requires a string for the first argument': {
            topic: function (cache) {
                try {
                    return cache.get(null, '2nd', '3rd', function(err, response) {
                        // do nothing
                    });
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 0 must be a string');
            }
        },
        'requires a string for the second argument': {
            topic: function (cache) {
                try {
                    return cache.get('1st', null, '3rd', function(err, response) {
                        // do nothing
                    });
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 1 must be a string');
            }
        },
        'requires a string for the third argument': {
            topic: function (cache) {
                try {
                    return cache.get('1st', '2nd', null, function(err, response) {
                        // do nothing
                    });
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 2 must be a string');
            }
        },
        'requires a function for the third argument': {
            topic: function (cache) {
                try {
                    return cache.get('1st', '2nd', '3rd', null);
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 3 must be a function');
            }
        }
    }
}).addBatch({
    // Ensure retrieving mapcache WMS resources works as expected

    'requesting an invalid WMS cache resource': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000',
                    '/',
                    'LAYERS=test&SERVICE=WMS',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 400 reponse code': function (response) {
                assert.strictEqual(response.code, 400);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers,  {
                    'Content-Type': [ 'application/vnd.ogc.se_xml' ]
                });
            },
            'which returns XML data': function (response) {
                assert.isObject(response.data);
                var string = response.data.toString();
                assert.equal(string.substr(0, 5), '<?xml');
            },
            'which does not have a last modified timestamp': function (response) {
                assert.isUndefined(response.mtime);
            }
        }
    },
    'a WMS `GetCapabilities` request': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000',
                    '/',
                    'SERVICE=WMS&REQUEST=GetCapabilities',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers,  {
                    'Content-Type': [ 'text/xml' ]
                });
            },
            'which returns XML data': function (response) {
                assert.isObject(response.data);
                var string = response.data.toString();
                assert.equal(string.substr(0, 5), '<?xml');
            }
        }
    },
    'a WMS `GetMap` request': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000',
                    '/',
                    'SERVICE=WMS&REQUEST=GetMap&VERSION=1.1.1&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=400&HEIGHT=400&LAYERS=test',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.includes(response.headers,  'Cache-Control');
                assert.includes(response.headers,  'Expires');
                assert.deepEqual(response.headers['Content-Type'], [ 'image/jpeg' ]);
            },
            'which returns binary image data': function (response) {
                assert.isObject(response.data);
                assert.isTrue(response.data.length > 0);
            },
            'which has a last modified timestamp': function (response) {
                assert.instanceOf(response.mtime, Date);
            }
        }
    }
}).addBatch({
    // Ensure retrieving mapcache TMS resources works as expected

    'requesting an invalid TMS cache resource': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000/',
                    '/tms/1.0.1',
                    '',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 404 reponse code': function (response) {
                assert.strictEqual(response.code, 404);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers,  {
                    'Content-Type': [ 'text/plain' ]
                });
            },
            'which returns data': function (response) {
                assert.isObject(response.data);
                assert.isTrue(response.data.length > 0);
            },
            'which does not have a last modified timestamp': function (response) {
                assert.isUndefined(response.mtime);
            }
        }
    },
    'a TMS `GetCapabilities` request': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000/',
                    '/tms/1.0.0',
                    '',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers,  {
                    'Content-Type': [ 'text/xml' ]
                });
            },
            'which returns XML data': function (response) {
                assert.isObject(response.data);
                var string = response.data.toString();
                assert.equal(string.substr(0, 15), '<TileMapService');
            }
        }
    },
    'a TMS tile request': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000',
                    '/tms/1.0.0/test@WGS84/0/0/0.png',
                    '',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.includes(response.headers,  'Cache-Control');
                assert.includes(response.headers,  'Expires');
                assert.deepEqual(response.headers['Content-Type'], [ 'image/png' ]);
            },
            'which returns binary image data': function (response) {
                assert.isObject(response.data);
                assert.isTrue(response.data.length > 0);
            },
            'which has a last modified timestamp': function (response) {
                assert.instanceOf(response.mtime, Date);
            }
        }
    }
}).addBatch({
    // Ensure retrieving mapcache KML resources works as expected

    'requesting an invalid KML cache resource': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000/',
                    '/kml/foo',
                    '',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 404 reponse code': function (response) {
                assert.strictEqual(response.code, 404);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers,  {
                    'Content-Type': [ 'text/plain' ]
                });
            },
            'which returns data': function (response) {
                assert.isObject(response.data);
                assert.isTrue(response.data.length > 0);
            },
            'which does not have a last modified timestamp': function (response) {
                assert.isUndefined(response.mtime);
            }
        }
    },
    'a KML overlay request': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                cache.get(
                    'http://localhost:3000',
                    '/kml/test@WGS84/0/0/0.kml',
                    '',
                    self.callback);
            });            
        },
        'returns an response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'application/vnd.google-earth.kml+xml' ]);
            },
            'which returns XML data': function (response) {
                assert.isObject(response.data);
                var string = response.data.toString();
                assert.equal(string.substr(0, 5), '<?xml');
            }
        }
    }
}).export(module); // Export the Suite
