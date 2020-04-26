#!/bin/bash
# build the project
mkdir -p build && \
    cd build && \
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DRUN_BENCHMARK=OFF .. && \
    ninja -j8

# Run test
cd build && ./graph_test