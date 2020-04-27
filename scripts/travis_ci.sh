#!/bin/bash
# build the project
if [ -d "build" ]; then rm -rf build; fi
mkdir -p build && \
    cd build && \
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DRUN_BENCHMARK=OFF .. && \
    ninja -j8 && ./graph_test
rm -rf build