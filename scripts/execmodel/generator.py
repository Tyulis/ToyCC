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

    group_tags = "enum class TranslationGroupTag {\n"
    translation_tags = "enum class TranslationTag {\n"

    for group_tag, group in translation_model.translations.items():
        group_tags += f"    {group_tag},\n"
        for translation in group.translations:
            translation_tags += f"    {translation.tag},\n"

    group_tags += "};\n"
    translation_tags += "};\n"

    header_content = group_tags + "\n" + translation_tags
    write_header(header_content, os.path.join(output_dir, TRANSLATION_TAG_HEADER))

def generate_group_subgraph(group: TranslationGroup) -> str:
    content = f"const static std::vector<toycc::ir::StatementTag> STATEMENTS_{group.tag()} = {{"
    content += ", ".join(f"toycc::ir::StatementTag::{tag}" for tag in group.statements)
    content += "};\n"

    if len(group.statements) > 1:
        content += f"const static arma::imat SUBGRAPH_{group.tag()} = {{"
        for row in group.subgraph:
            content += "{" + ", ".join(str(cell) for cell in row) + "}, "
        content += "};\n"
    return content

def generate_matcher(translation_model, output_dir):
    header_content = ""
    source_content = "const static arma::imat TRIVIAL_SUBGRAPH = {};\n\n"

    for group in translation_model.translations.values():
        source_content += generate_group_subgraph(group) + "\n"

    matcher_header = os.path.join(output_dir, "matcher.h")
    write_header(header_content, matcher_header, ["ir/statement.h", "arch/x86_64/execmodel.h"])
    write_source(source_content, os.path.join(output_dir, "matcher.cpp"), [matcher_header], ["armadillo"])

def generate_translations(translation_model, output_dir):
    generate_translation_tags(translation_model, output_dir)
    generate_matcher(translation_model, output_dir)


def generate_execmodel(translation_model, output_dir):
    generate_locations(translation_model, output_dir)
    generate_translations(translation_model, output_dir)
    #generate_operand_types(translation_model, output_dir)
    #generate_translations(translation_model, output_dir)
    #generate_instruction_set(translation_model, instruction_set, output_dir)
