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
    shift # past argument
    shift # past value
    ;;
    -f|--format)
    echo "Formatting the code"
    find ./ -iname *.hpp -o -iname *.cpp | xargs clang-format -i
    shift # past argument
    shift # past value
    ;;
    -c|--clean)
    echo "Cleaning builds"
    rm -rf build
    shift # past argument
    shift # past value
    ;;
    -rt|--run_test)
    echo "Running test"
    set -a # automatically export all variables
    cd build && ./graph_test
    set +a
    shift # past argument
    shift # past value
    ;;
    -b|--build)
    echo "Building the project"
    mkdir -p build
    cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. && ninja -j8
    shift # past argument
    shift # past value
    ;;
    -bm)
    echo "Running the benchmark"
    set -a # automatically export all variables
    source .env && cd build && ./dawn_benchmark
    set +a
    shift # past argument
    shift # past value
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [[ -n $1 ]]; then
    echo "Last line of file specified as non-opt/last argument:"
    tail -1 "$1"
fi