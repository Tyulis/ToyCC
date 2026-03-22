import copy
import itertools
import collections

from opcodes.x86_64 import *
from execmodel.constraints import *

unique_id = 0

class CustomHook:
    def __init__(self, description: dict[str, str]):
        self.header   : str = description["header"]
        self.function : str = description["function"]

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
        self.commutative = False

        if "input" in description:
            self.input = description["input"]
        if "output" in description:
            self.output = description["output"]
        if "block" in description:
            self.block = description["block"]
        if "translate" in description:
            self.translate = description["translate"]
        if "commutative" in description:
            self.commutative = description["commutative"]

class TranslationIRSpec:
    def __init__(self, tag: str, operands: dict[str, Constraint|tuple[int, str]]):
        self.tag = tag
        self.operands = operands

class TranslationTargetSpec:
    def __init__(self, form: InstructionForm, operands: list[tuple[int, str]], implicit_inputs: dict[str, tuple[int, str]], implicit_outputs: dict[str, tuple[int, str]],
                 ir_operands: dict[str, Constraint|str], allocations: dict[str, Constraint|str], allocation_use: dict[str, bool]):
        self.form = form
        self.operands = operands
        self.implicit_inputs = implicit_inputs
        self.implicit_outputs = implicit_outputs
        self.ir_operands = ir_operands
        self.allocations = allocations
        self.allocation_use = allocation_use

class TranslationSpec:
    def __init__(self, ir: list[TranslationIRSpec], target: list[TranslationTargetSpec]|CustomHook, allocations: dict[str, Constraint], subgraph: tuple[tuple[int]]):
        self.ir = ir
        self.target = target
        self.allocations = allocations
        self.allocation_order = tuple(allocations.keys())
        self.subgraph = subgraph

class BaseTranslationGroup:
    def __init__(self):
        global unique_id
        self.unique_id : int = unique_id
        unique_id += 1

    def tag(self):
        return "_".join(self.statements) + f"_{self.unique_id}"

class CustomIRSpec (BaseTranslationGroup):
    def __init__(self, tag: str, matcher: CustomHook, emitter: CustomHook):
        super().__init__()
        self.matcher    : CustomHook = matcher
        self.emitter    : CustomHook = emitter
        self.statements : tuple[str] = (tag, )

