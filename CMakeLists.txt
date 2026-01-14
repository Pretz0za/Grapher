cmake_minimum_required(VERSION 3.20)
project(Grapher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Your graph library (static)
add_library(graphvis STATIC 
    src/graph.c
    src/graphvis.c
    src/stack.c
    src/helpers.c
)

target_include_directories(graphvis 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/graph.h
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/graphvis.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/helpers.h
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

add_executable(graph_demo tests/dfs.c)
target_link_libraries(graph_demo PRIVATE graphvis)
