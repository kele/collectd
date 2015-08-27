set -e

if [ -d "__build_gnulib" ]; then
    echo "Assuming that gnulib is built already, because __build_gnulib exists."
else
    ./gnulib_build.sh
fi

export GNULIB_DIR=`pwd`/__build_gnulib/_build/gllib
export CURL_DIR='/home/kele/curl-7.40.0-devel-mingw64'

./build.sh
./configure --prefix="C:/opt" --disable-all-plugins \
    CFLAGS="-Drestrict=__restrict -I${CURL_DIR}/include -I$GNULIB_DIR" \
    LDFLAGS="-L$GNULIB_DIR -L${CURL_DIR}/bin" \
    LIBS="-lgnu" \
    --host="mingw32" \
    --enable-logfile \
    --enable-write_http \
    --enable-write_log \
    --enable-disk
cp ${GNULIB_DIR}/../config.h src/gnulib_config.h
echo "#include <config.h.in>" >> src/gnulib_config.h
cp my_libtool libtool
make
