#!/bin/bash
#cmake
wget https://github.com/Kitware/CMake/releases/download/v3.17.1/cmake-3.17.1-Linux-x86_64.tar.gz && \
    tar xzf cmake-3.17.1.tar.gz && cd cmake-3.17.1-Linux-x86_64 && ./configure && make -j4 && make install
# ninja/gtest/boost
apt-get install -y wget ninja-build libboost-all-dev googletest

