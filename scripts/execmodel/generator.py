import os
from opcodes.x86_64 import *
from execmodel.model import *

LOCATION_HEADER = "location.h"
OPERAND_TYPE_HEADER = "operand_type.h"
TRANSLATION_HEADER = "translation.h"
INSTRUCTION_HEADER = "instruction.h"

TRANSLATION_DIRECTORY = "translation"
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


# -------- Operand type matching
def generate_operand_types(translation_model, output_dir):
    content = "enum class OperandType {\n"
    for name, enum_value in sorted(translation_model.operandtype_enum.items()):
        content += f"    {enum_value},\n"
    content += "};\n\n"

    content += "template <OperandType type> std::optional<Location> match_operand_type"
    content += "(const toycc::ir::Operand& operand, const std::unordered_set<Location>& locations, const toycc::CodeLocation& code_location);\n\n"

    for name, operandtype in sorted(translation_model.operandtypes.items()):
        operandtype_enum = translation_model.operandtype_enum[name]

        operand_arg = " operand" if operandtype.uses_operand else ""
        code_location_arg = " code_location" if operandtype.uses_code_location else ""
        content += f"template<> inline std::optional<Location> match_operand_type<OperandType::{operandtype_enum}>"
        content += f"(const toycc::ir::Operand&{operand_arg}, const std::unordered_set<Location>& locations, const toycc::CodeLocation&{code_location_arg}) {{\n"

        if operandtype.uses_location:
            content += f"    std::unordered_set<Location> matching_locations;\n"
            content += f"    for (const Location& location : locations)\n"
            content += f"        if ({operandtype.expression})\n"
            content +=  "            matching_locations.insert(location);\n"
            content +=  "    return toycc::arch::x86_64::best_location(matching_locations);\n"
        else:
            content += f"    if ({operandtype.expression})\n"
            content +=  "        return toycc::arch::x86_64::best_location(locations);\n"
            content +=  "    else return {};\n"
        content += "}\n\n"

    write_header(content, os.path.join(output_dir, OPERAND_TYPE_HEADER),
                 ["code_location.h", "ir/declaration.h", "arch/x86_64/execmodel.h", "gen/execmodel/x86_64/location.h"], ["optional", "unordered_set"])


# -------- IR -> assembly translation
def translation_matching_condition(option: TranslationSpec, translation_model: TranslationModel):
    # Matching condition
    if option.inputs is not None:
        match_condition = f"statement->inputs.size() == {len(option.inputs)}"
        for index, operand in enumerate(option.inputs):
            match_condition += f" && match_operand_type<OperandType::{translation_model.operandtype_enum[operand.operand_type]}>"
            match_condition += f"(statement->inputs[{index}], frame.locate(statement->inputs[{index}]), statement->location)"
        return match_condition
    else:
        return None

def generate_translation_option(index, option, tag, translation_model):
    translation_function_name = f"translate_{tag}_{index}"
    if option.name is not None:
        translation_function_name += f"_{option.name}"

    # Prototype
    translation_function =  f"static bool {translation_function_name} (toycc::arch::x86_64::StackFrame& frame, std::shared_ptr<toycc::ir::Statement> statement) {{\n"

    # Call to the assembly emission function
    if option.target is not None:
        if option.target_inputs is not None:
            translation_function +=  "    std::vector<toycc::ir::Operand> target_inputs = {";
            for index, operand in enumerate(option.target_inputs):
                translation_function += f"statement->inputs[{operand.target_index}]"
                if index != len(option.target_inputs) - 1:
                    translation_function += ", "
            translation_function += "};\n"
            translation_function += f"    return emit_{option.target}(frame, target_inputs, statement->output, statement->location);\n"
        else:
            translation_function += f"    return emit_{option.target}(frame, statement->inputs, statement->output, statement->location);\n"

    translation_function += "}\n"
    return translation_function_name, translation_function

def generate_tag_translations(tag, options, translation_model, translation_dir):
    source_content = ""

    match_function_name = f"translate_{tag}"
    match_function_prototype = f"bool {match_function_name} (toycc::arch::x86_64::StackFrame& frame, std::shared_ptr<toycc::ir::Statement> statement)"
    match_function = f"{match_function_prototype} {{\n"

    checked_types = set()
    for index, option in enumerate(options):
        matching_condition = translation_matching_condition(option, translation_model)
        translation_function_name, translation_function = generate_translation_option(index, option, tag, translation_model)
        source_content += translation_function + "\n"

        if matching_condition is None:
            match_function += f"    if ({translation_function_name}(frame, statement))\n"
        else:
            match_function += f"    if (({matching_condition}) && {translation_function_name}(frame, statement))\n"
        match_function +=  "        return true;\n"

    match_function += "    return false;\n"
    match_function += "}"
    source_content += match_function

    write_source(source_content, os.path.join(translation_dir, f"{tag.lower()}.cpp"),
                 ["ir/statement.h", "arch/x86_64/allocation.h", "gen/execmodel/x86_64/instruction.h", "gen/execmodel/x86_64/translation.h", "gen/execmodel/x86_64/operand_type.h"])
    return f"{match_function_prototype};\n"

