cmake_minimum_required(VERSION 3.10.2)

set(cmake_cxx_standard 20)

add_library(native-lib SHARED
    src/main/cpp/sample_conversion.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")