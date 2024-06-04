cmake_minimum_required(VERSION 3.25)
project(scanner)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

add_subdirectory(lib)
add_subdirectory(bin)
