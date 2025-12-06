cmake_minimum_required(VERSION 3.29.0)
project(toycc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clangd

add_subdirectory(lib/subprocess)

# Compile the app
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/preprocess.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE lib/subprocess/src/cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE subprocess)
