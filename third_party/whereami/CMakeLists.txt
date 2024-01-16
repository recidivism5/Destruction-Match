cmake_minimum_required(VERSION 3.0)
project(whereami)

add_library(whereami include/whereami.h whereami.c)
target_include_directories(whereami PUBLIC include/)