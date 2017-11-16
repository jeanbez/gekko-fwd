#!/bin/bash

usage() {

    echo "Usage:
    ./compile_dep [ clone_path ] [ install_path ] [ na_plugin ]
    Valid na_plugin arguments: {bmi,cci,ofi,all}"
}

prepare_build_dir() {
    if [ ! -d "$1/build" ]; then
        mkdir $1/build
    fi
    rm -rf $1/build/*
}

if [[ ( -z ${1+x} ) || ( -z ${2+x} ) || ( -z ${3+x} ) ]]; then
    echo "Arguments missing."
    usage
    exit
fi

#LOG=/tmp/adafs_install.log
#echo "" &> $LOG
GIT=$1
INSTALL=$2
NA_LAYER=$3
USE_BMI="-DNA_USE_BMI:BOOL=OFF"
USE_CCI="-DNA_USE_CCI:BOOL=OFF"
USE_OFI="-DNA_USE_OFI:BOOL=OFF"


if [ "$NA_LAYER" == "cci" ] || [ "$NA_LAYER" == "bmi" ] || [ "$NA_LAYER" == "ofi" ] || [ "$NA_LAYER" == "all" ]; then
    echo "$NA_LAYER plugin(s) selected"
else
    echo "No valid plugin selected"
    usage
    exit
fi

echo "Git path is set to '$1'";
echo "Install path is set to '$2'";

mkdir -p $GIT

if [ "$NA_LAYER" == "bmi" ] || [ "$NA_LAYER" == "all" ]; then
    USE_BMI="-DNA_USE_BMI:BOOL=ON"
    echo "Installing BMI"
    # BMI
    CURR=$GIT/bmi
    prepare_build_dir $CURR
    cd $CURR
    ./prepare || exit 1
    cd $CURR/build
    ../configure --prefix=$INSTALL --enable-shared --enable-bmi-only  || exit 1
    make -j8 || exit 1
    make install || exit 1
fi

if [ "$NA_LAYER" == "cci" ] || [ "$NA_LAYER" == "all" ]; then
    USE_CCI="-DNA_USE_CCI:BOOL=ON"
    echo "Installing CCI"
    # CCI
    CURR=$GIT/cci
    prepare_build_dir $CURR
    cd $CURR
    ./autogen.pl || exit 1
    cd $CURR/build
    ../configure --with-verbs --prefix=$INSTALL LIBS="-lpthread"  || exit 1
    make -j8 || exit 1
    make install || exit 1
    make check || exit 1
fi

if [ "$NA_LAYER" == "ofi" ] || [ "$NA_LAYER" == "all" ]; then
    USE_OFI="-DNA_USE_OFI:BOOL=ON"
    echo "Installing LibFabric"
    #libfabric
    CURR=$GIT/libfabric
    prepare_build_dir $CURR
    cd $CURR
    ./autogen.sh || exit 1
    cd $CURR/build
    ../configure --prefix=$INSTALL  || exit 1
    make -j8 || exit 1
    make install || exit 1
    make check || exit 1
fi

echo "Installing Mercury"

# Mercury
CURR=$GIT/mercury
prepare_build_dir $CURR
cd $CURR/build
cmake -DMERCURY_USE_SELF_FORWARD:BOOL=ON -DMERCURY_USE_CHECKSUMS:BOOL=OFF -DBUILD_TESTING:BOOL=ON \
-DMERCURY_USE_BOOST_PP:BOOL=ON -DBUILD_SHARED_LIBS:BOOL=ON -DCMAKE_INSTALL_PREFIX=$INSTALL \
-DCMAKE_BUILD_TYPE:STRING=Release $USE_BMI $USE_CCI $USE_OFI ../  || exit 1
make -j8  || exit 1
make install  || exit 1

echo "Installing Argobots"

# Argobots
CURR=$GIT/argobots
prepare_build_dir $CURR
cd $CURR
./autogen.sh || exit 1
cd $CURR/build
../configure --prefix=$INSTALL || exit 1
make -j8 || exit 1
make install || exit 1
make check || exit 1

echo "Installing Abt-snoozer"
# Abt snoozer
CURR=$GIT/abt-snoozer
prepare_build_dir $CURR
cd $CURR
./prepare.sh || exit 1
cd $CURR/build
../configure --prefix=$INSTALL PKG_CONFIG_PATH=$INSTALL/lib/pkgconfig || exit 1
make -j8 || exit 1
make install || exit 1
make check || exit 1

echo "Installing Margo"
# Margo
CURR=$GIT/margo
prepare_build_dir $CURR
cd $CURR
./prepare.sh || exit 1
cd $CURR/build
../configure --prefix=$INSTALL PKG_CONFIG_PATH=$INSTALL/lib/pkgconfig CFLAGS="-g -Wall" || exit 1
make -j8 || exit 1
make install || exit 1
make check || exit 1

echo "Installing Rocksdb"
# Margo
CURR=$GIT/rocksdb
cd $CURR
make clean || exit 1
sed -i.bak "s#INSTALL_PATH ?= /usr/local#INSTALL_PATH ?= $INSTALL#g" Makefile
make -j8 static_lib || exit 1
make install || exit 1

echo "Done"
