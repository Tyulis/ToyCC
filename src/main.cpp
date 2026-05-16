#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

#include "config.h"
#include "debug/dwarf.h"
#include "linker.h"
#include "parser.h"
#include "assembler.h"
#include "source_map.h"
#include "diagnostic.h"
#include "preprocess.h"
#include "ir/scope.h"
#include "ir/postprocessor/postprocessor.h"

#include "util/log.h"
#include "util/strings.h"
#include "util/tempfile.h"

#include "arch/datamodel.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/datamodel.h"

enum class SequenceStep : unsigned int {
    NONE = 0,
    PREPROCESS = 1,
    SOURCE_MAP = 2,
    PARSE = 3,
    POSTPROCESS = 4,
    FLOW = 5,
    CODEGEN = 6,
    ASSEMBLY = 7,
    LINK = 8,
};

constexpr static std::string DEFAULT_OBJECT_FILE_NAME = "a.out";

void read_config(const boost::program_options::variables_map& options) {
    // Optimization options
    if (options.count("fsplit-intermediates"))
        toycc::config::optimization::split_intermediates = true;
    else if (options.count("fno-split-intermediates"))
        toycc::config::optimization::split_intermediates = false;

    // Debug options
    if (options.count("debug"))
        toycc::config::debug::enable = true;
    if (options.count("gdwarf-32")) {
        toycc::config::debug::enable = true;
        toycc::config::debug::format = toycc::debug::DWARFFormat::DWARF32;
    } else if (options.count("gdwarf-64")) {
        toycc::config::debug::enable = true;
        toycc::config::debug::format = toycc::debug::DWARFFormat::DWARF64;
    }
    if (options.count("gdefault-location"))
        toycc::config::debug::with_default_location = true;

    // Developer options
    if (options.count("vcomment-trace"))
        toycc::config::dev::with_comment_trace = true;
    if (options.count("vlocation-trace"))
        toycc::config::dev::with_location_trace = true;
    if (options.count("vtranslation-trace"))
        toycc::config::dev::with_translation_trace = true;

    // Config dump
    if (options.count("vdump-config")) {
        std::cerr << "Compiler configuration :" << std::endl;
        std::cerr << toycc::indent(toycc::config::dump(), "    ") << std::endl << std::endl;
    }
}

