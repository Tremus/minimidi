cmake_minimum_required(VERSION 3.7)
project(minimidi)
set(CMAKE_C_STANDARD 90)

add_library(minimidi STATIC minimidi.c)

add_executable(example_minimidi minimidi_example.c)
if(APPLE)
    target_link_libraries(example_minimidi PRIVATE "-framework CoreMIDI -framework CoreAudio -framework Foundation")
endif()
target_compile_options(example_minimidi PRIVATE -Wno-nullability-completeness)