import copy
import itertools
import collections

from opcodes.x86_64 import *
from execmodel.constraints import *

unique_id = 0

class TransferSpec:
    def __init__(self, form: InstructionForm, source: Constraint, destination: Constraint):
        self.form = form
        self.source = source
        self.destination = destination

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
    def __init__(self, ir: list[TranslationIRSpec], target: list[TranslationTargetSpec], allocations: dict[str, Constraint], subgraph: tuple[tuple[int]]):
        self.ir = ir
        self.target = target
        self.allocations = allocations
        self.allocation_order = tuple(allocations.keys())
        self.subgraph = subgraph

class TranslationGroup:
    def __init__(self, first_translation: TranslationSpec):
        self.statements = tuple(ir.tag for ir in first_translation.ir)
        self.subgraph = first_translation.subgraph

        global unique_id
        self.unique_id = unique_id
        unique_id += 1

        first_translation.tag = f"{self.tag()}_0"
        self.translations = [first_translation]

    def tag(self):
        return "_".join(self.statements) + f"_{self.unique_id}"

    def contains(self, translation: TranslationSpec) -> bool:
        translation_statements = tuple(statement.tag for statement in translation.ir)

        if translation_statements == self.statements and translation.subgraph == self.subgraph:
            return True
        if len(translation.subgraph) != len(self.subgraph) or len(translation.subgraph[0]) != len(self.subgraph[0]):
            return False

        required_statements = collections.Counter(self.statements)
        found_statements = collections.Counter(statement.tag for statement in translation.ir)
        if required_statements != found_statements:
            return False

        allowed_row_permutations = []
        for row_permutation in itertools.permutations(range(len(translation.subgraph))):
            permuted_statements = tuple(translation_statements[row] for row in row_permutation)
            if permuted_statements != self.statements:
                continue

            row_permuted_matrix = tuple(translation.subgraph[row] for row in row_permutation)
            if row_permuted_matrix == self.subgraph:
                return True
            allowed_row_permutations.append(row_permuted_matrix)

        for row_permuted_matrix in allowed_row_permutations:
            for column_permutation in itertools.permutations(range(len(translation.subgraph[0]))):
                col_permuted_matrix = tuple(tuple(row[column] for column in column_permutation) for row in row_permuted_matrix)
                if col_permuted_matrix == self.subgraph:
                    return True
        return False

    def try_add(self, translation: TranslationSpec) -> bool:
        translation_statements = tuple(statement.tag for statement in translation.ir)

        if translation_statements == self.statements and translation.subgraph == self.subgraph:
            return self.add(translation)

        required_statements = collections.Counter(self.statements)
        found_statements = collections.Counter(statement.tag for statement in translation.ir)
        if required_statements != found_statements:
            return False

        if len(translation.subgraph) != len(self.subgraph) or len(translation.subgraph[0]) != len(self.subgraph[0]):
            return False

        allowed_row_permutations = []
        for row_permutation in itertools.permutations(range(len(translation.subgraph))):
            permuted_statements = tuple(translation_statements[row] for row in row_permutation)
            if permuted_statements != self.statements:
                continue

            row_permuted_matrix = tuple(translation.subgraph[row] for row in row_permutation)
            if row_permuted_matrix == self.subgraph:
                return self.add(translation, row_permutation)
            allowed_row_permutations.append(row_permuted_matrix)

            for column_permutation in itertools.permutations(range(len(translation.subgraph[0]))):
                col_permuted_matrix = tuple(tuple(row[column] for column in column_permutation) for row in row_permuted_matrix)
                if col_permuted_matrix == self.subgraph:
                    return self.add(translation, row_permutation)
        return False

    def add(self, translation: TranslationSpec, statement_permutation: list[int] = None) -> True:
        if statement_permutation is not None:
            translation.ir = [translation.ir[row] for row in statement_permutation]
            for statement in translation.ir:
                new_operands = {}
                for name, operand in statement.operands.items():
                    if not isinstance(operand, Constraint):
                        operand = (statement_permutation[operand[0]], operand[1])
                    new_operands[name] = operand
                statement.operands = new_operands

            for target in translation.target:
                new_operands = []
                for initial_index, name in target.operands:
                    new_operands.append((statement_permutation[initial_index], name))
                target.operands = new_operands

        translation.tag = f"{self.tag()}_{len(self.translations)}"
        self.translations.append(translation)
        return True


