cmake_minimum_required(VERSION 3.29)
project(toycc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clangd

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Boost CONFIG REQUIRED
             COMPONENTS program_options)

# Include the ANTLR runtime
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(ANTLR REQUIRED)

# Include third-party dependencies
add_subdirectory(lib/subprocess)
add_subdirectory(lib/antlr4/runtime/Cpp)


# Generate the parser
set(PARSER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/parser)
set(GRAMMAR_G4 ${CMAKE_CURRENT_SOURCE_DIR}/grammar/C.g4)
antlr_target(CParser ${GRAMMAR_G4}
             PACKAGE "toycc::parser"
             OUTPUT_DIRECTORY ${PARSER_SRC}
             LEXER PARSER LISTENER)

# include_directories(${ANTLR_CParser_OUTPUT_DIR})

# Compile the app
add_executable(${PROJECT_NAME}
	src/diagnostic.cpp
	src/main.cpp
	src/parser.cpp
	src/preprocess.cpp
	src/source_map.cpp

	src/util/strings.cpp

	${ANTLR_CParser_CXX_OUTPUTS})

target_include_directories(${PROJECT_NAME} PRIVATE
	src/
	lib/subprocess/src/cpp
	${Boost_INCLUDE_DIRS}
	${libantlrcpp_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
	subprocess
	antlr4_static
	${Boost_LIBRARIES})
