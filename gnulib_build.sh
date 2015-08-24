
SRC_DIR=`pwd`

export LIBS="-lws2_32 -lpthread"

mkdir __build_gnulib
cd __build_gnulib
git clone git://git.savannah.gnu.org/gnulib.git
cd gnulib

set -e
set -x

./gnulib-tool --create-testdir \
    --source-base=lib \
    --dir=${SRC_DIR}/__build_gnulib/_build \
    canonicalize-lgpl \
    regex \
    socket \
    nanosleep \
    netdb \
    sendto \
    gettimeofday \
    time_r \
    sys_stat \
    fcntl-h \
    sys_resource \
    sys_wait \
    setlocale \
    strtok_r
cd ${SRC_DIR}/__build_gnulib/_build
./configure --host=mingw32
make 
cd gllib

# We have to rebuild libnug.a to get the list of *.o files to build a dll later
rm libgnu.a
OBJECT_LIST=`make V=1 | grep "ar" | cut -d' ' -f4-`
$CXX -shared -o libgnu.dll $OBJECT_LIST -lws2_32 -lpthread #-Wl,--out-implib,libgnu.dll.a
rm libgnu.a # get rid of it, to use libgnu.dll