class TranslationGroup (BaseTranslationGroup):
    def __init__(self, first_translation: TranslationSpec):
        super().__init__()
        self.statements = tuple(ir.tag for ir in first_translation.ir)
        self.subgraph = first_translation.subgraph

        first_translation.tag = f"{self.tag()}_0"
        self.translations = [first_translation]

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
        self.register_locations = description["register_locations"]
        self.register_sizes = description["register_sizes"]

        self.operand_types = {}
        for name, expression in description["operand_types"].items():
            self.operand_types[name] = load_constraint_expression(expression, self.constraint_context())

        allowed_outputs = set(self.locations) - set(description["no_output"])
        self.output_constraint = Constraint(ConstraintType.DISJUNCTION, frozenset(Constraint(ConstraintType.LOCATION, location) for location in allowed_outputs))

        allowed_allocations = set(self.locations) - set(description["no_allocation"])
        self.allocation_constraint = Constraint(ConstraintType.DISJUNCTION, frozenset(Constraint(ConstraintType.LOCATION, location) for location in allowed_allocations))

        self.banked_locations = set(description["banked_locations"])

        self.ir = {}
        custom_ir = []
        for tag, spec in description["ir"].items():
            if "matcher" in spec and "emitter" in spec:
                custom_ir.append(CustomIRSpec(tag, CustomHook(spec["matcher"]), CustomHook(spec["emitter"])))
            else:
                self.ir[tag] = IRSpec(tag, spec)

        self.transfers = self.parse_transfers(description["transfers"], instruction_set)
        self.translations = self.parse_translations(description["translations"], instruction_set)
        for group in custom_ir:
            self.translations[group.tag()] = group

    def constraint_context(self) -> ConstraintContext:
        return ConstraintContext(self.operand_types, self.category_locations)

    def get_type(self, name: str) -> Constraint:
        if name in self.operand_types:
            return self.operand_types[name]
        else:
            raise TranslationModelError(f"Operand type `{name}` is undefined")

    # -------- Transfers
    def parse_transfers(self, description: list[dict], instruction_set: dict[str, Instruction]) -> list[TransferSpec]:
        transfers = []
        for transfer in description:
            transfers.extend(self.parse_transfer_set(transfer, instruction_set[transfer["target"]]))
        return transfers

    def parse_transfer_set(self, description: dict[str, object], instruction: Instruction) -> list[TransferSpec]:
        transfer_index = 0
        transfers = []
        for form in instruction.forms:
            spec = self.make_transfer_spec(description, form)
            if spec is not None:
                spec.tag = f"{form.name}_{transfer_index}"
                transfers.append(spec)
                transfer_index += 1
        return transfers

    def make_transfer_spec(self, description: dict[str, object], form: InstructionForm) -> TransferSpec|None:
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

        if "source" in description:
            constraint = load_constraint_expression(description["source"], self.constraint_context())
            source = to_simplified_constraint(Constraint(ConstraintType.CONJUNCTION, frozenset({source, constraint})))
        if "destination" in description:
            constraint = load_constraint_expression(description["destination"], self.constraint_context())
            destination = to_simplified_constraint(Constraint(ConstraintType.CONJUNCTION, frozenset({destination, constraint})))

        if is_false(source) or is_false(destination):
            return None
        else:
            return TransferSpec(form, source, destination)

    # -------- Translations
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

    def parse_allocations(self, allocate_description: dict[str, object]) -> dict[str, Constraint]:
        allocations = {}
        for name, constraint in allocate_description.items():
            allocations[f"${name}"] = load_constraint_expression(constraint, self.constraint_context())
        return allocations

    def parse_operand_constraint(self, description: str|dict[str, object]) -> Constraint:
        if isinstance(description, str):
            return self.get_type(description)
        else:
            return load_constraint_expression(description, self.constraint_context())

    def resolve_operand(self, ir_operands: dict[str, Constraint|str], operand_id: str) -> tuple[str, Constraint]:
        constraint = ir_operands[operand_id]
        while isinstance(constraint, str):
            operand_id = constraint
            constraint = ir_operands[operand_id]
        return operand_id, constraint

    def make_ir_operands(self, ir_descriptions: list[dict]) -> dict[str, Constraint]:
        ir_operands = {}
        for index, ir_desc in enumerate(ir_descriptions):
            ir_spec = self.ir[ir_desc["tag"]]

            if ir_spec.output:
                if "output" in ir_desc:
                    conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({self.parse_operand_constraint(ir_desc["output"]), self.output_constraint}))
                    ir_operands[f"${index}.output"] = to_simplified_constraint(conjunction)
                else:
                    ir_operands[f"${index}.output"] = self.output_constraint

            for operand_name in ir_spec.input:
                if operand_name in ir_desc:
                    if isinstance(ir_desc[operand_name], str) and ir_desc[operand_name].startswith("$"):
                        ir_operands[f"${index}.{operand_name}"] = ir_desc[operand_name]
                    else:
                        ir_operands[f"${index}.{operand_name}"] = self.parse_operand_constraint(ir_desc[operand_name])
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

        return ir_operands

    def parse_translation_set(self, description: dict[str, object], instruction_set: dict[str, Instruction]) -> list[TranslationSpec]:
        if "ir" not in description or "target" not in description:
            raise TranslationModelError("Translation rules must have at least `ir` and `target` elements")

        if not isinstance(description["ir"], list):
            description["ir"] = [description["ir"]]

        ir_specs = [self.ir[ir_desc["tag"]] for ir_desc in description["ir"]]
        ir_operands = self.make_ir_operands(description["ir"])
        allocations = self.parse_allocations(description["allocate"]) if "allocate" in description else {}

        if not isinstance(description["target"], list):
            if "custom" in description["target"] and description["target"]["custom"]:
                target = self.parse_custom_target(description["target"])
                translation = self.make_translation_spec(ir_specs, ir_operands, target, allocations)
                return [] if translation is None else [translation]
            else:
                description["target"] = [description["target"]]

        target_sequence = []
        for target_desc in description["target"]:
            instruction = instruction_set[target_desc["instruction"]]
            options = []
            for target_form in instruction.forms:
                target = self.parse_target(ir_specs, target_desc, target_form, list(ir_operands.keys()), list(allocations.keys()))
                if target is not None:
                    options.append(target)

            if len(options) == 0:
                raise TranslationModelError(f"Target description {description} does not produce any valid instruction")
            target_sequence.append(options)

        specs = []
        for target_combination in itertools.product(*target_sequence):
            if not self.uses_all_allocations(allocations, target_combination):
                continue

            combined_operands    = self.combine_operands(ir_operands, [target.ir_operands for target in target_combination])
            combined_allocations = self.combine_operands(allocations, [target.allocations for target in target_combination])

            spec = self.make_translation_spec(ir_specs, combined_operands, target_combination, combined_allocations)
            if spec is not None:
                specs.append(spec)

        if len(specs) == 0:
            raise TranslationModelError(f"Rule description {description} does not produce any valid translation")
        return specs

    def uses_all_allocations(self, allocations: dict[str, Constraint], target_combination: list[TranslationTargetSpec]) -> bool:
        for name in allocations:
            if not any(target.allocation_use[name] for target in target_combination):
                return False
        return True

    # Combine the constraints on the IR operands according to the IR itself and every target
    def combine_operands(self, ir_operands: dict[str, Constraint], operands: list[dict[str, Constraint|str]]) -> dict[str, Constraint|str]:
        operand_names = tuple(operands[0].keys())
        crossed = {operand_name: set(target[operand_name] for target in operands) for operand_name in operand_names}  # crossed[operand_name] = Constraint for each target

        references = {}
        constraints = {}
        for operand_name in operand_names:
            operand_constraints = set(constraint for constraint in crossed[operand_name] if isinstance(constraint, Constraint))
            operand_references = set(constraint for constraint in {ir_operands[operand_name]} | crossed[operand_name] if isinstance(constraint, str))

            if len(operand_references) == 0:
                operand_constraints.add(ir_operands[operand_name])
                if operand_name in constraints:
                    constraints[operand_name] |= operand_constraints
                else:
                    constraints[operand_name] = operand_constraints
            elif len(operand_references) == 1:
                if not all(is_true(constraint) for constraint in operand_constraints):
                    raise TranslationModelError(f"Operand `{operand_name}` must be true or the same reference for all targets")

                reference = operand_references.pop()
                references[operand_name] = reference
                if isinstance(ir_operands[operand_name], Constraint):
                    if reference in constraints:
                        constraints[reference].add(ir_operands[operand_name])
                    else:
                        constraints[reference] = {ir_operands[operand_name]}
            else:
                raise TranslationModelError(f"Operand `{operand_name}` must be the same reference for all targets")


        for name in tuple(references.keys()):
            actual_id = name
            conjunction = constraints.get(actual_id, {CONSTRAINT_TRUE})
            while actual_id in references:
                actual_id = references[actual_id]
                conjunction |= constraints.get(actual_id, {CONSTRAINT_TRUE})

            constraints[actual_id] = conjunction
            references[name] = actual_id

        combined = {}
        for operand_name in operand_names:
            if operand_name in references:
                combined[operand_name] = references[operand_name]
            else:
                conjunction = constraints.get(operand_name, {CONSTRAINT_TRUE})
                combined[operand_name] = to_simplified_constraint(Constraint(ConstraintType.CONJUNCTION, frozenset(conjunction)))
        return combined


    def parse_custom_target(self, target_description: dict[str, object]) -> CustomHook:
        return CustomHook(target_description)

    def parse_target(self, ir_specs: list[IRSpec], target_desc: dict[str, object], target_form: InstructionForm, ir_operands_ids: list[str], allocations_ids: list[str]) -> TranslationTargetSpec|None:
        ir_operands = {name: CONSTRAINT_TRUE for name in ir_operands_ids}
        allocations = {name: CONSTRAINT_TRUE for name in allocations_ids}
        allocation_use = {name: False for name in allocations_ids}

        if "input" in target_desc:
            nof_required_operands = len(target_desc["input"])
            if "output" in target_desc:
                nof_required_operands += 1
            if len(target_form.operands) != nof_required_operands:
                return None
        elif "output" in target_desc:
            if len(target_form.operands) != 1 or not target_form.operands[0].is_output:
                return None
        elif len(ir_specs) == 1:
            nof_inputs = len([operand for operand in target_form.operands if operand.is_input])
            nof_outputs = len([operand for operand in target_form.operands if operand.is_output])

            if len(ir_specs[0].input) != nof_inputs:
                return None
            elif ir_specs[0].output and nof_outputs != 1:
                return None
            elif not ir_specs[0].output and nof_outputs != 0:
                return None

        input_index = 0
        target_operands = []
        for operand in target_form.operands:
            input_id = None
            output_id = None

            if operand.is_output:
                if "output" in target_desc:
                    output_id = target_desc["output"]
                elif len(ir_specs) == 1:
                    output_id = f"$0.output"
                else:
                    raise TranslationModelError(f"Output operand for target `{target_form}` can't be deduced")

            if operand.is_input:
                if "input" in target_desc:
                    if input_index >= len(target_desc["input"]):
                        raise TranslationModelError(f"Input operand `{input_index}` for target `{target_form}` can't be deduced'")
                    input_id = target_desc["input"][input_index]
                elif len(ir_specs) == 1:
                    if input_index >= len(ir_specs[0].input):
                        raise TranslationModelError(f"Input operand `{input_index}` for target `{target_form}` can't be deduced'")
                    input_id = "$0." + ir_specs[0].input[input_index]
                else:
                    raise TranslationModelError(f"Input operand `{input_index}` for target `{target_form}` can't be deduced'")

                input_index += 1

            if not operand.is_input and not operand.is_output:
                raise TranslationModelError(f"Operand `{operand}` is not input nor output")

            if input_id is not None:
                if not input_id.startswith("$"):
                    if len(ir_specs) == 1:
                        input_id = "$0." + input_id
                    else:
                        raise TranslationModelError(f"Operand `{input_id}` for target `{target_form}` can't be deduced")
                if input_id not in ir_operands and input_id not in allocations:
                    raise TranslationModelError(f"Name `{input_id}` for target `{target_form}` is undefined")
            if output_id is not None:
                if not output_id.startswith("$"):
                    if len(ir_specs) == 1:
                        output_id = "$0." + output_id
                    else:
                        raise TranslationModelError(f"Operand `{output_id}` for target `{target_form}` can't be deduced")
                if output_id not in ir_operands and output_id not in allocations:
                    raise TranslationModelError(f"Name `{output_id}` for target `{target_form}` is undefined")

            main_id = input_id if input_id is not None else output_id
            if main_id in ir_operands:
                main_id, base_constraint = self.resolve_operand(ir_operands, main_id)
                conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({base_constraint, self.operand_types[operand.type]}))
                ir_operands[main_id] = to_simplified_constraint(conjunction)
                # These constraints could be simplified all at once in `combine_operands`, but since the algorithm is exponential, it's much more efficient to do it in small parts

                ref, _, ir_name = main_id.partition(".")
                ir_index = int(ref.strip("$"))
                target_operands.append((ir_index, ir_name))

            elif main_id in allocations:
                base_constraint = allocations[main_id]
                conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset({base_constraint, self.operand_types[operand.type], self.allocation_constraint}))
                allocations[main_id] = to_simplified_constraint(conjunction)
                allocation_use[main_id] = True
                target_operands.append(("allocations", main_id))

            if input_id is not None and output_id is not None and input_id != output_id:
                if input_id in ir_operands:
                    ir_operands[output_id] = input_id

        target_implicit_operands = target_form.implicit_inputs | target_form.implicit_outputs
        if len(target_implicit_operands) != 0 and "implicit" not in target_desc:
            return None
        elif "implicit" in target_desc and len(target_desc["implicit"]) != len(target_implicit_operands):
            return None

        implicit_inputs = {}
        implicit_outputs = {}
        for register in target_form.implicit_inputs | target_form.implicit_outputs:
            location = self.register_locations[register]
            size     = self.register_sizes[register]

            if register in target_desc["implicit"]:
                main_id = target_desc["implicit"][register]
            elif location in target_desc["implicit"]:
                main_id = target_desc["implicit"][location]
            else:
                return None

            is_inout = False
            if not isinstance(main_id, str):
                if not "output" in main_id or len(main_id) != 2:
                    raise TranslationModelError(f"Multiple references for implicit operands is only allowed for one operand and `output`")
                is_inout = True
                main_id = main_id[0] if main_id[1] == "output" else main_id[1]

            if main_id not in ir_operands and main_id not in allocations:
                if "$" + main_id in allocations:
                    main_id = "$" + main_id
                elif len(ir_specs) == 1 and "$0." + main_id in ir_operands:
                    main_id = "$0." + main_id
                else:
                    raise TranslationModelError(f"Name `{main_id}` for implicit operand `{register}` is undefined")

            constraint = {Constraint(ConstraintType.LOCATION, location), Constraint(ConstraintType.SIZE, size)}
            if main_id in ir_operands:
                base_constraint = ir_operands[main_id]
                while isinstance(base_constraint, str):
                    main_id = base_constraint
                    base_constraint = ir_operands[main_id]
                constraint.add(base_constraint)
                conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset(constraint))
                ir_operands[main_id] = to_simplified_constraint(conjunction)

                ref, _, ir_name = main_id.partition(".")
                ir_index = int(ref.strip("$"))

                if register in target_form.implicit_inputs:
                    implicit_inputs[register] = (ir_index, ir_name)
                if register in target_form.implicit_outputs:
                    implicit_outputs[register] = (ir_index, ir_name)

            elif main_id in allocations:
                constraint.add(allocations[main_id])
                conjunction = Constraint(ConstraintType.CONJUNCTION, frozenset(constraint))
                allocations[main_id] = to_simplified_constraint(conjunction)
                allocation_use[main_id] = True

                if register in target_form.implicit_inputs:
                    implicit_inputs[register] = ("allocations", main_id)
                if register in target_form.implicit_outputs:
                    implicit_outputs[register] = ("allocations", main_id)

            if is_inout:
                if len(ir_specs) == 1:
                    output_id = "$0.output"
                else:
                    raise NotImplementedError(f"Deduced output operands among multiple statements")

                if main_id in ir_operands:
                    ir_operands[output_id] = main_id
                elif main_id in allocations:
                    allocations[output_id] = main_id
                    allocation_use[output_id] = True

        return TranslationTargetSpec(target_form, target_operands, implicit_inputs, implicit_outputs, ir_operands, allocations, allocation_use)

    def make_translation_spec(self, ir_specs: list[IRSpec], ir_operands: dict[str, Constraint], translation_targets: list[TranslationTargetSpec]|CustomHook, allocations: dict[str, Constraint]) -> TranslationSpec|None:
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
        for index, ir_spec in enumerate(ir_specs):
            operands = {operand_id.partition(".")[2]: constraint for operand_id, constraint in ir_operands.items()
                                                                 if int(operand_id.partition(".")[0].strip("$")) == index}
            translation_ir.append(TranslationIRSpec(ir_spec.tag, operands))

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
