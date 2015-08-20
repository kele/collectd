set -e

./build.sh
./configure --disable-all-plugins CFLAGS="-I$GNULIB_DIR" LDFLAGS="-L$GNULIB_DIR" LIBS="-lgnu" --host="mingw32"
make
