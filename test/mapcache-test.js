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

var vows = require('vows'),
    assert = require('assert'),
    path = require('path'),
    fs = require('fs'),
    events = require('events'),
    mapcache = require('../lib/mapcache');

function checkContentLength(response, expectedLength) {
    assert.includes(response.headers, 'Content-Length');

    if (arguments.length == 2) {
        assert.deepEqual(response.headers['Content-Length'], [ expectedLength ]);
    } else {
	assert.isTrue(response.headers['Content-Length'][0] > 20); // ensure there are some bytes
    }

    // ensure the content length matches the data length
    if (response.data) {
	assert.equal(response.headers['Content-Length'][0], response.data.length);
    }
}

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
                        err = new MapCache('test');
                    } catch (e) {
                        err = e;
                    }
                    assert.instanceOf(err, Error);
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
                },
                'which requires the correct number of arguments': function (MapCache) {
                    var err;
                    try {
                        err = new MapCache('yes', 'ok', 'oops!'); // only accepts two arguments
                    } catch (e) {
                        err = e;
                    }
                    assert.instanceOf(err, Error);
                    assert.equal(err.message, 'usage: new MapCache(config, [logger])');
                }
            }
        },
        'should have a `versions` property': {
            topic: function (mapcache) {
                return mapcache.versions;
            },
            'which is an object': function (versions) {
                assert.isObject(versions);
            },
            'which contains the node-mapcache version': {
                topic: function (versions) {
                    return versions.node_mapcache;
                },
                'is a string': function (version) {
                    assert.isString(version);
                    assert.isTrue(version.length > 0);
                },
                'which is the same as that in `package.json`': function (version) {
                    var contents = fs.readFileSync(path.join(__dirname, '..', 'package.json')),
                        json = JSON.parse(contents),
                        pversion = json.version;
                    assert.equal(version, pversion);
                }
            },
            'which contains the mapcache version': {
                topic: function (versions) {
                    return versions.mapcache;
                },
                'as a string': function (version) {
                    assert.isString(version);
                    assert.isTrue(version.length > 0);
                }
            },
            'which contains the apr version': {
                topic: function (versions) {
                    return versions.apr;
                },
                'as a string': function (version) {
                    assert.isString(version);
                    assert.isTrue(version.length > 0);
                }
            }
        },
        'should have a `logLevels` property': {
            topic: mapcache.logLevels,
            'which is an object': function (logLevels) {
                assert.isObject(logLevels);
            },
            'which contains the `debug` level': function (logLevels) {
                assert.isNumber(logLevels.DEBUG);
            },
            'which contains the `info` level': function (logLevels) {
                assert.isNumber(logLevels.INFO);
            },
            'which contains the `notice` level': function (logLevels) {
                assert.isNumber(logLevels.NOTICE);
            },
            'which contains the `warn` level': function (logLevels) {
                assert.isNumber(logLevels.WARN);
            },
            'which contains the `error` level': function (logLevels) {
                assert.isNumber(logLevels.ERROR);
            },
            'which contains the `crit` level': function (logLevels) {
                assert.isNumber(logLevels.CRIT);
            },
            'which contains the `alert` level': function (logLevels) {
                assert.isNumber(logLevels.ALERT);
            },
            'which contains the `emerg` level': function (logLevels) {
                assert.isNumber(logLevels.EMERG);
            }
        }
    }
}).addBatch({
    // Ensure `FromConfigFile` has the expected interface

    '`MapCache.FromConfigFile`': {
        topic: function () {
            return mapcache.MapCache.FromConfigFile;
        },

        'works with two valid arguments': {
            topic: function (FromConfigFile) {
                return typeof(FromConfigFile('non-existent-file', function(err, cache) {
                    // do nothing
                }));
            },
            'returning undefined': function (retval) {
                assert.equal(retval, 'undefined');
            }
        },
        'works with three valid arguments': {
            topic: function (FromConfigFile) {
                var logger = new events.EventEmitter();
                return typeof(FromConfigFile('non-existent-file', logger, function(err, cache) {
                    // do nothing
                }));
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
                assert.equal(err.message, 'usage: MapCache.FromConfigFile(configfile, [logger], callback)');
            }
        },
        'fails with four arguments': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile('first-arg', 'second-arg', 'third-arg', 'fourth-arg');
                } catch (e) {
                    return e;
                }
            },
            'by throwing an error': function (err) {
                assert.instanceOf(err, Error);
                assert.equal(err.message, 'usage: MapCache.FromConfigFile(configfile, [logger], callback)');
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
        'requires a function for the second argument with two arguments': {
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
        },
        'requires an object for the second argument with three arguments': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile('first-arg', 'bad argument', 'third-arg');
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 1 must be an object');
            }
        },
        'requires a function for the third argument with three arguments': {
            topic: function (FromConfigFile) {
                try {
                    return FromConfigFile('first-arg', new events.EventEmitter(), 'second-arg');
                } catch (e) {
                    return e;
                }
            },
            'throwing an error otherwise': function (err) {
                assert.instanceOf(err, TypeError);
                assert.equal(err.message, 'Argument 2 must be a function');
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
                return cache.get(
                    'http://localhost:3000',
                    '/',
                    'LAYERS=test&SERVICE=WMS',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 400 reponse code': function (response) {
                assert.strictEqual(response.code, 400);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'application/vnd.ogc.se_xml' ]);
		checkContentLength(response);
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
                return cache.get(
                    'http://localhost:3000',
                    '/',
                    'SERVICE=WMS&REQUEST=GetCapabilities',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'text/xml' ]);
		checkContentLength(response);
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
                return cache.get(
                    'http://localhost:3000',
                    '/',
                    'SERVICE=WMS&REQUEST=GetMap&VERSION=1.1.1&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=400&HEIGHT=400&LAYERS=test',
                    self.callback);
            });
        },
        'returns a response': {
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
		checkContentLength(response);
            },
            'which returns binary image data': function (response) {
                assert.isObject(response.data);
                assert.isTrue(response.data.length > 0);
            },
            'which has a last modified timestamp': function (response) {
                assert.instanceOf(response.mtime, Date);
            }
        }
    },
    'a WMS `GetFeatureInfo` request': {
        topic: function () {
            var self = this;
            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), function (err, cache) {
                if (err) {
                    return self.callback(err, null);
                }
                return cache.get(
                    'http://localhost:3000',
                    '/',
                    'SERVICE=WMS&REQUEST=GetFeatureInfo&VERSION=1.1.1&SRS=EPSG%3A4326&BBOX=-180,-90,180,90&WIDTH=400&HEIGHT=400&QUERY_LAYERS=basic&X=200&Y=200&INFO_FORMAT=text/plain',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'text/plain' ]);
		checkContentLength(response);
            },
            'which returns data': function (response) {
                assert.isObject(response.data);
                assert.isTrue(response.data.length > 0);
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
                return cache.get(
                    'http://localhost:3000/',
                    '/tms/1.0.1',
                    '',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 404 reponse code': function (response) {
                assert.strictEqual(response.code, 404);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'text/plain' ])
		checkContentLength(response);
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
                return cache.get(
                    'http://localhost:3000/',
                    '/tms/1.0.0',
                    '',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'text/xml' ]);
		checkContentLength(response);
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
                return cache.get(
                    'http://localhost:3000',
                    '/tms/1.0.0/test@WGS84/0/0/0.png',
                    '',
                    self.callback);
            });
        },
        'returns a response': {
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
		checkContentLength(response);
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
                return cache.get(
                    'http://localhost:3000/',
                    '/kml/foo',
                    '',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 404 reponse code': function (response) {
                assert.strictEqual(response.code, 404);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'text/plain' ]);
		checkContentLength(response);
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
                return cache.get(
                    'http://localhost:3000',
                    '/kml/test@WGS84/0/0/0.kml',
                    '',
                    self.callback);
            });
        },
        'returns a response': {
            'which is an object': function (response) {
                assert.instanceOf(response, Object);
            },
            'which has a 200 reponse code': function (response) {
                assert.strictEqual(response.code, 200);
            },
            'which has the correct headers': function (response) {
                assert.deepEqual(response.headers['Content-Type'], [ 'application/vnd.google-earth.kml+xml' ]);
		checkContentLength(response);
            },
            'which returns XML data': function (response) {
                assert.isObject(response.data);
                var string = response.data.toString();
                assert.equal(string.substr(0, 5), '<?xml');
            }
        }
    }
}).addBatch({
    // Ensure the logger works as expected

    'requesting an invalid KML cache resource': {
        topic: function () {
            var promise = new events.EventEmitter(),
                logger = new events.EventEmitter(),
                timeout = setTimeout(function() {
                    logger.emit('error', new Error('No log was emitted'));
                }, 2000);

            function log(level, msg) {
                clearTimeout(timeout);

                // ignore all levels bar ERROR.
                if (level == mapcache.logLevels.ERROR) {
                    promise.emit('success', level, msg);
                    logger.removeListener('log', log); // we've done our job
                }
            }

            logger.on('log', log);

            mapcache.MapCache.FromConfigFile(path.join(__dirname, 'good.xml'), logger, function (err, cache) {
                if (err) {
                    clearTimeout(timeout);
                    return promise.emit('error', err);
                }
                return cache.get(
                    'http://localhost:3000/',
                    '/kml/foobar',
                    '',
                    function (err, response) {
                        if (err) {
                            clearTimeout(timeout);
                            promise.emit('error', err);
                        }
                    });
            });

            return promise;
        },
        'logs a valid log level': function (err, level, msg) {
            assert.isNull(err);
            assert.isNumber(level);
            assert.equal(level >= mapcache.logLevels.DEBUG, true);
            assert.equal(level <= mapcache.logLevels.EMERG, true);
        },
        'logs a message': function (err, level, msg) {
            assert.isNull(err);
            assert.isString(msg);
            assert.equal(msg.length > 0, true);
        }
    }
}).export(module); // Export the Suite
