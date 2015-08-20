set -e

./build.sh
./configure --disable-all-plugins --enable-logfile CFLAGS="-I$GNULIB_DIR" LDFLAGS="-L$GNULIB_DIR" LIBS="-lgnu" --host="mingw32"
cp my_libtool libtool
make
