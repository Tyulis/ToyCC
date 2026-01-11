import os
from opcodes.x86_64 import *
from execmodel.model import *

LOCATION_HEADER = "location.h"
TRANSLATION_TAG_HEADER = "translation_tag.h"
MATCHER_HEADER = "matcher.h"

MATCHER_DIRECTORY = "matcher"
INSTRUCTION_DIRECTORY = "instruction"

generated_sources = set()
generated_headers = set()

def write_header(content, header_path, includes=(), stdincludes=()):
    generated_headers.add(header_path)

    with open(header_path, "w") as f:
        f.write("#pragma once\n\n")
        for include in stdincludes:
            f.write(f"#include <{include}>\n")
        for include in includes:
            f.write(f'#include "{include}"\n')
        f.write("\n")
        f.write("namespace toycc::execmodel::x86_64 {\n")
        for line in content.splitlines():
            f.write(f"    {line}\n")
        f.write("}\n")

def write_source(content, source_path, includes=(), stdincludes=()):
    generated_sources.add(source_path)
    with open(source_path, "w") as f:
        for include in stdincludes:
            f.write(f"#include <{include}>\n")
        for include in includes:
            f.write(f'#include "{include}"\n')
        f.write("\n")
        f.write("namespace toycc::execmodel::x86_64 {\n")
        for line in content.splitlines():
            f.write(f"    {line}\n")
        f.write("}\n")

# -------- Value locations
def generate_locations(translation_model, output_dir):
    content = "enum class Location {\n"
    for index, location in enumerate(translation_model.locations):
        content += f"    {location} = {index},\n"
    content += "};"
    write_header(content, os.path.join(output_dir, LOCATION_HEADER))


# -------- Graph matcher
def generate_translation_tags(translation_model, output_dir):
    header_content = ""

    max_depth = max(len(translation.ir) for translation in translation_model.translations)
    header_content += f"constexpr size_t MAX_TRANSLATION_DEPTH = {max_depth};\n"

    header_content += "enum class TranslationTag {\n"
    for translation in translation_model.translations:
        header_content += f"    {translation.tag},\n"
    header_content += "};\n"

    write_header(header_content, os.path.join(output_dir, TRANSLATION_TAG_HEADER), [], ["cstddef"])

def group_statements_array(translation: TranslationSpec) -> str:
    # Subgraph description
    source_content  = f"constexpr static std::array<toycc::ir::StatementTag, {len(translation.ir)}> STATEMENTS_{translation.group_tag} = {{"
    for index, spec in enumerate(translation.ir):
        source_content += f"toycc::ir::StatementTag::{spec.tag}"
        if index != len(translation.ir) - 1:
            source_content += ", "
    source_content += "};\n"
    return source_content

def generate_translation_matcher(translation: TranslationSpec) -> tuple[str, str]:
    prototype = f"template<> toycc::arch::x86_64::MatchingResult match_translation<TranslationTag::{translation.tag}> (const toycc::arch::x86_64::StackFrame& frame, const toycc::ir::DependencyGraph& graph)"
    header_content = f"{prototype};\n"
    source_content = ""

    links = translation.ir_links()
    if len(links) > 0:
        source_content += (f"constexpr static std::array<std::pair<size_t, size_t>, {len(links)}> LINKS_{translation.tag} = {{" +
                           ", ".join(f"std::pair{{{link[0]}, {link[1]}}}" for link in links) +
                           "};\n")

    source_content += f"{prototype} {{\n"
    source_content += f"    std::vector<std::shared_ptr<toycc::ir::DependencyNode>> statements = toycc::arch::x86_64::match_subgraph_statements(graph, STATEMENTS_{translation.group_tag}, LINKS_{translation.tag});\n"
    source_content +=  "}\n"

    return header_content, source_content

def generate_matcher(translation_model, output_dir):
    header_content =  "template <TranslationTag tag>\n"
    header_content += "toycc::arch::x86_64::MatchingResult match_translation(const toycc::arch::x86_64::StackFrame& frame, const toycc::ir::DependencyGraph& graph);\n\n"

    source_files = {}
    for translation in translation_model.translations:
        matcher_header, matcher_source = generate_translation_matcher(translation)

        header_content += matcher_header;
        if translation.group_tag not in source_files:
            source_files[translation.group_tag] = group_statements_array(translation)
        source_files[translation.group_tag] += matcher_source + "\n"

    matcher_dir = os.path.join(output_dir, MATCHER_DIRECTORY)
    matcher_header = os.path.join(output_dir, MATCHER_HEADER)
    os.makedirs(matcher_dir, exist_ok=True)
    for group, content in source_files.items():
        write_source(content, os.path.join(matcher_dir, f"{group}.cpp"), ["ir/flow.h", "arch/x86_64/execmodel.hpp", "arch/x86_64/allocation.h", matcher_header], ["array", "utility"])

    header_content = "std::vector<toycc::arch::x86_64::MatchingResult> match_translations(const toycc::arch::x86_64::StackFrame& frame, const toycc::ir::DependencyGraph& graph);\n"
    source_content = ""

    write_header(header_content, os.path.join(output_dir, "matcher.h"), ["ir/flow.h", "arch/x86_64/execmodel.hpp", "arch/x86_64/allocation.h"])
    write_source(source_content, os.path.join(output_dir, "matcher.cpp"), [matcher_header])

def generate_matcher(translation_model: TranslationModel, output_dir: str):
    prototype = "std::vector<toycc::arch::x86_64::MatchingResult> match_translations(const toycc::arch::x86_64::StackFrame& frame, const toycc::ir::DependencyGraph& graph)"
    header_content = f"{prototype};\n"

    source_content  = f"{prototype} {{\n"
    source_content += f"    std::unordered_set<std::shared_ptr<toycc::ir::DependencyNode>> entry_statements = toycc::arch::x86_64::get_entry_statements(graph);\n"
    source_content += f"    std::vector<toycc::arch::x86_64::MatchingResult> matches;\n"
    source_content += f"    for (std::shared_ptr<toycc::ir::DependencyNode> node : entry_statements) {{\n"
    source_content += f"        const Statement& statement = node->statement();\n"
    source_content += f"        "
    source_content += f"    }}\n"
    source_content += "}\n"

    write_header(header_content, os.path.join(output_dir, "matcher.h"), ["ir/flow.h", "arch/x86_64/execmodel.h", "arch/x86_64/allocation.h"])
    write_source(source_content, os.path.join(output_dir, "matcher.cpp"), [os.path.join(output_dir, "matcher.h")])

def generate_translations(translation_model, output_dir):
    generate_translation_tags(translation_model, output_dir)
    generate_matcher(translation_model, output_dir)


def generate_execmodel(translation_model, output_dir):
    generate_locations(translation_model, output_dir)
    generate_translations(translation_model, output_dir)
    #generate_operand_types(translation_model, output_dir)
    #generate_translations(translation_model, output_dir)
    #generate_instruction_set(translation_model, instruction_set, output_dir)
