import copy
import itertools

from opcodes.x86_64 import *
from execmodel.constraints import *

class OperandSpec:
    def __init__(self, description: dict[str, str], operand_types: dict[str, Constraint]):
        if "input" in description:
            self.input_order = []
            self.inputs = {}
            for tag, type_name in description["input"].items():
                self.input_order.append(tag)
                if type_name is not None:
                    self.inputs[tag] = operand_types[type_name]
        else:
            self.input_order = None
            self.inputs = None

        if "output" in description:
            self.output_order = []
            self.outputs = {}
            for tag, type_name in description["output"].items():
                self.output_order.append(tag)
                if type_name is not None:
                    self.outputs[tag] = operand_types[type_name]
        else:
            self.output_order = None
            self.outputs = None

class TargetSpec:
    def __init__(self, description: dict[str, object]):
        self.instruction = description["instruction"]

        self.inputs = None
        self.outputs = None

        if "input" in description:
            self.inputs = description["input"]
        if "output" in description:
            self.outputs = description["output"]

class TransferSpec (OperandSpec):
    def __init__(self, description: dict[str, object], operand_types: dict[str, Constraint]):
        super().__init__(description, operand_types)
        self.target = TargetSpec(description["target"])

class IRSpec:
    def __init__(self, tag: str, description: dict[str, object]):
        self.tag = tag

        self.input = []
        self.output = False
        self.block = False
        self.translate = True

        if "input" in description:
            self.input = description["input"]
        if "output" in description:
            self.output = description["output"]
        if "block" in description:
            self.block = description["block"]
        if "translate" in description:
            self.translate = description["translate"]

class TranslationIRSpec:
    def __init__(self, tag: str, operands: dict[str, Constraint]):
        self.tag = tag
        self.operands = operands

class TranslationTargetSpec:
    def __init__(self, form: InstructionForm, operands: list[tuple[int, str]]):
        self.form = form
        self.operands = operands

class TranslationSpec:
    def __init__(self, ir: list[TranslationIRSpec], target: list[TranslationTargetSpec]):
        self.ir = ir
        self.target = target

