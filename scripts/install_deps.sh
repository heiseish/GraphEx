#!/bin/bash
#cmake
# wget https://github.com/Kitware/CMake/releases/download/v3.17.1/cmake-3.17.1.tar.gz && \
#     tar xzf cmake-3.17.1.tar.gz && cd cmake-3.17.1 && ./configure && make -j`nproc` && sudo make install
# ninja/gtest/boost
sudo apt-get install -y  ninja-build libboost-all-dev build-essential cmake

git clone https://github.com/google/googletest.git && \
    cd googletest && mkdir build && cd build && \
    cmake .. -DBUILD_SHARED_LIBS=ON -DINSTALL_GTEST=ON && make -j`nproc` && sudo make install && sudo ldconfig
