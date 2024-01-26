cmake_minimum_required(VERSION 3.5)
project(stb_truetype)

add_library(stb_truetype include/stb_truetype.h stb_truetype.c)
target_include_directories(stb_truetype PUBLIC include/)