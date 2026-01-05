import os
import sys
sys.path.append(os.path.join("lib", "opcodes-x86_64"))

import json
import pprint
from opcodes.x86_64 import *
from execmodel.model import *
from execmodel.generator import generate_execmodel

if __name__ == "__main__":
    instruction_list = read_instruction_set(os.path.join("lib", "opcodes-x86_64", "opcodes", "x86_64.xml"))
    instruction_set = {instruction.name: instruction for instruction in instruction_list}

    with open(os.path.join("execmodel", "x86_64.json")) as f:
        translation_model = TranslationModel(json.load(f), instruction_set)

    output_dir = os.path.join("gen", "execmodel", "x86_64")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(os.path.join("gen", "execmodel", "x86_64", "translation_model.py"), "w") as f:
        pprint.pprint(json.loads(json.dumps(translation_model, default=serialize_model)),
                      compact=True, stream=f, width=140, sort_dicts=True)


    # generate_execmodel(translation_model, instruction_set, output_dir)