class TranslationModel:
    def __init__(self, description: dict[str, object], instruction_set: dict[str, Instruction]):
        self.locations = description["locations"]

        self.operand_types = {}
        for name, expression in description["operand_types"].items():
            self.operand_types[name] = load_constraint_expression(expression, self.operand_types)

        self.no_reuse  = load_constraint_expression(description["no_reuse"], self.operand_types)
        self.no_output = load_constraint_expression(description["no_output"], self.operand_types)

        self.ir = {tag: IRSpec(tag, spec) for tag, spec in description["ir"].items()}
        self.transfers = [TransferSpec(transfer, self.operand_types) for transfer in description["transfers"]]
        self.translations = self.parse_translations(description["translations"], instruction_set)

    def get_type(self, name: str) -> Constraint:
        if name in self.operand_types:
            return self.operand_types[name]
        else:
            raise TranslationModelError(f"Operand type `{name}` is undefined")

    def parse_translations(self, description: list[dict], instruction_set: dict[str, Instruction]) -> list[TranslationSpec]:
        translations = []
        for translation in description:
            try:
                translations.extend(self.parse_translation_set(translation, instruction_set))
            except TranslationModelError:
                raise TranslationModelError(f"Error while generating translation spec for {translation}")
        return translations

    def parse_translation_set(self, description: dict[str, object], instruction_set: dict[str, Instruction]) -> list[TranslationSpec]:
        if not isinstance(description["ir"], list):
            description["ir"] = [description["ir"]]
        if not isinstance(description["target"], list):
            description["target"] = [description["target"]]

        target_forms = []
        for target in description["target"]:
            instruction = instruction_set[target["instruction"]]
            target_forms.append(instruction.forms)

        specs = []
        for target_combination in itertools.product(*target_forms):
            spec = self.make_translation_spec(description["ir"], description["target"], target_combination)
            if spec is not None:
                specs.append(spec)
        return specs

    def make_translation_spec(self, ir_descriptions: list[dict], target_descriptions: list[dict], target_combination: list[InstructionForm]) -> TranslationSpec|None:
        ir_operands = {}
        for index, ir_desc in enumerate(ir_descriptions):
            ir_spec = self.ir[ir_desc["tag"]]

            if ir_spec.output:
                if "output" in ir_desc:
                    ir_operands[f"${index}.output"] = self.get_type(ir_desc["output"])
                else:
                    ir_operands[f"${index}.output"] = CONSTRAINT_TRUE

            for operand_name in ir_spec.input:
                if operand_name in ir_desc:
                    if ir_desc[operand_name].startswith("$"):
                        ir_operands[f"${index}.{operand_name}"] = ir_desc[operand_name]
                    else:
                        ir_operands[f"${index}.{operand_name}"] = self.get_type(ir_desc[operand_name])
                else:
                    ir_operands[f"${index}.{operand_name}"] = CONSTRAINT_TRUE

            for operand_name in ir_desc.keys():
                if operand_name == "tag":
                    continue
                elif operand_name == "output":
                    if not ir_spec.output:
                        raise TranslationModelError(f"IR statement `{ir_spec.tag}` doesn't have an output")
                elif operand_name not in ir_spec.input:
                    raise TranslationModelError(f"Input `{operand_name}` not defined in IR spec for {ir_spec.tag}")

        translation_targets = []
        for index, (target_desc, target_form) in enumerate(zip(target_descriptions, target_combination)):
            if "input" in target_desc:
                nof_required_operands = len(target_desc["input"])
                if "output" in target_desc:
                    nof_required_operands += 1
                if len(target_form.operands) != nof_required_operands:
                    return None
            elif len(ir_descriptions) == 1:
                ir_spec = self.ir[ir_descriptions[0]["tag"]]
                nof_required_operands = len(ir_spec.input)
                if ir_spec.output:
                    nof_required_operands += 1
                if len(target_form.operands) != nof_required_operands:
                    return None

            input_index = 0
            target_operands = []
            for operand in target_form.operands:
                input_id = None
                output_id = None

                if operand.is_output:
                    if "output" in target_desc:
                        output_id = target_desc["output"]
                    elif len(ir_descriptions) == 1:
                        output_id = f"$0.output"
                    else:
                        raise TranslationModelError(f"Output operand for target `{target_form}` can't be deduced")

                if operand.is_input:
                    if "input" in target_desc:
                        if input_index >= len(target_desc["input"]):
                            raise TranslationModelError(f"Input operand `{input_index}` for target `{target_form}` can't be deduced'")
                        input_id = target_desc["input"][input_index]
                    elif len(ir_descriptions) == 1:
                        ir_spec = self.ir[ir_descriptions[0]["tag"]]
                        if input_index >= len(ir_spec.input):
                            raise TranslationModelError(f"Input operand `{input_index}` for target `{target_form}` can't be deduced'")
                        input_id = "$0." + ir_spec.input[input_index]
                    else:
                        raise TranslationModelError(f"Input operand `{input_index}` for target `{target_form}` can't be deduced'")

                    input_index += 1

                if not operand.is_input and not operand.is_output:
                    raise TranslationModelError(f"Operand `{operand}` is not input nor output")

                if input_id is not None:
                    if not input_id.startswith("$"):
                        if len(ir_descriptions) == 1:
                            input_id = "$0." + input_id
                        else:
                            raise TranslationModelError(f"Operand `{input_id}` for target `{target_form}` can't be deduced")
                    if input_id not in ir_operands:
                        raise TranslationModelError(f"Name `{input_id}` for target `{target_form}` is undefined")
                if output_id is not None:
                    if not output_id.startswith("$"):
                        if len(ir_descriptions) == 1:
                            output_id = "$0." + output_id
                        else:
                            raise TranslationModelError(f"Operand `{output_id}` for target `{target_form}` can't be deduced")
                    if output_id not in ir_operands:
                        raise TranslationModelError(f"Name `{output_id}` for target `{target_form}` is undefined")

                main_id = input_id if input_id is not None else output_id
                base_constraint = ir_operands[main_id]
                while isinstance(base_constraint, str):
                    main_id = base_constraint
                    base_constraint = ir_operands[main_id]
                conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({base_constraint, self.operand_types[operand.type]}))
                ir_operands[main_id] = to_simplified_constraint(conjunction)

                ref, _, ir_name = main_id.partition(".")
                ir_index = int(ref.strip("$"))
                target_operands.append((ir_index, ir_name))

                if input_id is not None and output_id is not None:
                    ir_operands[output_id] = input_id

            translation_targets.append(TranslationTargetSpec(target_form, target_operands))

        for operand_id, constraint in ir_operands.items():
            if isinstance(constraint, str):
                ref, _, ir_name = constraint.partition(".")
                ir_index = int(ref.strip("$"))
                ir_operands[operand_id] = (ir_index, ir_name)

        translation_ir = []
        for index, ir_desc in enumerate(ir_descriptions):
            operands = {operand_id.partition(".")[2]: constraint for operand_id, constraint in ir_operands.items()
                                                                 if int(operand_id.partition(".")[0].strip("$")) == index}
            translation_ir.append(TranslationIRSpec(ir_desc["tag"], operands))

        return TranslationSpec(translation_ir, translation_targets)

def serialize_model(obj):
    if isinstance(obj, Constraint):
        return denormalize(obj).serialize()
    elif isinstance(obj, (set, frozenset)):
        return list(obj)
    elif isinstance(obj, InstructionForm):
        return {"name": obj.name, "gas_name": obj.gas_name, "operands": obj.operands}
    else:
        return obj.__dict__
