cmake_minimum_required(VERSION 3.29)
project(toycc VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -Werror=switch -Werror=implicit-fallthrough -DWITH_COMMENT_TRACE")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # For clangd

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Boost CONFIG REQUIRED COMPONENTS program_options)
find_package(Armadillo REQUIRED)

# Include the ANTLR runtime
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(ANTLR REQUIRED)

# Include third-party dependencies
add_subdirectory(lib/subprocess)
add_subdirectory(lib/antlr4/runtime/Cpp)

# Generate the parser
set(PARSER_SRC ${CMAKE_CURRENT_BINARY_DIR}/gen/parser)
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
                  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/CParserDependencies.py ${CMAKE_CURRENT_BINARY_DIR}/gen/parser/CParser.h
                  DEPENDS ${ANTLR_CParser_CXX_OUTPUTS}
                  COMMENT "Adding the CParserBase.h include in CParser.h")

# Generate the translation model
execute_process(COMMAND python3 scripts/execmodel-x86_64.py ${CMAKE_CURRENT_BINARY_DIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE GENERATED_EXECMODEL_SOURCES)
string(STRIP ${GENERATED_EXECMODEL_SOURCES} GENERATED_EXECMODEL_SOURCES)
separate_arguments(GENERATED_EXECMODEL_SOURCES)

# Copy the prologues
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

# Compile the app
add_executable(${PROJECT_NAME}
	src/assembler.cpp
	src/config.cpp
	src/diagnostic.cpp
	src/linker.cpp
	src/main.cpp
	src/parser.cpp
	src/preprocess.cpp
	src/source_map.cpp
	src/xml_output.cpp

	src/arch/codegen.cpp
	src/arch/datamodel.cpp
	src/arch/x86_64/allocation.cpp
	src/arch/x86_64/assembly.cpp
	src/arch/x86_64/block.cpp
	src/arch/x86_64/custom_targets.cpp
	src/arch/x86_64/datamodel.cpp
	src/arch/x86_64/execmodel.cpp
	src/arch/x86_64/exports.cpp
	src/arch/x86_64/global.cpp
	src/arch/x86_64/output.cpp
	src/arch/x86_64/symbols.cpp

	src/ir/declaration.cpp
	src/ir/flow.cpp
	src/ir/scope.cpp
	src/ir/scopeframe.cpp
	src/ir/statement.cpp
	src/ir/type.cpp
	src/ir/type_expressions.cpp

	src/semantic/conversions.cpp
	src/semantic/declarations.cpp
	src/semantic/exports.cpp
	src/semantic/expressionresult.cpp
	src/semantic/expressions.cpp
	src/semantic/external.cpp
	src/semantic/literals.cpp
	src/semantic/state.cpp
	src/semantic/statements.cpp
	src/semantic/symbols.cpp
	src/semantic/temporarygenerator.cpp
	src/semantic/values.cpp

	src/ir/postprocessor/dereference.cpp
	src/ir/postprocessor/descope.cpp
	src/ir/postprocessor/detype.cpp
	src/ir/postprocessor/exports.cpp
	src/ir/postprocessor/state.cpp

	src/parser/CParserBase.cpp
	src/parser/SymbolTable.cpp

	src/util/log.cpp
	src/util/strings.cpp
	src/util/tempfile.cpp

	${ANTLR_CLexer_CXX_OUTPUTS}
	${ANTLR_CParser_CXX_OUTPUTS}

	${GENERATED_EXECMODEL_SOURCES})

add_dependencies(${PROJECT_NAME} CParserDependencies)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
	src/
	lib/subprocess/src/cpp
	${Boost_INCLUDE_DIRS}
	${libantlrcpp_INCLUDE_DIRS}
	${ARMADILLO_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
	subprocess
	antlr4_static
	${ARMADILLO_LIBRARIES}
	Boost::program_options)