class TranslationModel:
    def __init__(self, description: dict[str, object], instruction_set: dict[str, Instruction]):
        self.locations = description["locations"]
        self.category_locations = {name: set(locations) for name, locations in description["category_locations"].items()}

        self.operand_types = {}
        for name, expression in description["operand_types"].items():
            self.operand_types[name] = load_constraint_expression(expression, self.constraint_context())

        self.no_reuse  = load_constraint_expression(description["no_reuse"], self.constraint_context())
        self.no_output = load_constraint_expression(description["no_output"], self.constraint_context())

        allowed_allocations = set(self.locations) - set(description["no_allocation"])
        self.allocation_constraint = Constraint(ConstraintType.DISJUNCTION, frozenset(Constraint(ConstraintType.LOCATION, location) for location in allowed_allocations))

        self.ir = {tag: IRSpec(tag, spec) for tag, spec in description["ir"].items()}
        self.transfers = self.parse_transfers(description["transfers"], instruction_set)
        self.translations = self.parse_translations(description["translations"], instruction_set)

    def constraint_context(self) -> ConstraintContext:
        return ConstraintContext(self.operand_types, self.category_locations)

    def get_type(self, name: str) -> Constraint:
        if name in self.operand_types:
            return self.operand_types[name]
        else:
            raise TranslationModelError(f"Operand type `{name}` is undefined")

    def parse_transfers(self, description: list[str], instruction_set: dict[str, Instruction]) -> list[TransferSpec]:
        transfers = []
        for instruction in description:
            transfers.extend(self.parse_transfer_set(instruction_set[instruction]))
        return transfers

    def parse_transfer_set(self, instruction: Instruction) -> list[TransferSpec]:
        transfer_index = 0
        transfers = []
        for form in instruction.forms:
            spec = self.make_transfer_spec(form)
            if spec is not None:
                spec.tag = f"{form.name}_{transfer_index}"
                transfers.append(spec)
                transfer_index += 1
        return transfers

    def make_transfer_spec(self, form: InstructionForm) -> TransferSpec|None:
        source = None
        destination = None
        for operand in form.operands:
            if operand.type not in self.operand_types:
                raise TranslationModelError(f"Operand {operand} of transfer instruction {form} has unknown type {operand.type}")
            constraint = self.operand_types[operand.type]

            if operand.is_input and not operand.is_output:
                if source is None:
                    source = constraint
                else:
                    raise TranslationModelError(f"Transfer instruction form {form} has multiple inputs")
            elif operand.is_output and not operand.is_input:
                if destination is None:
                    destination = constraint
                else:
                    raise TranslationModelError(f"Transfer instruction form {form} has multiple outputs")
            else:
                raise TranslationModelError(f"Operand {operand} of transfer instruction {form} is not only an input or an output")

        if source is None or destination is None:
            raise TranslationModelError(f"Transfer instruction form {form} doesn't have exactly one input and one output")

        if is_false(source) or is_false(destination):
            return None
        else:
            return TransferSpec(form, source, destination)

    def parse_translations(self, description: list[dict], instruction_set: dict[str, Instruction]) -> dict[str, TranslationGroup]:
        groups = []
        for translation_desc in description:
            try:
                translations = self.parse_translation_set(translation_desc, instruction_set)
            except TranslationModelError:
                raise TranslationModelError(f"Error while generating translation spec for {translation_desc}")

            for translation in translations:
                for group in groups:
                    if group.try_add(translation):
                        break
                else:
                    groups.append(TranslationGroup(translation))

        return {group.tag(): group for group in groups}

    def parse_translation_set(self, description: dict[str, object], instruction_set: dict[str, Instruction]) -> list[TranslationSpec]:
        if "ir" not in description or "target" not in description:
            raise TranslationModelError("Translation rules must have at least `ir` and `target` elements")

        if not isinstance(description["ir"], list):
            description["ir"] = [description["ir"]]
        if not isinstance(description["target"], list):
            description["target"] = [description["target"]]

        allocations = {}
        if "allocate" in description:
            for name, constraint in description["allocate"].items():
                allocations[f"${name}"] = load_constraint_expression(constraint, self.operand_types)

        target_forms = []
        for target in description["target"]:
            instruction = instruction_set[target["instruction"]]
            target_forms.append(instruction.forms)

        specs = []
        for target_combination in itertools.product(*target_forms):
            spec = self.make_translation_spec(description["ir"], description["target"], target_combination, allocations.copy())
            if spec is not None:
                specs.append(spec)

        if len(specs) == 0:
            print(f"WARNING : Rule description {description} does not produce any valid translation")
        return specs

    def make_translation_spec(self, ir_descriptions: list[dict], target_descriptions: list[dict], target_combination: list[InstructionForm], allocations: dict[str, Constraint]) -> TranslationSpec|None:
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
            elif "output" in target_desc:
                if len(target_form.operands) != 1 or not target_form.operands[0].is_output:
                    return None
            elif len(ir_descriptions) == 1:
                nof_inputs = len([operand for operand in target_form.operands if operand.is_input])
                nof_outputs = len([operand for operand in target_form.operands if operand.is_output])
                ir_spec = self.ir[ir_descriptions[0]["tag"]]

                if len(ir_spec.input) != nof_inputs:
                    return None
                elif ir_spec.output and nof_outputs != 1:
                    return None
                elif not ir_spec.output and nof_outputs != 0:
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
                    if input_id not in ir_operands and input_id not in allocations:
                        raise TranslationModelError(f"Name `{input_id}` for target `{target_form}` is undefined")
                if output_id is not None:
                    if not output_id.startswith("$"):
                        if len(ir_descriptions) == 1:
                            output_id = "$0." + output_id
                        else:
                            raise TranslationModelError(f"Operand `{output_id}` for target `{target_form}` can't be deduced")
                    if output_id not in ir_operands and output_id not in allocations:
                        raise TranslationModelError(f"Name `{output_id}` for target `{target_form}` is undefined")

                main_id = input_id if input_id is not None else output_id
                if main_id in ir_operands:
                    base_constraint = ir_operands[main_id]
                    while isinstance(base_constraint, str):
                        main_id = base_constraint
                        base_constraint = ir_operands[main_id]
                    conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({base_constraint, self.operand_types[operand.type]}))
                    ir_operands[main_id] = to_simplified_constraint(conjunction)

                    ref, _, ir_name = main_id.partition(".")
                    ir_index = int(ref.strip("$"))
                    target_operands.append((ir_index, ir_name))

                elif main_id in allocations:
                    base_constraint = allocations[main_id]
                    conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({base_constraint, self.operand_types[operand.type], self.allocation_constraint}))
                    allocations[main_id] = to_simplified_constraint(conjunction)
                    target_operands.append(("allocations", main_id))

                if input_id is not None and output_id is not None:
                    if input_id in ir_operands:
                        ir_operands[output_id] = input_id
                    elif input_id in allocations:
                        allocations[output_id] = output_id

            translation_targets.append(TranslationTargetSpec(target_form, target_operands))

        for operand_id, constraint in ir_operands.items():
            if isinstance(constraint, str):
                ref, _, ir_name = constraint.partition(".")
                ir_index = int(ref.strip("$"))
                ir_operands[operand_id] = (ir_index, ir_name)
            elif is_false(constraint):
                return None

        for name, constraint in allocations.items():
            if is_false(constraint):
                return None

        translation_ir = []
        for index, ir_desc in enumerate(ir_descriptions):
            operands = {operand_id.partition(".")[2]: constraint for operand_id, constraint in ir_operands.items()
                                                                 if int(operand_id.partition(".")[0].strip("$")) == index}
            translation_ir.append(TranslationIRSpec(ir_desc["tag"], operands))

        subgraph = self.make_subgraph(translation_ir)
        return TranslationSpec(translation_ir, translation_targets, allocations, subgraph)

    def make_subgraph(self, translation_ir: list[TranslationIRSpec]) -> tuple[tuple[int]]:
        if len(translation_ir) <= 1:
            return ()

        ir_specs = [self.ir[statement.tag] for statement in translation_ir]
        columns = []
        column_indices = {}

        # Handle new operands and register their ids
        for statement_index, statement in enumerate(translation_ir):
            for name, operand in statement.operands.items():
                if isinstance(operand, Constraint):
                    column_indices[(statement_index, name)] = len(columns)
                    columns.append([0] * len(translation_ir))
                    operand_index = -1 if name == "output" else 1 + ir_specs[statement_index].input.index(name)
                    columns[-1][statement_index] = operand_index

        # Handle reference operands
        for statement_index, statement in enumerate(translation_ir):
            for name, operand in statement.operands.items():
                if not isinstance(operand, Constraint):
                    column_index = column_indices[tuple(operand)]
                    operand_index = -1 if name == "output" else 1 + ir_specs[statement_index].input.index(name)
                    columns[column_index][statement_index] = operand_index

        # Remove values that aren't links (= not relevant in matching)
        filtered_columns = []
        for column in columns:
            nof_uses = len([cell for cell in column if cell != 0])
            if nof_uses >= 2:
                filtered_columns.append(column)

        # Transpose the matrix
        matrix = []
        for row in range(len(translation_ir)):
            matrix.append(tuple(column[row] for column in filtered_columns))
        return tuple(matrix)


def serialize_model(obj):
    if isinstance(obj, Constraint):
        return denormalize(obj).serialize()
    elif isinstance(obj, (set, frozenset)):
        return list(obj)
    elif isinstance(obj, InstructionForm):
        return str(obj)
    else:
        return obj.__dict__
