#!/bin/bash
set -e

source paths_shared_0.6dev.sh

BUILD_PATH=${GKFS_MN_BUILD_PATH:?}
DEPS_PATH=${GKFS_MN_DEPS_INSTALL_PATH:?}
INSTALL_PATH=${GKFS_MN_INSTALL_PATH:?}
SRC=${GKFS_MN_SRC}/..

#module load gnu8
#export CC=/opt/ohpc/pub/compiler/gcc/8.3.0/bin/gcc
#export CXX=/opt/ohpc/pub/compiler/gcc/8.3.0/bin/g++
export CC="ccache gcc"
export CXX="ccache g++"

#module load libfabric/1.8.0
#export LD_LIBRARY_PATH=/home/software/libfabric/1.8.0/lib:/home/software/psm2/11.2.77/usr/lib64:$LD_LIBRARY_PATH

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set +e
#source ${DIR:?}/set_cmake.sh
set -e
# export BOOST_LIB="/home/nx01/shared/GekkoFS_BSC/0.6dev/lib"
# export BOOST_INCLUDEDIR="/home/nx01/shared/GekkoFS_BSC/0.6dev/include"
#export BOOST_LIB="/home/nx01/shared/GekkoFS_BSC/lib"
#export BOOST_INCLUDEDIR="/home/nx01/shared/GekkoFS_BSC/include"

rm -rf ${BUILD_PATH:?}
mkdir -p ${BUILD_PATH:?} && cd ${BUILD_PATH:?}
#export CXX=/opt/ohpc/pub/compiler/gcc/8.3.0/bin/g++
#export CC=/opt/ohpc/pub/compiler/gcc/8.3.0/bin/gcc

cmake \
    -Wdev \
    -Wdeprecated \
    -DCMAKE_PREFIX_PATH:STRING="${DEPS_PATH:?};${CMAKE_PREFIX_PATH}" \
    -DBoost_DEBUG:BOOL=OFF \
    -DCMAKE_BUILD_TYPE=Debug \
    -DRPC_PROTOCOL="ofi+sockets" \
    -DUSE_SHM:BOOL=OFF \
    -DCMAKE_CXX_FLAGS_DEBUG="-ggdb3" \
    -DLOG_SYSCALLS:BOOL=OFF \
    -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_PATH:?} \
    -DSYMLINK_SUPPORT:BOOL=OFF \
    ${SRC:?}

#-DUSE_OFI_TCP:BOOL=OFF \
#time make
time make -j$(nproc)
make install

