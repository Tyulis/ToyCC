import os
from pathlib import Path
from opcodes.x86_64 import *
from execmodel.model import *

LOCATION_HEADER = "location.h"
TRANSLATION_TAG_HEADER = "translation_tag.h"
MATCHER_HEADER = "matcher.h"

MATCHER_DIRECTORY = "matcher"
INSTRUCTION_DIRECTORY = "instruction"

BOOL = {True: "true", False: "false"}

def write_file(content, path):
    if os.path.exists(path):
        with open(path, "r") as f:
            initial_content = f.read()
        if initial_content == content:
            return

    with open(path, "w") as f:
        f.write(content)

def write_header(content, header_path, includes=(), stdincludes=()):
    full_content = "#pragma once\n\n"
    for include in stdincludes:
        full_content += f"#include <{include}>\n"
    for include in includes:
        full_content += f'#include "{include}"\n'
    full_content += "\n";
    full_content += "namespace toycc::execmodel::x86_64 {\n";
    full_content += "\n".join("    " + line for line in content.splitlines())
    full_content += "}\n"
    write_file(full_content, header_path)


def write_source(content, source_path, includes=(), stdincludes=()):
    full_content = ""
    for include in stdincludes:
        full_content += f"#include <{include}>\n"
    for include in includes:
        full_content += f'#include "{include}"\n'
    full_content += "\n"
    full_content += "namespace toycc::execmodel::x86_64 {\n"
    full_content += "\n".join("    " + line for line in content.splitlines())
    full_content += "}\n"
    write_file(full_content, source_path)


# -------- Value locations
def generate_locations(translation_model: TranslationModel, output_dir: Path):
    content = "enum class Location {\n"
    for index, location in enumerate(translation_model.locations):
        content += f"    {location} = {index},\n"
    content += "};"
    write_header(content, os.path.join(output_dir, LOCATION_HEADER))


# -------- Graph matcher
def generate_translation_tags(translation_model: TranslationModel, output_dir: Path):
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
    content = f"const static std::vector<ir::StatementTag> STATEMENTS_{group.tag()} = {{"
    content += ", ".join(f"ir::StatementTag::{tag}" for tag in group.statements)
    content += "};\n"

    if len(group.subgraph) > 0:
        if len(group.subgraph[0]) == 1:
            content += f"const static arma::icolvec SUBGRAPH_{group.tag()} = {{"
            content += ", ".join(str(row[0]) for row in group.subgraph)
            content += "};\n"
        else:
            content += f"const static arma::imat SUBGRAPH_{group.tag()} = {{"
            for row in group.subgraph:
                content += "{" + ", ".join(str(cell) for cell in row) + "}, "
            content += "};\n"
    return content

def generate_group_matcher_functions(translation_model: TranslationModel) -> tuple[str, str]:
    prototype = "std::vector<GroupMatch> match_groups(const ir::DependencyMatrix& dependencies)"
    header_content = f"{prototype};\n"

    source_content = ""
    source_content += f"template <TranslationGroupTag group>\n"
    source_content += f"void match_subgraph_group(std::vector<GroupMatch>& matches, const ir::DependencyMatrix& matrix);\n\n"

    for group in translation_model.translations.values():
        source_content += f"template<> inline void match_subgraph_group<TranslationGroupTag::{group.tag()}>"
        source_content += f"(std::vector<GroupMatch>& matches, const ir::DependencyMatrix& matrix) {{\n"

        subgraph_name = "TRIVIAL_SUBGRAPH" if len(group.subgraph) == 0 else f"SUBGRAPH_{group.tag()}"
        source_content += f"    matches.insert_range(matches.begin(), match_dependency_subgraph(TranslationGroupTag::{group.tag()}, matrix, STATEMENTS_{group.tag()}, {subgraph_name}));\n"
        source_content += f"}}\n\n"

    source_content += f"{prototype} {{\n"
    source_content += f"    std::vector<GroupMatch> matches;\n"
    for group in translation_model.translations.values():
        source_content += f"    match_subgraph_group<TranslationGroupTag::{group.tag()}> (matches, dependencies);\n"
    source_content += f"    return matches;\n"
    source_content += "}\n"

    return header_content, source_content

