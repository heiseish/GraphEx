format:
	find ./ -iname "*.hpp" -o -iname "*.cpp" | xargs clang-format -i
clean:
	rm -rf build
test:
	echo "Running test"
	cd build && ./graph_test

.PHONY: build

build:
	echo "Building the project"
	mkdir -p build
	cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DRUN_BENCHMARK=ON .. && ninja -j8
bench:
	echo "Running the benchmark"
	cd build && ./bmark