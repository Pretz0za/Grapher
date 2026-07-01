cmake_minimum_required(VERSION 3.20)
project(Grapher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

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
    src/dsa/gvizGraph.c
    src/dsa/gvizTree.c
    src/dsa/gvizArray.c
    src/dsa/gvizDeque.c
    src/dsa/gvizBitArray.c
    src/dsa/gvizSubgraph.c

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
)

# DSA tests
add_executable(gvizArrayTests third-party/unity/unity.c tests/dsa/gvizArrayTests.c)
add_executable(gvizGraphTests third-party/unity/unity.c tests/dsa/gvizGraphTests.c)
add_executable(gvizBitArrayTests third-party/unity/unity.c tests/dsa/gvizBitArrayTests.c)
add_executable(gvizDequeTests third-party/unity/unity.c tests/dsa/gvizDequeTests.c)
add_executable(gvizSubgraphTests third-party/unity/unity.c tests/dsa/gvizSubgraphTests.c)

# Embedder tests
add_executable(gvizGRIPTests third-party/unity/unity.c tests/embedders/gvizGRIPTests.c)
add_executable(gvizPlanarTests third-party/unity/unity.c tests/embedders/gvizPlanarTests.c)
add_executable(gvizTutteTests third-party/unity/unity.c tests/embedders/gvizTutteTests.c)

target_compile_definitions(gvizArrayTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizGraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizBitArrayTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizDequeTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizSubgraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizGRIPTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizPlanarTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizTutteTests PRIVATE UNITY_INCLUDE_DOUBLE)

set_target_properties(gvizArrayTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizGraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizBitArrayTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizDequeTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizSubgraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)

set_target_properties(gvizGRIPTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)
set_target_properties(gvizPlanarTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)
set_target_properties(gvizTutteTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/embedders)

target_link_libraries(gvizArrayTests PRIVATE graphvis)
target_link_libraries(gvizGraphTests PRIVATE graphvis)
target_link_libraries(gvizBitArrayTests PRIVATE graphvis)
target_link_libraries(gvizDequeTests PRIVATE graphvis)
target_link_libraries(gvizSubgraphTests PRIVATE graphvis)

target_link_libraries(gvizGRIPTests PRIVATE graphvis)
target_link_libraries(gvizPlanarTests PRIVATE graphvis)
target_link_libraries(gvizTutteTests PRIVATE graphvis)

enable_testing()
add_test(NAME gvizArrayTests COMMAND gvizArrayTests)
add_test(NAME gvizGraphTests COMMAND gvizGraphTests)
add_test(NAME gvizBitArrayTests COMMAND gvizBitArrayTests)
add_test(NAME gvizDequeTests COMMAND gvizDequeTests)
add_test(NAME gvizSubgraphTests COMMAND gvizSubgraphTests)
add_test(NAME gvizGRIPTests COMMAND gvizGRIPTests)
add_test(NAME gvizPlanarTests COMMAND gvizPlanarTests)
add_test(NAME gvizTutteTests COMMAND gvizTutteTests)