def generate_group_matcher(translation_model: TranslationModel, output_dir: Path):
    header_content = "using namespace toycc::arch::x86_64;\n\n"
    source_content = "using namespace toycc::arch::x86_64;\n\n"
    source_content += "const static arma::imat TRIVIAL_SUBGRAPH = {};\n\n"

    for group in translation_model.translations.values():
        source_content += generate_group_subgraph(group) + "\n"

    matcher_header, matcher_source = generate_group_matcher_functions(translation_model)
    header_content += matcher_header
    source_content += matcher_source

    write_header(header_content, output_dir / "group_matcher.h", ["arch/x86_64/execmodel.h"])
    write_source(source_content, output_dir / "group_matcher.cpp", ["ir/statement.h", "arch/x86_64/execmodel.h", output_dir / "group_matcher.h"], ["memory", "vector", "armadillo"])


# -------- Translation rules
def generate_constraint(constraint: Constraint, arg_usage: dict[str, bool]) -> str:
    match constraint.type:
        case ConstraintType.CONSTANT:
            return "OperandMatch::OK" if constraint.parameter else "OperandMatch::KO"
        case ConstraintType.CONJUNCTION:
            return "(" + " & ".join(generate_constraint(sub, arg_usage) for sub in constraint.parameter) + ")"
        case ConstraintType.DISJUNCTION:
            return "(" + " | ".join(generate_constraint(sub, arg_usage) for sub in constraint.parameter) + ")"
        case ConstraintType.CATEGORY:
            arg_usage["operand"] = True
            match constraint.parameter:
                case "constant":
                    return "is_constant(operand)"
                case "variable":
                    return "is_variable(operand)"
                case "label":
                    return "is_label(operand)"
                case "dereference":
                    return "is_dereference(operand)"
                case "_":
                    raise TranslationModelError(f"Invalid category constraint `{constraint.parameter}`")
        case ConstraintType.TYPE:
            arg_usage["operand"] = True
            return f"check_type(operand, ir::TypeCategory::{constraint.parameter})"
        case ConstraintType.LOCATION:
            arg_usage["frame"] = True
            arg_usage["operand"] = True
            return f"check_location(frame, operand, Location::{constraint.parameter})"
        case ConstraintType.SIZE:
            arg_usage["operand"] = True
            return f"check_size(operand, {constraint.parameter})"
        case ConstraintType.VALUE_EQ:
            arg_usage["operand"] = True
            return f'check_value_eq(operand, ir::IntegerConstant("{constraint.parameter}"))'
        case ConstraintType.VALUE_LE:
            arg_usage["operand"] = True
            return f'check_value_le(operand, ir::IntegerConstant("{constraint.parameter}"))'
        case ConstraintType.VALUE_GE:
            arg_usage["operand"] = True
            return f'check_value_ge(operand, ir::IntegerConstant("{constraint.parameter}"))'
        case ConstraintType.STORAGE:
            arg_usage["operand"] = True
            return f"check_storage(operand, ir::StorageClass::{constraint.parameter})"

def generate_operand_constraint(operand: Constraint, arg_usage: dict[str, bool]) -> str:
    denormalized = denormalize(operand)
    return generate_constraint(denormalized, arg_usage)

