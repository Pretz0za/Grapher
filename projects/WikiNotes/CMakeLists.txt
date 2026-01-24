cmake_minimum_required(VERSION 3.20)
project(wikinotes C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(wikinotes ./main.c ./src/helpers.c ./src/parser.c)

target_link_libraries(wikinotes
    PRIVATE
    /usr/local/lib/libraylib.a
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework IOKit"
    "-framework CoreVideo"
    "-framework CoreAudio"
    "-lm"
    /usr/local/lib/libgraphvis.a
)
