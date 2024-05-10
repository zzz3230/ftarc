cmake_minimum_required(VERSION 3.26)
project(ftarc C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_compile_options(
        $<$<CONFIG:RELEASE>:-Ofast>
        $<$<CONFIG:DEBUG>:-O0>
)

add_executable(ftarc main.c
        utilities/graph.c
        compression_algorithms/huffman.c
        compression_algorithms/huffman.h
        archiver.c
        archiver.h
        utilities/ustring.c
        utilities/ustring.h
        utilities/graph.h
        utilities/heap.c
        utilities/heap.h
        utilities/colorer.h
        utilities/utils.h
        utilities/collections.c
        utilities/collections.h
        utilities/utils.c
        utilities/md5.c
        utilities/md5.h
        archive_structs.h
        exceptions.c
        exceptions.h
        args_parser.c
        args_parser.h
        progress_display.c
        progress_display.h
)