def generate_operand_condition(operand_condition_name: str, operand: Constraint|tuple[int, str], overwritten_by: list[int]) -> str:
    conjunction = []
    arg_usage = {"frame": False, "graph": False, "group_match": False, "operand": False}
    if isinstance(operand, Constraint):
        conjunction.append(generate_operand_constraint(operand, arg_usage))

    for overwrite_index in range(len(overwritten_by)):
        arg_usage["frame"] = True
        arg_usage["graph"] = True
        arg_usage["group_match"] = True
        arg_usage["operand"] = True
        conjunction.append(f"check_overwrite(frame, graph, operand, overwrite_{overwrite_index}, group_match)")

    if len(conjunction) == 0:
        expression = "OperandMatch::OK"
    else:
        expression = " & ".join(f"({subexpression})" for subexpression in conjunction)


    frame_arg       = " frame"       if arg_usage["frame"]       else ""
    graph_arg       = " graph"       if arg_usage["graph"]       else ""
    group_match_arg = " group_match" if arg_usage["group_match"] else ""
    operand_arg     = " operand"     if arg_usage["operand"]     else ""

    source  = f"static inline OperandMatch {operand_condition_name}(const StackFrame&{frame_arg}, const ir::DependencyGraph&{graph_arg}, const GroupMatch&{group_match_arg}, const ir::Operand&{operand_arg}"
    if len(overwritten_by) > 0:
        source += ", " + ", ".join(f"const ir::Operand& overwrite_{index}" for index in range(len(overwritten_by)))
    source += "){\n"

    source += f"    return {expression};\n"
    source += "}\n"
    return source;

def generate_operand_ref(statement_index: int, input_index: int|None) -> str:
    if input_index is None:  # Output
        return f"(*group_match.statements[{statement_index}]->statement().output)"
    else:
        return f"group_match.statements[{statement_index}]->statement().inputs[{input_index}]"

def generate_translation_matcher_function(translation: TranslationSpec, translation_model: TranslationModel) -> str:
    operand_conditions = []
    statements_code = ""
    has_operands = False
    for statement_index, statement in enumerate(translation.ir):
        ir_spec = translation_model.ir[statement.tag]
        inputs = []
        outputs = []

        for name, operand in statement.operands.items():
            overwritten_by = []
            if name == "output":
                operand_list = outputs
                operand_ref = generate_operand_ref(statement_index, None)
            else:
                operand_list = inputs
                input_index = ir_spec.input.index(name)
                operand_ref = generate_operand_ref(statement_index, input_index)

                for overwrite_statement_index, overwrite_statement in enumerate(translation.ir):
                    for overwrite_operand_name, overwrite_operand in overwrite_statement.operands.items():
                        if overwrite_operand_name == "output" and not isinstance(overwrite_operand, Constraint) and overwrite_operand[0] == statement_index and overwrite_operand[1] == name:
                            overwritten_by.append(overwrite_statement_index)
                            break

            operand_condition_name = f"condition_{translation.tag}_{statement_index}_{name}"
            operand_conditions.append(generate_operand_condition(operand_condition_name, operand, overwritten_by))
            operand_match = f"{operand_condition_name}(frame, graph, group_match, {operand_ref}"
            if len(overwritten_by) > 0:
                operand_match += ", " + ", ".join(generate_operand_ref(overwrite_statement_index, None) for overwrite_statement_index in overwritten_by)
            operand_match += ")"
            operand_list.append(operand_match)

        if len(inputs) > 0 or len(outputs) > 0:
            has_operands = True

        statements_code += "        StatementMatch {\n"
        statements_code += "            .input = {" + ", ".join(inputs) + "},\n"
        statements_code += "            .output = {" + ", ".join(outputs) + "},\n"
        statements_code += "        },\n"

    source_content =  f"template<> TranslationMatch match_translation<TranslationTag::{translation.tag}> "
    source_content += f"(const StackFrame&{' frame' if has_operands else ''}, const ir::DependencyGraph&{' graph' if has_operands else ''}, const GroupMatch& group_match) {{\n"
    source_content += f"    return {{.translation = TranslationTag::{translation.tag}, .group_match = group_match, .statements = {{\n"
    source_content += statements_code
    source_content +=  "    }};\n"
    source_content += "}\n\n"
    return "\n".join(operand_conditions) + "\n" + source_content

