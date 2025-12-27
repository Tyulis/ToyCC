#include <boost/program_options/errors.hpp>
#include <boost/program_options/positional_options.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

#include <boost/program_options.hpp>

#include "parser.h"
#include "source_map.h"
#include "diagnostic.h"
#include "preprocess.h"
#include "util/log.h"

enum class SequenceStep : unsigned int {
    NONE = 0,
    PREPROCESS = 1,
    SOURCE_MAP = 2,
    PARSE = 3,
};

int main(int argc, char** argv) {
    boost::program_options::options_description sequence_options("Sequence options");
    sequence_options.add_options()("preprocess,E",   "Only preprocess the source code")
                                  ("source-map",     "Preprocess and annotate the source lines")
                                  ("parse-lisp",     "Output the AST as Lisp")
                                  ("parse-xml",      "Output the AST as XML")
                                  ("parse-ir",       "Output the intermediate representation");

    boost::program_options::options_description generic_options("Generic options");
    generic_options.add_options()("help,h",        "Show this help message")
                                 ("output,o",      boost::program_options::value<std::string>(), "Output file name")
                                 ("source-file,f", boost::program_options::value<std::string>(), "Source files");

    boost::program_options::options_description all_options;
    all_options.add(generic_options).add(sequence_options);

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

    SequenceStep target_step = SequenceStep::PARSE;
    if (options.count("preprocess"))
        target_step = SequenceStep::PREPROCESS;
    if (options.count("source-map"))
        target_step = SequenceStep::SOURCE_MAP;
    if (options.count("parse-lisp") || options.count("parse-xml") || options.count("parse-ir"))
        target_step = SequenceStep::PARSE;

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
                output_stream.get() << parser.to_ir() << std::endl;
            return 0;
        }
    } catch (toycc::Diagnostic const& diagnostic) {
        toycc::log(diagnostic);
        return 2;
    }

    return 0;
}
