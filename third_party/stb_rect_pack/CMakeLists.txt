cmake_minimum_required(VERSION 3.5)
project(stb_rect_pack)

add_library(stb_rect_pack include/stb_rect_pack.h stb_rect_pack.c)
target_include_directories(stb_rect_pack PUBLIC include/)