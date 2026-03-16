cmake_minimum_required(VERSION 3.20)
project(Grapher C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Your graph library (static)
add_library(graphvis STATIC 
    src/dsa/graph.c
    src/graphvis.c
    src/dsa/uLongArray.c
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

add_executable(ulArrayTests third-party/unity/unity.c tests/dsa/ulArrayTests.c)
target_link_libraries(ulArrayTests PRIVATE graphvis)
