cmake_minimum_required(VERSION 3.20)
project(Grapher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

find_package(Threads REQUIRED)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

add_library(planar STATIC
    third-party/boyerMyrvold/graphEmbed.c
    third-party/boyerMyrvold/graphIO.c
    third-party/boyerMyrvold/graphIsolator.c
    third-party/boyerMyrvold/graphNonplanar.c
    third-party/boyerMyrvold/graphPreprocess.c
    third-party/boyerMyrvold/graphStructure.c
    third-party/boyerMyrvold/listcoll.c
    third-party/boyerMyrvold/stack.c
)
target_include_directories(planar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/)

add_library(graphvis STATIC
    src/core/gvizThreadPool.c

    src/ds/gvizGraph.c
    src/ds/gvizTree.c
    src/ds/gvizArray.c
    src/ds/gvizDeque.c
    src/ds/gvizBitArray.c
    src/ds/gvizSubgraph.c

    src/algorithms/search/gvizBreadthFirst.c
    src/algorithms/search/gvizDepthFirst.c
    src/algorithms/search/gvizKNearest.c

    src/embedders/gvizEmbeddedGraph.c
    src/embedders/gvizEmbeddedTree.c
    src/embedders/gvizGRIPEmbedder.c
    src/embedders/gvizForceDirected.c
    src/embedders/gvizPlanarEmbedder.c
    src/embedders/gvizSchnyderWood.c
    src/embedders/gvizTutteEmbedder.c

    src/utils/graphs.c
    src/utils/serializers.c
    src/utils/helpers.c
)

target_include_directories(graphvis
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/
)

target_link_libraries(graphvis
    PRIVATE
    planar
    openblas
    m
    Threads::Threads
)

# Core tests
add_executable(gvizThreadPoolTests third-party/unity/unity.c tests/core/gvizThreadPoolTests.c)

# DS tests
add_executable(gvizArrayTests third-party/unity/unity.c tests/ds/gvizArrayTests.c)
add_executable(gvizGraphTests third-party/unity/unity.c tests/ds/gvizGraphTests.c)
add_executable(gvizBitArrayTests third-party/unity/unity.c tests/ds/gvizBitArrayTests.c)
add_executable(gvizDequeTests third-party/unity/unity.c tests/ds/gvizDequeTests.c)
add_executable(gvizSubgraphTests third-party/unity/unity.c tests/ds/gvizSubgraphTests.c)

# Algorithm tests
add_executable(gvizSearchTests third-party/unity/unity.c tests/algorithms/search/gvizSearchTests.c)

# Embedder tests
add_executable(gvizEmbeddedGraphTests third-party/unity/unity.c tests/embedders/gvizEmbeddedGraphTests.c)
add_executable(gvizGRIPTests third-party/unity/unity.c tests/embedders/gvizGRIPTests.c)
add_executable(gvizGRIPKBench tests/embedders/gvizGRIPKBench.c)
add_executable(gvizPlanarTests third-party/unity/unity.c tests/embedders/gvizPlanarTests.c)
add_executable(gvizTutteTests third-party/unity/unity.c tests/embedders/gvizTutteTests.c)

target_compile_definitions(gvizThreadPoolTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizArrayTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizGraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizBitArrayTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizDequeTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizSubgraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizSearchTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizEmbeddedGraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizGRIPTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizPlanarTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizTutteTests PRIVATE UNITY_INCLUDE_DOUBLE)

set_target_properties(gvizThreadPoolTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/core)
set_target_properties(gvizArrayTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(gvizGraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(gvizBitArrayTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(gvizDequeTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(gvizSubgraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)

set_target_properties(gvizSearchTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/algorithms/search)

set_target_properties(gvizEmbeddedGraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)
set_target_properties(gvizGRIPKBench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)
set_target_properties(gvizGRIPTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)
set_target_properties(gvizPlanarTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)
set_target_properties(gvizTutteTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)

target_link_libraries(gvizThreadPoolTests PRIVATE graphvis)
target_link_libraries(gvizArrayTests PRIVATE graphvis)
target_link_libraries(gvizGraphTests PRIVATE graphvis)
target_link_libraries(gvizBitArrayTests PRIVATE graphvis)
target_link_libraries(gvizDequeTests PRIVATE graphvis)
target_link_libraries(gvizSubgraphTests PRIVATE graphvis)
target_link_libraries(gvizSearchTests PRIVATE graphvis)

target_link_libraries(gvizEmbeddedGraphTests PRIVATE graphvis)
target_link_libraries(gvizGRIPKBench PRIVATE graphvis)
target_link_libraries(gvizGRIPTests PRIVATE graphvis)
target_link_libraries(gvizPlanarTests PRIVATE graphvis)
target_link_libraries(gvizTutteTests PRIVATE graphvis)

enable_testing()
add_test(NAME gvizThreadPoolTests COMMAND gvizThreadPoolTests)
add_test(NAME gvizArrayTests COMMAND gvizArrayTests)
add_test(NAME gvizGraphTests COMMAND gvizGraphTests)
add_test(NAME gvizBitArrayTests COMMAND gvizBitArrayTests)
add_test(NAME gvizDequeTests COMMAND gvizDequeTests)
add_test(NAME gvizSubgraphTests COMMAND gvizSubgraphTests)
add_test(NAME gvizSearchTests COMMAND gvizSearchTests)
add_test(NAME gvizEmbeddedGraphTests COMMAND gvizEmbeddedGraphTests)
add_test(NAME gvizGRIPTests COMMAND gvizGRIPTests)
add_test(NAME gvizPlanarTests COMMAND gvizPlanarTests)
add_test(NAME gvizTutteTests COMMAND gvizTutteTests)
