cmake_minimum_required(VERSION 3.5)
project(fast_obj)

add_library(fast_obj include/fast_obj.h fast_obj.c)
target_include_directories(fast_obj PUBLIC include/)