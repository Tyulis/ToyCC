import os
import sys
sys.path.append(os.path.join("lib", "opcodes-x86_64"))

import json
import pprint
from opcodes.x86_64 import *
from execmodel.model import *
from execmodel.generator import generate_execmodel

def preprocess_instruction_form(form: InstructionForm) -> InstructionForm:
    has_output = any(operand.is_output for operand in form.operands)
    for operand in form.operands:
        if not operand.is_input and not operand.is_output:
            if operand.type in ("rel8", "rel32", "rel32m"):
                operand.is_input = True
            elif not has_output and operand.is_variable:
                operand.is_output = True
            else:
                operand.is_input = True
    return form

def preprocess_instruction(instruction: Instruction) -> Instruction:
    for form in instruction.forms:
        preprocess_instruction_form(form)
    return instruction

if __name__ == "__main__":
    instruction_list = read_instruction_set(os.path.join("lib", "opcodes-x86_64", "opcodes", "x86_64.xml"))
    instruction_set = {instruction.name: preprocess_instruction(instruction) for instruction in instruction_list}

    with open(os.path.join("execmodel", "x86_64.json")) as f:
        translation_model = TranslationModel(json.load(f), instruction_set)

    output_dir = os.path.join("gen", "execmodel", "x86_64")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(os.path.join("gen", "execmodel", "x86_64", "translation_model.py"), "w") as f:
        pprint.pprint(json.loads(json.dumps(translation_model, default=serialize_model)),
                      compact=True, stream=f, width=140, sort_dicts=True)


    generate_execmodel(translation_model, output_dir)
