# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(MyFirstProject)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(my_app
    src/main.cpp 
    src/utils.cpp 
)

target_include_directories(my_app PRIVATE src include)
