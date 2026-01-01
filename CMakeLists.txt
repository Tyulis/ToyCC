cmake_minimum_required(VERSION 3.29)
project(toycc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Werror=switch -Werror=implicit-fallthrough")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clangd

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS program_options)

# Include the ANTLR runtime
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(ANTLR REQUIRED)

# Include third-party dependencies
add_subdirectory(lib/subprocess)
add_subdirectory(lib/antlr4/runtime/Cpp)


# Generate the parser
set(PARSER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/parser)
set(CLEXER_G4 ${CMAKE_CURRENT_SOURCE_DIR}/grammar/CLexer.g4)
set(CPARSER_G4 ${CMAKE_CURRENT_SOURCE_DIR}/grammar/CParser.g4)
antlr_target(CLexer ${CLEXER_G4}
             PACKAGE "toycc"
             OUTPUT_DIRECTORY ${PARSER_SRC}
             LEXER)
antlr_target(CParser ${CPARSER_G4}
             PACKAGE "toycc"
             OUTPUT_DIRECTORY ${PARSER_SRC}
             DEPENDS_ANTLR CLexer
             PARSER LISTENER)

# Add the CParserBase include in CParser.h
add_custom_target(CParserDependencies
                  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CParserDependencies.py ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/CParser.h
                  DEPENDS ${ANTLR_CParser_CXX_OUTPUTS}
                  COMMENT "Adding the CParserBase.h include in CParser.h")

# Copy the prologues
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

# Compile the app
add_executable(${PROJECT_NAME}
	src/assembler.cpp
	src/diagnostic.cpp
	src/linker.cpp
	src/main.cpp
	src/parser.cpp
	src/preprocess.cpp
	src/source_map.cpp
	src/xml_output.cpp

	src/arch/codegen.cpp
	src/arch/datamodel.cpp
	src/arch/x86_64/datamodel.cpp
	src/arch/x86_64/exports.cpp
	src/arch/x86_64/global.cpp
	src/arch/x86_64/statements.cpp
	src/arch/x86_64/write.cpp

	src/ir/declaration.cpp
	src/ir/flow.cpp
	src/ir/scope.cpp
	src/ir/scopeframe.cpp
	src/ir/statement.cpp
	src/ir/type.cpp
	src/ir/type_expressions.cpp

	src/ir/generator/conversions.cpp
	src/ir/generator/declarations.cpp
	src/ir/generator/exports.cpp
	src/ir/generator/expressionresult.cpp
	src/ir/generator/expressions.cpp
	src/ir/generator/external.cpp
	src/ir/generator/literals.cpp
	src/ir/generator/state.cpp
	src/ir/generator/statements.cpp
	src/ir/generator/symbols.cpp
	src/ir/generator/temporarygenerator.cpp

	src/ir/postprocessor/descope.cpp
	src/ir/postprocessor/detype.cpp
	src/ir/postprocessor/exports.cpp
	src/ir/postprocessor/flow_analysis.cpp
	src/ir/postprocessor/state.cpp

	src/parserbase/CParserBase.cpp
	src/parserbase/SymbolTable.cpp

	src/util/log.cpp
	src/util/strings.cpp
	src/util/tempfile.cpp

	${ANTLR_CLexer_CXX_OUTPUTS}
	${ANTLR_CParser_CXX_OUTPUTS})

add_dependencies(${PROJECT_NAME} CParserDependencies)

target_include_directories(${PROJECT_NAME} PRIVATE
	src/
	lib/subprocess/src/cpp
	${Boost_INCLUDE_DIRS}
	${libantlrcpp_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
	subprocess
	antlr4_static
	Boost::program_options)