def generate_translations(translation_model, output_dir):
    translation_dir = os.path.join(output_dir, TRANSLATION_DIRECTORY)
    os.makedirs(translation_dir, exist_ok=True)

    header_content = ""
    for tag, options in translation_model.translations.items():
        header_content += generate_tag_translations(tag, options, translation_model, translation_dir)

    write_header(header_content, os.path.join(output_dir, TRANSLATION_HEADER), ["ir/statement.h", "arch/x86_64/allocation.h"])


# -------- Assembly instruction emission
def instruction_matching_condition(form: InstructionForm, translation_model: TranslationModel):
    match_condition = f"inputs.size() == {len(form.operands)}"
    for index, operand in enumerate(form.operands):
        match_condition += f" && match_operand_type<OperandType::{translation_model.operandtype_enum[operand.type]}>"
        match_condition += f"(inputs[{index}], frame.locate(inputs[{index}]), code_location)"
    return match_condition

def generate_instruction_form(index: int, name: str, form: InstructionForm, translation_model: TranslationModel) -> tuple[str, str]:
    emission_function_name = f"emit_{name}_{index}"
    for operand in form.operands:
        emission_function_name += f"_{translation_model.operandtype_enum[operand.type]}"

    # Prototype
    emission_function = f"static bool {emission_function_name}(toycc::arch::x86_64::StackFrame& frame, const std::vector<toycc::ir::Operand>& inputs, const std::optional<toycc::ir::Operand>& output, const CodeLocation& code_location) {{\n"

    emission_function += "std::vector<Location> input_locations = {"
    for index, operand in enumerate(form.operands):
        if operand.is_input:
            emission_function += f"frame.locate"  #HERE
    emission_function += "};\n"

    # Check for the case where a stack variable shouldn't be overwritten
    for index, operand in enumerate(form.operands):
        if operand.is_output:
            emission_function += "    if (!output.has_value())\n"
            emission_function += "        return false;"  #HERE

    emission_function += "    return true;\n"
    emission_function += "}\n"
    return emission_function_name, emission_function

def generate_instruction(instruction: Instruction, instruction_dir: str, translation_model: TranslationModel) -> str:
    source_content = f"// -------- {instruction.name} : {instruction.summary}\n"

    match_function_name = f"emit_{instruction.name}"
    match_function_prototype = f"bool {match_function_name}(toycc::arch::x86_64::StackFrame& frame, const std::vector<toycc::ir::Operand>& inputs, const std::optional<toycc::ir::Operand>& output, const CodeLocation& code_location)"
    match_function = f"{match_function_prototype} {{\n"

    for index, form in enumerate(instruction.forms):
        matching_condition = instruction_matching_condition(form, translation_model)
        emission_function_name, emission_function = generate_instruction_form(index, instruction.name, form, translation_model)
        source_content += emission_function + "\n"

        if matching_condition is None:
            match_function += f"    if ({emission_function_name}(frame, inputs, output, code_location))\n"
        else:
            match_function += f"    if (({matching_condition}) && {emission_function_name}(frame, inputs, output, code_location))\n"
        match_function +=  "        return true;\n"

    match_function += "    return false;\n"
    match_function += "}"
    source_content += match_function

    write_source(source_content, os.path.join(instruction_dir, f"{instruction.name}.cpp"), ["gen/execmodel/x86_64/instruction.h", "gen/execmodel/x86_64/operand_type.h"])
    return f"{match_function_prototype};\n"

def generate_instruction_set(translation_model: TranslationModel, instruction_set: list[Instruction], output_dir: str):
    instruction_dir = os.path.join(output_dir, INSTRUCTION_DIRECTORY)
    os.makedirs(instruction_dir, exist_ok=True)

    header_content = ""
    for instruction in instruction_set:
        if instruction.name in translation_model.targets:
            header_content += generate_instruction(instruction, instruction_dir, translation_model)

    write_header(header_content, os.path.join(output_dir, INSTRUCTION_HEADER),
                 ["ir/declaration.h", "arch/x86_64/allocation.h"], ["vector", "optional"])


def generate_execmodel(translation_model, instruction_set, output_dir):
    generate_locations(translation_model, output_dir)
    generate_operand_types(translation_model, output_dir)
    generate_translations(translation_model, output_dir)
    generate_instruction_set(translation_model, instruction_set, output_dir)
