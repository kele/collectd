set -e

if [ -d "__build_gnulib" ]; then
    echo "Assuming that gnulib is built already, because __build_gnulib exists."
else
    ./gnulib_build.sh
fi

export GNULIB_DIR=`pwd`/__build_gnulib/__build/gllib

./build.sh
./configure --disable-all-plugins --enable-logfile CFLAGS="-I$GNULIB_DIR" LDFLAGS="-L$GNULIB_DIR" LIBS="-lgnu" --host="mingw32"
cp my_libtool libtool
make
