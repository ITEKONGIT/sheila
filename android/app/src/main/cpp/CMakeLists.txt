cmake_minimum_required(VERSION 3.24)

project(sheila_android LANGUAGES CXX)

add_library(sheila_android SHARED native_bridge.cpp)

target_compile_features(sheila_android PRIVATE cxx_std_20)
target_compile_options(sheila_android PRIVATE -Wall -Wextra -Wpedantic)

target_link_options(sheila_android PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
)
