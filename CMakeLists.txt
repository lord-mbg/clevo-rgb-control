cmake_minimum_required(VERSION 3.16)
project(clevo_rgb_control LANGUAGES CXX)

add_executable(clevo-rgb src/main.cpp)
target_compile_features(clevo-rgb PRIVATE cxx_std_17)
target_compile_options(clevo-rgb PRIVATE -Wall -Wextra -Wpedantic)
