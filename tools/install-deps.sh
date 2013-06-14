#!/bin/sh

# Install a checkout of mapcache
#
# This is used by Travis-CI for installing a version of mapcache against which
# node-mapcache can be built and tested.

die() {
    if [ -n "${1}" ]; then
        echo $1
    fi
    exit 1;
}

PREFIX=$1                       # the directory to install into
MAPCACHE_COMMIT=$2              # the commit number to checkout (optional)

if [ -z "${PREFIX}" ]; then
    die "usage: install-deps.sh PREFIX [ MAPCACHE_COMMIT ]"
fi

# clone the mapcache repository
git clone https://github.com/mapserver/mapcache.git $PREFIX/mapcache || die "Git clone failed"
cd ${PREFIX}/mapcache || die "Cannot cd to ${PREFIX}/mapcache"
if [ -n "${MAPCACHE_COMMIT}" ]; then
    git checkout $MAPCACHE_COMMIT || die "Cannot checkout ${MAPCACHE_COMMIT}"
fi

# build and install mapcache
if [ -f ./CMakeLists.txt ]; then # it's a cmake build
    cmake CMakeLists.txt \
        -DWITH_FCGI=0 \
        -DWITH_SQLITE=0 \
        -DWITH_GDAL=0 \
        -DWITH_OGR=0 \
        -DWITH_GEOS=0 \
        -DWITH_APACHE=0 \
        -DCMAKE_INSTALL_PREFIX=${PREFIX}/mapcache-install || die "cmake failed"
else                            # it's an autotools build
    autoreconf --force --install || die "autoreconf failed"
    ./configure --prefix=${PREFIX}/mapcache-install --without-sqlite --without-bdb --disable-module || die "configure failed"
fi

make || die "make failed"
make install || die "make install failed"

# point `npm` at the build
npm config set mapcache:build_dir ${PREFIX}/mapcache
