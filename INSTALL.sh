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
flag=""
if [ "$1" == "debug" ]; then
    flag="-DCMAKE_BUILD_TYPE=Debug"
else
    flag="-DCMAKE_BUILD_TYPE=Release"
fi

flag+=" -DPAHO_BUILD_STATIC=TRUE -DPAHO_BUILD_SHARED=FALSE"

cmake .. $flag
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
