#!/bin/bash

##
# Mapcache regression testing script for use with `git bisect run`
#
# Commits in mapcache will sometimes cause the node-mapcache test suite to
# fail.  This script can be used in conjunction with `git bisect run` to find
# out which mapcache commit caused the failure: basically it builds and
# installs mapcache, builds node-mapcache against the install, tests
# node-mapcache and passes the test exit status back to `git bisect`
#
# Example: Assume you have a situation where mapcache `HEAD` has a change that
# is failing the node-mapcache test suite. You know for a fact that commit
# `f42f9cce244083f5fa19816a7609b11ba117a587` passed the test suite.  Your
# Mapcache checkout is in `/tmp/mapcache`; you are installing mapcache to
# `/tmp/mapcache-install`; your node-mapcache checkout is at
# `/tmp/node-mapcache`: you would run the following commands to find the commit
# which introduced the change that caused the test suite to fail:
#
#    cd /tmp/mapcache
#    git bisect start HEAD f42f9cce244083f5fa19816a7609b11ba117a587 --
#    git bisect run /tmp/node-mapcache/tools/git-bisect-run.sh /tmp/mapcache-install
#
# N.B. You may need to change the `cmake` or `./configure` arguments to suit
# your environment.
#

INSTALL_PREFIX=$1               # where is mapcache going to be installed?
GIT_DIR=`pwd`                   # cache the working directory
NODE_MAPCACHE_DIR="$( dirname "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )" )" # where is the node-mapcache root directory?

# build and install mapcache
cd $GIT_DIR
git status --short | cut -b 4- | xargs rm -rf
if [ -f ./CMakeLists.txt ]; then # it's a cmake build
    cmake CMakeLists.txt \
        -DWITH_BERKELEY_DB=1 \
        -DWITH_PIXMAN=1 \
        -DCMAKE_PREFIX_PATH=${INSTALL_PREFIX}/ \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX}/ \
        -DWITH_FCGI=0 \
        -DWITH_SQLITE=0 \
        -DWITH_OGR=0 \
        -DWITH_APACHE=0;
else                            # it's an autotools build
    ./configure --prefix=${INSTALL_PREFIX}/ \
        --disable-module \
        --without-sqlite \
        --with-pixman=${INSTALL_PREFIX}/lib/pkgconfig/pixman-1.pc \
        --with-bdb \
        --with-bdb-dir=${INSTALL_PREFIX} \
        --with-apxs=/usr/sbin/apxs; 
fi
make && make install

# build and test node-mapcache
cd $NODE_MAPCACHE_DIR
rm -rf build
npm_config_mapcache_build_dir=$GIT_DIR ./node_modules/.bin/node-gyp --debug configure build
./node_modules/.bin/vows --spec ./test/mapcache-test.js
EXIT=$?                         # get the exit status from vows

# Ensure segmentation faults are tested for: `git bisect run` fails when there
# is an exit code >= 129 - segmentation faults return 139 so we must downgrade
# them
if [ $EXIT -eq 139 ]; then EXIT=2; fi;

# clean up
cd $GIT_DIR
git status --short | cut -b 4- | xargs rm -rf

# return the vows exit status to git-bisect
exit $EXIT
