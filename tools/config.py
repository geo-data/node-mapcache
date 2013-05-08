
#!/usr/bin/env python2

##############################################################################
# Copyright (c) 2012, GeoData Institute (www.geodata.soton.ac.uk)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  - Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
#  - Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
##############################################################################

"""
Output mapcache configuration information to `node-gyp`

Configuration options are retrieved from environment variables set using `npm
config set`.  This allows for a simple `npm install mapcache` to work.
"""

from optparse import OptionParser
import os
import re

class ConfigError(Exception):
    pass

class Config(object):
    """Base Class for obtaining mapcache configuration information"""

    def __init__(self, build_dir):
        self.build_dir = build_dir

    def getLibDir(self):
        return os.environ.get('npm_config_mapcache_lib_dir', '')

    def getIncludeDir(self):
        return os.path.join(self.build_dir, 'include')

    def getCflags(self):
        # add debugging flags and defines
        if 'npm_config_mapcache_debug' in os.environ:
            return '-DDEBUG -g -ggdb'
        return ''

class AutoconfConfig(Config):
    """Class for obtaining mapcache configuration pre mapcache 1.0

    Mapcache uses autotools for building and configuration in this version.
    """

    def __init__(self, *args, **kwargs):
        super(AutoconfConfig, self).__init__(*args, **kwargs)
        makefile_inc = os.path.join(self.build_dir, 'Makefile.inc')
        if not os.path.exists(makefile_inc):
            raise ConfigError('Expected `Makefile.inc` in %s' % self.build_dir)

        self.makefile_inc = makefile_inc

    def getLibDir(self):
        p = re.compile('^prefix *= *(.+)$') # match the prefix
        with open(self.makefile_inc, 'r') as f:
            for line in f:
                match = p.match(line)
                if match:
                    arg = match.groups()[0].strip()
                    if arg:
                        return os.path.join(arg, 'lib')
        return ''

    def getCflags(self):
        # add includes from the Makefile
        p = re.compile('^[A-Z]+_INC *= *(.+)$') # match an include header
        matches = []
        with open(self.makefile_inc, 'r') as f:
            for line in f:
                match = p.match(line)
                if match:
                    arg = match.groups()[0].strip()
                    if arg:
                        matches.append(arg)

        debug_flags = super(AutoconfConfig, self).getCflags()
        if debug_flags:
            matches.append(debug_flags)

        return ' '.join(matches)

class CmakeConfig(Config):
    """Class for obtaining mapcache configuration for versions >= 1.0

    Mapcache uses Cmake for building and configuration in this version.
    """

    def __init__(self, *args, **kwargs):
        super(CmakeConfig, self).__init__(*args, **kwargs)
        cmake_cache = os.path.join(self.build_dir, 'CMakeCache.txt')
        if not os.path.exists(cmake_cache):
            raise ConfigError('Expected `CMakeCache.txt` in %s' % self.build_dir)

        self.cmake_cache = cmake_cache

    def getLibDir(self):
        p = re.compile('^CMAKE_INSTALL_PREFIX:PATH *= *(.+)$') # match the prefix

        with open(self.cmake_cache, 'r') as f:
            for line in f:
                match = p.match(line)
                if match:
                    arg = match.groups()[0].strip()
                    if arg:
                        return os.path.join(arg, 'lib')

        return ''

    def getIncludeDir(self):
        dirs = [os.path.join(self.build_dir, 'include')]
        p = re.compile('^\w+_INCLUDE_DIR:PATH *= *(.+)$') # match a library directory

        with open(self.cmake_cache, 'r') as f:
            for line in f:
                match = p.match(line)
                if match:
                    arg = match.groups()[0].strip()
                    if arg:
                        dirs.append(arg)

        return ' '.join(dirs)


parser = OptionParser()
parser.add_option("--include",
                  action="store_true", default=False,
                  help="output the mapcache include path")

parser.add_option("--libraries",
                  action="store_true", default=False,
                  help="output the mapcache library link option")

parser.add_option("--ldflags",
                  action="store_true", default=False,
                  help="output the mapcache library rpath option")

parser.add_option("--cflags",
                  action="store_true", default=False,
                  help="output the mapcache cflag options")

(options, args) = parser.parse_args()

try:
    build_dir = os.environ['npm_config_mapcache_build_dir']
except KeyError:
    raise EnvironmentError('`npm config set mapcache:build_dir` has not been called')

# get the config object, trying the legacy autoconf build sytem first and
# falling back to the new cmake system
try:
    config = AutoconfConfig(build_dir)
except ConfigError:
    config = CmakeConfig(build_dir)

if options.include:
    print config.getIncludeDir()

if options.libraries:
    lib_dir = config.getLibDir()
    if lib_dir:
        print "-L%s" % lib_dir

if options.ldflags:
    # write the library path into the resulting binary
    lib_dir = config.getLibDir()
    if lib_dir:
        print "-Wl,-rpath=%s" % lib_dir

if options.cflags:
    print config.getCflags()
