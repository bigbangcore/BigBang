#!/bin/bash

origin_path=$(cd `dirname $0`; pwd)
cd `dirname $0`

# create build directory
if [ ! -d "build/" ]; then
    mkdir build
fi

# go to build
cd build

# cmake
flagdebug=""
if [[ "$1" = "debug" || "$2" = "debug" ]]
then
    flagdebug="-DCMAKE_BUILD_TYPE=Debug"
else
    flagdebug="-DCMAKE_BUILD_TYPE=Release"
fi

flagtestnet=""
if [[ "$1" = "testnet" || "$2" = "testnet" ]]
then
    flagtestnet="-DTESTNET=on"
else
    flagtestnet="-DTESTNET=off"
fi

cmake .. $flagdebug $flagtestnet
if [ $? -ne 0 ]; then 
    cd $origin_path
    exit 1 
fi 

# make & install
os=`uname`
if [ "$os" == "Darwin" ]; then
    cores=`sysctl -n hw.logicalcpu`
    if [ "${cores}" == "" ]; then
        cores = 1
    fi
    echo "make install -j${cores}"
    make install -j${cores}
    
    if [ $? -ne 0 ]; then   
        exit 1 
    fi
else
    cores=`nproc --all`
    if [ "${cores}" == "" ]; then
        cores = 1
    fi
    echo "make -j${cores}"
    make -j${cores}

    if [ $? -ne 0 ]; then   
        exit 1 
    fi 

    echo "sudo make install"
    sudo make install
fi

cd $origin_path
