import copy
import itertools

from opcodes.x86_64 import *
from execmodel.constraints import *

class TranslationOperand:
    def __init__(self, description : str, default_index : int):
        self.target_index = default_index
        if ":" in description:
            description, target_index = description.split(":")
            target_index = target_index.strip()
            if target_index == "":
                self.target_index = None
            else:
                self.target_index = int(target_index)

        self.operand_type = description.strip()

class TransferSpec:
    def __init__(self, description: dict[str, object]):
        self.source = TranslationOperand(description["input"], 0)
        self.destination = TranslationOperand(description["output"], 1)
        self.target = description["target"]

class TransferSet:
    def __init__(self, description : list[object]):
        self.specs = [TransferSpec(spec) for spec in description]

class TranslationTarget:
    def __init__(self, description : dict[str, object]):
        self.name = description.get("name", None)
        self.inputs = None  # By default : any operands, let the target match
        self.target_inputs = None  # ir operand index -> asm operand index

        if "input" in description:
            self.inputs = []
            self.target_inputs = {}
            for index, operand in enumerate(description["input"]):
                spec = TranslationOperand(operand, index)
                if spec.target_index is not None:
                    if spec.target_index in self.target_inputs:
                        raise TranslationModelError("Duplicate target index")
                    self.target_inputs[index] = spec.target_index
                self.inputs.append(spec)

            if set(self.target_inputs.values()) != set(range(len(self.target_inputs))):
                raise TranslationModelError("Non-consecutive target indices")

        self.target = description.get("target", None)  # By default, don't generate anything

    def commute(self) -> list[TranslationSpec]:
        if self.inputs is None or len(self.inputs) <= 1:
            return [self]

        options = []
        for permutation in itertools.permutations(self.inputs, len(self.inputs)):
            option = copy.deepcopy(self)
            option.inputs = permutation
            options.append(option)
        return options

class TranslationSpec:
    def __init__(self, operands: list[Constraint], operand_mapping: dict[int, int], form: InstructionForm|None, target_name: str = None):
        self.operands = operands
        self.operand_mapping = operand_mapping
        self.form = form
        self.target_name = target_name

class TranslationModel:
    def __init__(self, description : dict[str, object], instruction_set: dict[str, Instruction]):
        self.locations = description["locations"]

        self.operand_types = {}
        for name, expression in description["operand_types"].items():
            self.operand_types[name] = load_constraint_expression(expression, self.operand_types)

        self.no_reuse = load_constraint_expression(description["no_reuse"], self.operand_types)
        self.no_output = load_constraint_expression(description["no_output"], self.operand_types)
        self.transfers = TransferSet(description["transfers"])

        self.translations = self.parse_translations(description["translations"], instruction_set)

    def parse_translations(self, description, instruction_set: dict[str, Instruction]) -> dict[str, list[TranslationSpec]]:
        translations = {}
        for tag, options in description.items():
            translations[tag] = self.parse_translation_set(tag, options, instruction_set)
        return translations

    def parse_translation_set(self, tag: str, options, instruction_set: dict[str, Instruction]) -> list[TranslationSpec]:
        try:
            targets = []
            for translation in options:
                target = TranslationTarget(translation)
                if translation.get("commutative", False):
                    targets.extend(target.commute())
                else:
                    targets.append(target)

            specs = []
            for target in targets:
                specs.extend(self.make_translation_specs(tag, target, instruction_set))
            return specs

        except TranslationModelError:
            raise TranslationModelError(f"Error in translation set for tag `{tag}`")

    def make_translation_specs(self, tag: str, target: TranslationTarget, instruction_set: dict[str, Instruction]) -> list[TranslationSpec]:
        if target.target is None:
            ir_operands = [self.operand_types[operand.operand_type] for operand in target.inputs]
            return TranslationSpec(ir_operands, {}, None, target.name)

        instruction = instruction_set[target.target]

        specs = []
        for form in instruction.forms:
            spec = self.make_translation_spec(tag, target, form)
            if spec is not None:
                specs.append(spec)
        return specs

    def make_translation_spec(self, tag: str, target: TranslationTarget, form: InstructionForm) -> TranslationSpec|None:
        asm_input_indices  = [index for index, operand in enumerate(form.operands) if operand.is_input]
        asm_output_indices = [index for index, operand in enumerate(form.operands) if operand.is_output]

        operands = []
        operand_mapping = {}
        if target.inputs is None:
            for asm_index, asm_operand in enumerate(form.operands):
                operand_mapping[asm_index] = asm_index
                constraint = self.operand_types[asm_operand.type]
                operands.append(constraint)
        else:
            operand_mapping = target.target_inputs
            for ir_operand in target.inputs:
                constraint = self.operand_types[ir_operand.operand_type]
                if ir_operand.target_index is not None:
                    asm_operand = form.operands[ir_operand.target_index]
                    asm_constraint = self.operand_types[asm_operand.type]
                    conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({constraint, asm_constraint}))
                    constraint = simplify_disjunction(to_dnf(conjunction))

                if not is_false(constraint):
                    operands.append(constraint)

        return TranslationSpec(operands, target.target_inputs, form, target.name)

    def operand_type_enum_value(self, name: str):
        if name == "-1":
            return "minusone"
        elif name == "1":
            return "one"
        elif name == "3":
            return "three"
        else:
            return (name.replace("{", "").replace("}", "").replace("/", ""))

def serialize_model(obj):
    if isinstance(obj, Constraint):
        return obj.serialize()
    elif isinstance(obj, (set, frozenset)):
        return list(obj)
    elif isinstance(obj, InstructionForm):
        return {"name": obj.name, "gas_name": obj.gas_name, "operands": obj.operands,
                "isa_extensions": obj.isa_extensions, "implicit_inputs": obj.implicit_inputs, "implicit_outputs": obj.implicit_outputs}
    else:
        return obj.__dict__
