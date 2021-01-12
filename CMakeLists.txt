cmake_minimum_required(VERSION 3.5)
project(Arts-Bench)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -march=native -fopenmp -o3")

include_directories(
        ${GTEST_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

file(GLOB INCLUDE_H "include/*.cpp" "include/*.h")


add_executable(main ${INCLUDE_H} main.cpp)

set(BENCH_FILES benchmark/bench.cpp)
add_executable(bench ${INCLUDE_H} ${BENCH_FILES})
target_include_directories(bench
        PRIVATE Threads::Threads)

set(ART_BENCH_FILES benchmark/art_bench.cpp)
add_executable(art-bench ${INCLUDE_H} ${ART_BENCH_FILES})
target_include_directories(art-bench
        PRIVATE Threads::Threads)

set(DPGM_BENCH_FILES benchmark/dpgm_bench.cpp)
add_executable(dpgm-bench ${DPGM_BENCH_FILES})
target_include_directories(dpgm-bench
        PRIVATE Threads::Threads
        PRIVATE "benchmark/PGM-index/include")

set(PGM_BENCH_FILES benchmark/pgm_bench.cpp)
add_executable(pgm-bench ${PGM_BENCH_FILES})
target_include_directories(pgm-bench
        PRIVATE Threads::Threads
        PRIVATE "benchmark/PGM-index/include")

set(STX_BENCH_FILES benchmark/btree_bench.cpp)
add_executable(stx-bench ${STX_BENCH_FILES} benchmark/tracking_allocator.h)
target_include_directories(stx-bench
        PRIVATE Threads::Threads
        PRIVATE "benchmark/stx-btree-0.9/include")

set(ALEX_BENCH_FILES benchmark/alex_bench.cpp)
add_executable(alex-bench ${ALEX_BENCH_FILES} benchmark/tracking_allocator.h)
target_include_directories(alex-bench
        PRIVATE Threads::Threads
        PRIVATE "benchmark/ALEX/src/core")

set(DIFF_MAX_ERROR_FILES benchmark/diff_max_error_experiment.cpp)
add_executable(diff-max-error-exp ${INCLUDE_H} ${DIFF_MAX_ERROR_FILES})
target_include_directories(diff-max-error-exp
        PRIVATE Threads::Threads)

set(ADAP_EXP_FILES benchmark/adaptive_experiment.cpp)
add_executable(adap-exp ${INCLUDE_H} ${ADAP_EXP_FILES})
target_include_directories(adap-exp
        PRIVATE Threads::Threads)