def generate_translation_group(group: TranslationGroup, translation_model: TranslationModel, output_dir: Path):
    header_content = "using namespace toycc::arch::x86_64;\n\n"
    source_content = "using namespace toycc::arch::x86_64;\n\n"

    prototype = f"template<> std::optional<TranslationMatch> match_translation_group<TranslationGroupTag::{group.tag()}> (const StackFrame& frame, const ir::DependencyGraph& graph, const GroupMatch& group_match)"
    header_content += f"{prototype};\n"

    source_content += f"template <TranslationTag tag>\n"
    source_content += f"TranslationMatch match_translation(const StackFrame& frame, const ir::DependencyGraph& graph, const GroupMatch& group_match);\n\n"

    for translation in group.translations:
        source_content += generate_translation_matcher_function(translation, translation_model)

    source_content += f"{prototype} {{\n"
    source_content += f"    std::optional<TranslationMatch> match;\n"
    for translation in group.translations:
        source_content += f"    update_translation_match(match, match_translation<TranslationTag::{translation.tag}> (frame, graph, group_match));\n"
    source_content += f"    return match;\n"
    source_content += "}\n"

    header_path = output_dir / f"{group.tag()}.h"
    source_path = output_dir / f"{group.tag()}.cpp"

    write_header(header_content, header_path, ["arch/x86_64/allocation.h", "arch/x86_64/execmodel.h", "gen/execmodel/x86_64/translation_matcher.h"])
    write_source(source_content, source_path, ["arch/x86_64/constraints.hpp", header_path])
    return header_path

def generate_translation_matcher(translation_model: TranslationModel, output_dir: Path):
    translation_matcher_dir = output_dir / "translation_matcher"
    os.makedirs(translation_matcher_dir, exist_ok=True)

    group_headers = []
    for group in translation_model.translations.values():
        group_headers.append(generate_translation_group(group, translation_model, translation_matcher_dir))

    header_content = "using namespace toycc::arch::x86_64;\n\n"
    source_content = "using namespace toycc::arch::x86_64;\n\n"

    header_content += "template <TranslationGroupTag group>\n"
    header_content += "std::optional<TranslationMatch> match_translation_group(const StackFrame& frame, const ir::DependencyGraph& graph, const GroupMatch& group_match);\n\n"

    prototype = "std::vector<TranslationMatch> match_translations(const StackFrame& frame, const ir::DependencyGraph& graph, const std::vector<GroupMatch>& group_matches)"
    header_content += f"{prototype};\n"

    source_content += f"{prototype} {{\n"
    source_content += f"    std::vector<TranslationMatch> matches;"
    source_content += f"    auto add_match = [&](std::optional<TranslationMatch> match) {{if (match.has_value())  matches.push_back(match.value());}};\n"
    source_content += f"    for (const GroupMatch& group_match : group_matches) {{\n"
    source_content += f"        switch (group_match.group) {{\n"
    for group in translation_model.translations.values():
        source_content += f"            case TranslationGroupTag::{group.tag()}: add_match(match_translation_group<TranslationGroupTag::{group.tag()}> (frame, graph, group_match));  break;\n"
    source_content +=  "        }\n"
    source_content +=  "    }\n"
    source_content +=  "    return matches;\n"
    source_content +=  "}\n"

    write_header(header_content, output_dir / "translation_matcher.h", ["ir/flow.h", "arch/x86_64/allocation.h", "arch/x86_64/execmodel.h"], ["vector", "optional"])
    write_source(source_content, output_dir / "translation_matcher.cpp", [output_dir / "translation_matcher.h"] + group_headers)


