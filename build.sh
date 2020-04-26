#!/bin/bash

print_help() {
    echo "Usage for build.sh:"
    echo "-h --help      Print help manual"
    echo "-f --format    Format C++ codes"
    echo "-b --build     Build C++ project"
}

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -h|--help)
    print_help
    shift 
    shift 
    ;;
    -f|--format)
    echo "Formatting the code"
    find ./ -iname "*.hpp" -o -iname "*.cpp" | xargs clang-format -i
    shift 
    shift 
    ;;
    -c|--clean)
    echo "Cleaning builds"
    rm -rf build
    shift 
    shift 
    ;;
    -rt|--run_test)
    echo "Running test"
    set -a 
    cd build && ./graph_test
    set +a
    shift 
    shift  
    ;;
    -b|--build)
    echo "Building the project"
    mkdir -p build
    cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && ninja -j8
    shift 
    shift 
    ;;
    -bm)
    echo "Running the benchmark"
    set -a 
    cd build && ./bmark
    set +a
    shift 
    shift 
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift 
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [[ -n $1 ]]; then
    echo "Last line of file specified as non-opt/last argument:"
    tail -1 "$1"
fi