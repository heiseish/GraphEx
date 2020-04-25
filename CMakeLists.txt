cmake_minimum_required (VERSION 3.15)
project (DawnCpp)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++17")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(3RDPARTY_INCLUDE_DIRS)
set(3RDPARTY_LIBRARIES)

enable_testing()
find_package(GTest REQUIRED)
list(APPEND 3RDPARTY_INCLUDE_DIRS "${GTEST_INCLUDE_DIRS}")
list(APPEND 3RDPARTY_LIBRARIES "${GTEST_BOTH_LIBRARIES}")


file(GLOB_RECURSE test_files
	"${CMAKE_CURRENT_SOURCE_DIR}/test.cpp"
)

add_executable(graph_test 
    ${test_files}
)
target_include_directories(
    graph_test
        PRIVATE
        ${3RDPARTY_INCLUDE_DIRS}
        "{CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
    graph_test
        ${3RDPARTY_LIBRARIES}
)

find_package(benchmark REQUIRED)
add_executable(bmark
   ${CMAKE_SOURCE_DIR}/benchmark.cpp
)
target_include_directories(bmark 
    PUBLIC
        "${GOOGLE_BENCHMARK_SRC}/include"
        "{CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(bmark 
    PUBLIC
        benchmark::benchmark
)
