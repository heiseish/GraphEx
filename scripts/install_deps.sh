#!/bin/bash
#cmake
wget https://github.com/Kitware/CMake/releases/download/v3.17.1/cmake-3.17.1.tar.gz && \
    tar xzf cmake-3.17.1.tar.gz && cd cmake-3.17.1 && ./configure && make -j`nproc` && sudo make install
# ninja/gtest/boost
sudo apt-get install -y wget ninja-build libboost-all-dev googletest

