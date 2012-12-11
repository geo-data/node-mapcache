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

def get_lib_dir():
    return os.environ.get('npm_config_mapcache_lib_dir', '')

def get_include_dir():
    try:
        build_dir = os.environ['npm_config_mapcache_build_dir']
    except KeyError:
        return ''

    return os.path.join(build_dir, 'include')

def get_cflags():
    try:
        build_dir = os.environ['npm_config_mapcache_build_dir']
    except KeyError:
        raise EnvironmentError('`npm config set mapcache:build_dir` has not been called')

    makefile_inc = os.path.join(build_dir, 'Makefile.inc')
    
    # add includes from the Makefile
    p = re.compile('^[A-Z]+_INC *= *(.+)$') # match an include header
    matches = []
    with open(makefile_inc, 'r') as f:
        for line in f:
            match = p.match(line)
            if match:
                arg = match.groups()[0].strip()
                if arg:
                    matches.append(arg)

    # add debugging flags and defines
    if 'npm_config_mapcache_debug' in os.environ:
        matches.extend(["-DDEBUG", "-ggdb", "-pedantic"])

    cflags = ' '.join(matches)
    return cflags

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

if options.include:
    print get_include_dir()

if options.libraries:
    lib_dir = get_lib_dir()
    if lib_dir:
        print "-L%s" % lib_dir

if options.ldflags:
    # write the library path into the resulting binary
    lib_dir = get_lib_dir()
    if lib_dir:
        print "-Wl,-rpath=%s" % lib_dir

if options.cflags:
    print get_cflags()
