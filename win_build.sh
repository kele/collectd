set -e

if [ -d "__build_gnulib" ]; then
    echo "Assuming that gnulib is built already, because __build_gnulib exists."
else
    ./gnulib_build.sh
fi

export GNULIB_DIR=`pwd`/__build_gnulib/_build/gllib

./build.sh
./configure --prefix="C:/opt" --disable-all-plugins --enable-logfile CFLAGS="-I$GNULIB_DIR" LDFLAGS="-L$GNULIB_DIR" LIBS="-lgnu" --host="mingw32"
cp $GNULIB_dir/../config.h src/gnulib_config.h
#sed -i '1i#include <gnulib_config.h>\n#include <config.h.in>' src/config.h
cp my_libtool libtool
make
