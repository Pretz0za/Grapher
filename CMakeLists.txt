cmake_minimum_required(VERSION 3.20)
project(gviz CXX C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

find_package(Threads REQUIRED)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# gviz (and everything it statically links, e.g. `planar` below) builds as a
# shared library, so every object file needs position-independent code even
# on platforms where that isn't the default for static archives.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Vendored Boyer-Myrvold planarity testing (stays C, linked into gviz).
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

add_library(gviz SHARED
    # core/
    src/ThreadPool.cpp

    # ds/
    src/Graph.cpp
    src/Subgraph.cpp
    src/QuadTree.cpp

    # search/
    src/BreadthFirst.cpp
    src/DepthFirst.cpp
    src/ConnectedComponents.cpp
    src/Tree.cpp
    src/KNearest.cpp

    # layout/
    src/EmbeddedGraph.cpp
    src/ForceAtlas.cpp
    src/GRIP.cpp
    src/ReingoldTilford.cpp
    src/Planar.cpp
    src/SchnyderWood.cpp
    src/Tutte.cpp
    src/SpringTutte.cpp

    # io/ + graphs/
    src/GraphLoader.cpp
    src/Graphs.cpp
)

target_include_directories(gviz
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/
)

set_target_properties(gviz PROPERTIES
    VERSION 0.1.0
    SOVERSION 0
    # MSVC/MinGW shared libs need explicit exports; auto-export every symbol
    # rather than hand-annotating ~20 public headers with __declspec. No
    # effect on non-Windows targets.
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(gviz
    PRIVATE
    planar
    m
    Threads::Threads
)

# Smoke test: full gviz.hpp umbrella header compiles cleanly, exception
# hierarchy behaves as documented.
add_executable(gvizCxxSmokeTest third-party/unity/unity.c tests/gvizCxxSmokeTest.cpp)
target_include_directories(gvizCxxSmokeTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/unity)

# core/: ThreadPool, Vec helpers
add_executable(ThreadPoolTests third-party/unity/unity.c tests/ThreadPoolTests.cpp)
add_executable(VecTests third-party/unity/unity.c tests/VecTests.cpp)

# ds/: Graph, Subgraph, BitSet, QuadTree
add_executable(BitSetTests third-party/unity/unity.c tests/BitSetTests.cpp)
add_executable(GraphTests third-party/unity/unity.c tests/GraphTests.cpp)
add_executable(SubgraphTests third-party/unity/unity.c tests/SubgraphTests.cpp)
add_executable(QuadTreeTests third-party/unity/unity.c tests/QuadTreeTests.cpp)

# search/: BreadthFirst, DepthFirst, ConnectedComponents,
# Tree (IsTree/IsLeaf/CountLeaves), KNearest
add_executable(SearchTests third-party/unity/unity.c tests/SearchTests.cpp)
add_executable(TreeTests third-party/unity/unity.c tests/TreeTests.cpp)
add_executable(KNearestTests third-party/unity/unity.c tests/KNearestTests.cpp)

# layout/: ForceModel, EmbeddedGraph, ForceAtlas, GRIP, ReingoldTilford,
# Planar, SchnyderWood, Tutte, SpringTutte
add_executable(ForceModelTests third-party/unity/unity.c tests/ForceModelTests.cpp)
add_executable(EmbeddedGraphTests third-party/unity/unity.c tests/EmbeddedGraphTests.cpp)
add_executable(ForceAtlasTests third-party/unity/unity.c tests/ForceAtlasTests.cpp)
add_executable(GRIPTests third-party/unity/unity.c tests/GRIPTests.cpp)
add_executable(ReingoldTilfordTests third-party/unity/unity.c tests/ReingoldTilfordTests.cpp)
add_executable(PlanarTests third-party/unity/unity.c tests/PlanarTests.cpp)
add_executable(SchnyderWoodTests third-party/unity/unity.c tests/SchnyderWoodTests.cpp)
add_executable(TutteTests third-party/unity/unity.c tests/TutteTests.cpp)
add_executable(SpringTutteTests third-party/unity/unity.c tests/SpringTutteTests.cpp)

# io/ + graphs/: GraphLoader, Graphs (synthetic generators)
add_executable(GraphLoaderTests third-party/unity/unity.c tests/GraphLoaderTests.cpp)
add_executable(GraphsTests third-party/unity/unity.c tests/GraphsTests.cpp)

# Benchmarks and debug tools (not registered with ctest, except TuttePick
# which returns a real pass/fail exit code): standalone drivers exercising
# internals via layout/'s narrow debug-introspection accessors
# (GRIP::DebugXxx, GRIP::LayerBorder/FiltrationVertexAt/Displacement).
add_executable(GRIPKBench tests/GRIPKBench.cpp)
add_executable(GRIPLayerProbe tests/GRIPLayerProbe.cpp)
add_executable(GRIPMigrateBench tests/GRIPMigrateBench.cpp)
add_executable(GRIPFiltrationDebug tests/GRIPFiltrationDebug.cpp)
add_executable(GRIPPlaceBench tests/GRIPPlaceBench.cpp)
add_executable(TuttePick tests/TuttePick.cpp)

target_compile_definitions(ThreadPoolTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(VecTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(BitSetTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(GraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(SubgraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(QuadTreeTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(SearchTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(TreeTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(KNearestTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(ForceModelTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(EmbeddedGraphTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(ForceAtlasTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(GRIPTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(ReingoldTilfordTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(PlanarTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(SchnyderWoodTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(TutteTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(SpringTutteTests PRIVATE UNITY_INCLUDE_DOUBLE)
target_compile_definitions(GraphLoaderTests PRIVATE UNITY_INCLUDE_DOUBLE
    GVIZ_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
target_compile_definitions(GraphsTests PRIVATE UNITY_INCLUDE_DOUBLE)

set_target_properties(gvizCxxSmokeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set_target_properties(ThreadPoolTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set_target_properties(VecTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set_target_properties(BitSetTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(GraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(SubgraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(QuadTreeTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/ds)
set_target_properties(SearchTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/search)
set_target_properties(TreeTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/search)
set_target_properties(KNearestTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/search)
set_target_properties(ForceModelTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(EmbeddedGraphTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(ForceAtlasTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(GRIPTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(ReingoldTilfordTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(PlanarTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(SchnyderWoodTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(TutteTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(SpringTutteTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(GraphLoaderTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/utils)
set_target_properties(GraphsTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/utils)
set_target_properties(GRIPKBench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(GRIPLayerProbe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(GRIPMigrateBench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(GRIPFiltrationDebug PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(GRIPPlaceBench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)
set_target_properties(TuttePick PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/layout)

target_link_libraries(gvizCxxSmokeTest PRIVATE gviz)
target_link_libraries(ThreadPoolTests PRIVATE gviz)
target_link_libraries(VecTests PRIVATE gviz)
target_link_libraries(BitSetTests PRIVATE gviz)
target_link_libraries(GraphTests PRIVATE gviz)
target_link_libraries(SubgraphTests PRIVATE gviz)
target_link_libraries(QuadTreeTests PRIVATE gviz)
target_link_libraries(SearchTests PRIVATE gviz)
target_link_libraries(TreeTests PRIVATE gviz)
target_link_libraries(KNearestTests PRIVATE gviz)
target_link_libraries(ForceModelTests PRIVATE gviz)
target_link_libraries(EmbeddedGraphTests PRIVATE gviz)
target_link_libraries(ForceAtlasTests PRIVATE gviz)
target_link_libraries(GRIPTests PRIVATE gviz)
target_link_libraries(ReingoldTilfordTests PRIVATE gviz)
target_link_libraries(PlanarTests PRIVATE gviz)
target_link_libraries(SchnyderWoodTests PRIVATE gviz)
target_link_libraries(TutteTests PRIVATE gviz)
target_link_libraries(SpringTutteTests PRIVATE gviz)
target_link_libraries(GraphLoaderTests PRIVATE gviz)
target_link_libraries(GraphsTests PRIVATE gviz)
target_link_libraries(GRIPKBench PRIVATE gviz)
target_link_libraries(GRIPLayerProbe PRIVATE gviz)
target_link_libraries(GRIPMigrateBench PRIVATE gviz)
target_link_libraries(GRIPFiltrationDebug PRIVATE gviz)
target_link_libraries(GRIPPlaceBench PRIVATE gviz)
target_link_libraries(TuttePick PRIVATE gviz)

enable_testing()
add_test(NAME gvizCxxSmokeTest COMMAND gvizCxxSmokeTest)
add_test(NAME ThreadPoolTests COMMAND ThreadPoolTests)
add_test(NAME VecTests COMMAND VecTests)
add_test(NAME BitSetTests COMMAND BitSetTests)
add_test(NAME GraphTests COMMAND GraphTests)
add_test(NAME SubgraphTests COMMAND SubgraphTests)
add_test(NAME QuadTreeTests COMMAND QuadTreeTests)
add_test(NAME SearchTests COMMAND SearchTests)
add_test(NAME TreeTests COMMAND TreeTests)
add_test(NAME KNearestTests COMMAND KNearestTests)
add_test(NAME ForceModelTests COMMAND ForceModelTests)
add_test(NAME EmbeddedGraphTests COMMAND EmbeddedGraphTests)
add_test(NAME ForceAtlasTests COMMAND ForceAtlasTests)
add_test(NAME GRIPTests COMMAND GRIPTests)
add_test(NAME ReingoldTilfordTests COMMAND ReingoldTilfordTests)
add_test(NAME PlanarTests COMMAND PlanarTests)
add_test(NAME SchnyderWoodTests COMMAND SchnyderWoodTests)
add_test(NAME TutteTests COMMAND TutteTests)
add_test(NAME SpringTutteTests COMMAND SpringTutteTests)
add_test(NAME GraphLoaderTests COMMAND GraphLoaderTests)
add_test(NAME GraphsTests COMMAND GraphsTests)
add_test(NAME TuttePick COMMAND TuttePick)
