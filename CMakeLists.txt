cmake_minimum_required(VERSION 3.20)
project(Grapher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Your graph library (static)
add_library(graphvis STATIC 
    src/dsa/gvizGraph.c
    src/dsa/gvizTree.c
    src/dsa/gvizArray.c
    src/dsa/gvizDeque.c

    src/renderer/embeddings/gvizEmbeddedGraph.c
    src/renderer/embeddings/gvizEmbeddedTree.c
    src/renderer/embeddings/gvizGRIPEmbedding.c
    src/renderer/embeddings/gvizForceDirected.c

    src/renderer/layers/gvizLayer.c
    src/renderer/layers/gvizLayerGraph.c


    src/utils/serializers.c

    src/helpers.c
)

target_include_directories(graphvis 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/
    PRIVATE /usr/local/include # raylib
)

target_link_libraries(graphvis
    PRIVATE
    /usr/local/lib/libraylib.a
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework CoreAudio"
    "-lm"
)

# Tests
add_executable(gvizArrayTests third-party/unity/unity.c tests/dsa/gvizArrayTests.c)
add_executable(gvizGraphTests third-party/unity/unity.c tests/dsa/gvizGraphTests.c)
add_executable(gvizBitArrayTests third-party/unity/unity.c tests/dsa/gvizBitArrayTests.c)
add_executable(gvizDequeTests third-party/unity/unity.c tests/dsa/gvizDequeTests.c)


add_executable(gvizGRIPTests third-party/unity/unity.c tests/renderer/GRIP/gvizGRIPTests.c)

add_executable(treeDemo tests/renderer/treeDemo.c)
add_executable(gripDemo tests/renderer/gripDemo.c)

target_compile_definitions(gvizArrayTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizGraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizBitArrayTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(gvizDequeTests PRIVATE UNITY_INCLUDE_DOUBLE)

target_compile_definitions(gvizGRIPTests PRIVATE UNITY_INCLUDE_DOUBLE)

set_target_properties(gvizArrayTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizGraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizBitArrayTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)
set_target_properties(gvizDequeTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/dsa)

set_target_properties(gvizGRIPTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/renderer/GRIP)

set_target_properties(treeDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/renderer/)
set_target_properties(gripDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/renderer/)

target_link_libraries(gvizArrayTests PRIVATE graphvis)
target_link_libraries(gvizGraphTests PRIVATE graphvis)
target_link_libraries(gvizBitArrayTests PRIVATE graphvis)
target_link_libraries(gvizDequeTests PRIVATE graphvis)

target_link_libraries(gvizGRIPTests PRIVATE graphvis)

target_link_libraries(treeDemo PRIVATE graphvis)
target_link_libraries(gripDemo PRIVATE graphvis)

