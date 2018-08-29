#!/bin/bash

if [ ! -d "hyperscan-4.7.0" ]; then

apt-get -y install ragel

wget https://github.com/intel/hyperscan/archive/v4.7.0.tar.gz
tar -xf v4.7.0.tar.gz

wget https://dl.bintray.com/boostorg/release/1.67.0/source/boost_1_67_0.tar.gz
tar -xf boost_1_67_0.tar.gz
cp -r boost_1_67_0/boost hyperscan-4.7.0/include

cd hyperscan-4.7.0
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=true ..
make
make install

fi