# -------- Code emission routines
def generate_emission_translation(translation: Translation, translation_model: TranslationModel) -> Path:
    function_content = ""
    uses_match = False
    for target in translation.target:
        inputs = []
        outputs = []
        for index, operand in enumerate(target.form.operands):
            if operand.is_output:
                outputs.append(index)
            elif operand.is_input:
                inputs.append(index)

        operand_arguments = []
        operand_moves = ""
        operand_order = inputs + outputs
        for operand_index in operand_order:
            operand = target.form.operands[operand_index]
            statement_index, operand_name = target.operands[operand_index]
            ir_spec = translation_model.ir[translation.ir[statement_index].tag]

            if operand.is_output:
                output_ref = f"match.group_match.statements[{statement_index}]->statement().output.value()"
                output_location = f"*match.statements[{statement_index}].output->location"

            if operand.is_input:
                input_index = ir_spec.input.index(operand_name)
                operand_ref = f"match.group_match.statements[{statement_index}]->statement().inputs[{input_index}]"
                operand_location = f"*match.statements[{statement_index}].input[{input_index}].location"
            else:
                operand_ref = output_ref
                operand_location = output_location

            operand_arguments.append(f"emit_operand(frame, {operand_ref}, {operand_location})")

            if operand.is_output:
                operand_moves += f"    move_operand(frame, {output_ref}, {operand_location});\n"

        asm_format = f"{target.form.gas_name} " + ", ".join("{}" for _ in operand_order)

        if len(operand_arguments) == 0:
            function_content += f'    frame.output.statement("{asm_format}");\n'
        else:
            uses_match = True
            operand_code = ", ".join(operand_arguments)
            function_content += f'    frame.output.statement(std::format("{asm_format}", {operand_code}));\n'
        function_content += operand_moves

    prototype = f"template<> void emit_translation<TranslationTag::{translation.tag}> (StackFrame& frame, const TranslationMatch&{' match' if uses_match else ''})"
    header_content = f"{prototype};\n"

    source_content  = f"{prototype} {{\n"
    source_content += function_content
    source_content += "}\n"

    return header_content, source_content

def generate_emission_group(group: TranslationGroup, translation_model: TranslationModel, output_dir: Path) -> Path:
    header_path = output_dir / f"{group.tag()}.h"
    source_path = output_dir / f"{group.tag()}.cpp"

    header_content = "using namespace toycc::arch::x86_64;\n\n"
    source_content = "using namespace toycc::arch::x86_64;\n\n"

    for translation in group.translations:
        translation_header, translation_source = generate_emission_translation(translation, translation_model)
        header_content += translation_header
        source_content += translation_source + "\n"

    write_header(header_content, header_path, ["gen/execmodel/x86_64/emission.h"])
    write_source(source_content, source_path, ["arch/x86_64/assembly.h", header_path], ["format"])
    return header_path

def generate_emission(translation_model: TranslationModel, output_dir: Path):
    emission_dir = output_dir / "emission"
    os.makedirs(emission_dir, exist_ok=True)

    group_headers = []
    for group in translation_model.translations.values():
        group_headers.append(generate_emission_group(group, translation_model, emission_dir))

    header_content = "using namespace toycc::arch::x86_64;\n\n"
    source_content = "using namespace toycc::arch::x86_64;\n\n"

    header_content += "template <TranslationTag translation>\n"
    header_content += "void emit_translation(StackFrame& frame, const TranslationMatch& match);\n\n"

    prototype = "void emit_code(StackFrame& frame, const TranslationMatch& match)"
    header_content += f"{prototype};\n"

    source_content += f"{prototype} {{\n"
    source_content += f"    switch (match.translation) {{\n"

    for group in translation_model.translations.values():
        for translation in group.translations:
            source_content += f"        case TranslationTag::{translation.tag}:  return emit_translation<TranslationTag::{translation.tag}> (frame, match);\n"

    source_content += "    }\n"
    source_content += "}\n"

    write_header(header_content, output_dir / "emission.h", ["arch/x86_64/allocation.h", "arch/x86_64/execmodel.h", output_dir / "translation_tag.h"])
    write_source(source_content, output_dir / "emission.cpp", [output_dir / "emission.h"] + group_headers)

def generate_execmodel(translation_model: TranslationModel, output_dir: Path):
    generate_locations(translation_model, output_dir)
    generate_translation_tags(translation_model, output_dir)
    generate_group_matcher(translation_model, output_dir)
    generate_translation_matcher(translation_model, output_dir)
    generate_emission(translation_model, output_dir)
