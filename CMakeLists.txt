cmake_minimum_required(VERSION 3.8)
project("hybrid-renderer" VERSION 1.0.0)

# inlude & build renderer lib
add_subdirectory("hybrid-renderer-lib")

# include & build demo project
add_subdirectory("demo")