int main(int argc, char** argv) {
    toycc::arch::DATAMODEL = &toycc::arch::x86_64::DATAMODEL;  // Only x86_64 for now

    boost::program_options::options_description generic_options("Generic options");
    generic_options.add_options()("help,h",        "Show this help message")
                                 ("output,o",      boost::program_options::value<std::string>(), "Output file name")
                                 ("source-file,f", boost::program_options::value<std::string>(), "Source files");

    boost::program_options::options_description sequence_options("Sequence options");
    sequence_options.add_options()("preprocess,E",   "Only preprocess the source code")
                                  ("source-map",     "Preprocess and annotate the source lines")
                                  ("parse-lisp",     "Output the AST as Lisp")
                                  ("parse-xml",      "Output the AST as XML")
                                  ("parse-ir",       "Output the intermediate representation after semantic analysis")
                                  ("process-ir",     "Output the postprocessed intermediate representation")
                                  ("flow",           "Output the flow graph")
                                  ("codegen,S",      "Output the generated assembly code")
                                  ("compile,c",      "Compile to an object file");

    boost::program_options::options_description debug_options("Debug options");
    debug_options.add_options()("debug,g",           "Emit debugging information in DWARF5 format")
                               ("gdwarf-32",         "Force the 32-bits DWARF format (default)")
                               ("gdwarf-64",         "Force the 64-bits DWARF format (currently has issues because GAS only generates DWARF-32)")
                               ("gdefault-location", "Emit more reliable default location entries (not supported by current GDB)");

    boost::program_options::options_description optimization_options("Optimization options");
    optimization_options.add_options()("fsplit-intermediates",    "Split intermediate values in basic blocks")
                                      ("fno-split-intermediates", "Don't split intermediate values in basic blocks");

    boost::program_options::options_description dev_options("Compiler developer options");
    dev_options.add_options()("vtranslation-trace", "Log all translation model steps")
                             ("vcomment-trace",     "Add comments with the translation process in the assembly code output")
                             ("vlocation-trace",    "Add variable movements to the location trace")
                             ("vdump-config",       "Begin by printing the compiler config");

    boost::program_options::options_description all_options;
    all_options.add(generic_options).add(sequence_options).add(optimization_options).add(debug_options).add(dev_options);

    boost::program_options::positional_options_description positional;
    positional.add("source-file", 1);

    boost::program_options::variables_map options;

    try {
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(all_options).positional(positional).run(), options);
        boost::program_options::notify(options);
    } catch (boost::program_options::error const& exc) {
        std::cerr << exc.what() << std::endl;
        std::cout << all_options << std::endl;
        return 1;
    }

    if (options.count("help") || options.count("source-file") < 1) {
        std::cout << "Usage : " << argv[0] << " [options] <source-file>\n" << std::endl;
        std::cout << all_options << std::endl;
        return 1;
    }

    read_config(options);

    SequenceStep target_step = SequenceStep::LINK;
    if (options.count("preprocess"))
        target_step = SequenceStep::PREPROCESS;
    if (options.count("source-map"))
        target_step = SequenceStep::SOURCE_MAP;
    if (options.count("parse-lisp") || options.count("parse-xml") || options.count("parse-ir"))
        target_step = SequenceStep::PARSE;
    if (options.count("process-ir"))
        target_step = SequenceStep::POSTPROCESS;
    if (options.count("flow"))
        target_step = SequenceStep::FLOW;
    if (options.count("codegen"))
        target_step = SequenceStep::CODEGEN;
    if (options.count("compile"))
        target_step = SequenceStep::ASSEMBLY;

    if (target_step == SequenceStep::NONE)
        return 0;

    std::reference_wrapper<std::ostream> output_stream = std::cout;
    std::ofstream output_file;
    if (options.count("output")) {
        const std::string output_file_name = options["output"].as<std::string>();
        output_file = std::ofstream(output_file_name);
        if (!output_file.is_open()) {
            std::cerr << std::format("Can't open output file {}", output_file_name) << std::endl;
            return 1;
        }
        output_stream = std::reference_wrapper(output_file);
    }

    const std::string input_file_name = options["source-file"].as<std::string>();

    try {
        // -------- Preprocessing
        std::stringstream preprocessed_code;
        std::ostream& preprocessing_output = (target_step == SequenceStep::PREPROCESS? output_stream : preprocessed_code);
        toycc::preprocess(input_file_name, preprocessing_output);

        if (target_step == SequenceStep::PREPROCESS)
            return 0;

        // -------- Source map
        std::stringstream stripped_code;
        toycc::SourceMap source_map(preprocessed_code, stripped_code);

        if (target_step == SequenceStep::SOURCE_MAP) {
            source_map.annotate(stripped_code, output_stream);
            return 0;
        }

        // -------- Parsing
        stripped_code.seekg(0);
        toycc::Parser parser(stripped_code, source_map);

        if (target_step == SequenceStep::PARSE) {
            if (options.count("parse-lisp"))
                output_stream.get() << parser.to_lisp() << std::endl;
            else if (options.count("parse-xml"))
                output_stream.get() << parser.to_xml() << std::endl;
            else  // if (options.count("parse-ir"))  // Default to IR
                output_stream.get() << parser.to_ir()->ir_code() << std::endl;
            return 0;
        }

        // -------- Postprocessing
        std::shared_ptr<toycc::ir::Scope> ir = parser.to_ir();
        std::shared_ptr<toycc::ir::Scope> processed_ir = toycc::ir::PostProcessor::process(ir);
        if (target_step == SequenceStep::POSTPROCESS) {
            output_stream.get() << processed_ir->ir_code() << std::endl;
            return 0;
        }

        // -------- Flow analysis
        toycc::ir::TranslationUnit unit(processed_ir, std::filesystem::current_path().string(), input_file_name);
        if (target_step == SequenceStep::FLOW) {
            output_stream.get() << unit.dot_graph() << std::endl;
            return 0;
        }

        // -------- Code generation
        std::stringstream assembly;
        toycc::arch::x86_64::CodeGenerator codegen(unit);
        codegen(assembly);

        if (target_step == SequenceStep::CODEGEN) {
            output_stream.get() << assembly.str() << std::endl;
            return 0;
        }

        // -------- Assembly
        toycc::TempFile temp_object_path(std::filesystem::path(input_file_name).filename().replace_extension("o"));
        std::filesystem::path production_path;
        if (options.count("output"))
            production_path = options["output"].as<std::string>();
        else
            production_path = DEFAULT_OBJECT_FILE_NAME;

        toycc::assemble(assembly.str(), temp_object_path);
        if (target_step == SequenceStep::ASSEMBLY) {
            std::filesystem::copy_file(temp_object_path, production_path, std::filesystem::copy_options::overwrite_existing);
            return 0;
        }

        // -------- Linker
        toycc::link(temp_object_path, production_path);
        return 0;

    } catch (toycc::Diagnostic const& diagnostic) {
        toycc::log(diagnostic);
        return 2;
    }

    return 0;
}
