set -e

if [ -d "__build_gnulib" ]; then
    echo "Assuming that gnulib is built already, because __build_gnulib exists."
else
    ./gnulib_build.sh
fi

./build.sh

export GNULIB_DIR=`pwd`/__build_gnulib/_build/gllib
export CURL_DIR='/home/kele/curl-7.40.0-devel-mingw64'
export LDFLAGS="-L$GNULIB_DIR -L${CURL_DIR}/bin"
export LIBS="-lgnu"
export CFLAGS="-Drestrict=__restrict -I${CURL_DIR}/include -I$GNULIB_DIR"

./configure --prefix="C:/opt" --disable-all-plugins \
    --host="mingw32" \
    --enable-logfile \
    --enable-write_http \
    --enable-write_log \
    --enable-wmi

cp ${GNULIB_DIR}/../config.h src/gnulib_config.h
echo "#include <config.h.in>" >> src/gnulib_config.h

export LTCFLAGS="$CFLAGS -I`pwd`/src -include gnulib_config.h -include config.h"
export LTLDFLAGS="$LDFLAGS $LIBS"
cp libtool libtool_bak
sed -i "s%\$LTCC \$LTCFLAGS\(.*cwrapper.*\)%\$LTCC $LTCFLAGS \1 $LTLDFLAGS%" libtool

make
