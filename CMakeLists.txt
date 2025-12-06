cmake_minimum_required(VERSION 3.29.0)
project(toycc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-g -Wall")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clangd

add_subdirectory(lib/cppparser)

# Compile the app
add_executable(${PROJECT_NAME}
	src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE lib/cppparser/cppast/include lib/cppparser/cppparser/include lib/cppparser/cppwriter/include)
target_link_libraries(${PROJECT_NAME} PRIVATE cppast cppparser cppwriter)
