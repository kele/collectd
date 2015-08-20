
SRC_DIR=`pwd`

export LIBS="-lws2_32"

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
    sys_wait 
cd ${SRC_DIR}/__build_gnulib/_build
./configure
make
make install
#echo "Run: ./configure && make && make install"
