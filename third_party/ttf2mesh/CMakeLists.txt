cmake_minimum_required(VERSION 3.5)
project(ttf2mesh)

add_library(ttf2mesh include/ttf2mesh.h ttf2mesh.c)
target_include_directories(ttf2mesh PUBLIC include/)