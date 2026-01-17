import enum
import itertools

class TranslationModelError (Exception):
    pass

class ConstraintType (enum.IntEnum):
    CONSTANT = 0
    CONJUNCTION = 1
    DISJUNCTION = 2
    CATEGORY = 3
    TYPE = 4
    LOCATION = 5
    SIZE = 6
    VALUE_EQ = 7
    VALUE_LE = 8
    VALUE_GE = 9
    STORAGE = 10

CONSTRAINT_TYPE_NAME = {
    ConstraintType.CONSTANT:    "constant",
    ConstraintType.CONJUNCTION: "inter",
    ConstraintType.DISJUNCTION: "union",
    ConstraintType.CATEGORY:    "category",
    ConstraintType.TYPE:        "type",
    ConstraintType.LOCATION:    "location",
    ConstraintType.SIZE:        "size",
    ConstraintType.VALUE_EQ:    "value",
    ConstraintType.VALUE_LE:    "value_le",
    ConstraintType.VALUE_GE:    "value_ge",
    ConstraintType.STORAGE:     "storage",
}

class Constraint:
    def __init__(self, type, parameter):
        self.type = type
        self.parameter = parameter

    def __str__(self):
        if self.type in (ConstraintType.CONJUNCTION, ConstraintType.DISJUNCTION):
            return f"{self.type.name}({', '.join(str(subexpression) for subexpression in self.parameter)})"
        else:
            return f"{self.type.name}({str(self.parameter)})"
    def __repr__(self):
        return str(self)

    def __hash__(self):
        return hash(self.type) ^ (hash(self.parameter) << 1)
    def __eq__(self, rhs: Constraint) -> bool:
        return self.type == rhs.type and self.parameter == rhs.parameter

    def serialize(self) -> dict[str, object]:
        if self.type in (ConstraintType.DISJUNCTION, ConstraintType.CONJUNCTION) and len(self.parameter) == 1:
            for constraint in self.parameter:
                return constraint
        elif self.type == ConstraintType.CONSTANT:
            return self.parameter
        else:
            return {CONSTRAINT_TYPE_NAME[self.type]: self.parameter}

class ConstraintContext:
    def __init__(self, operand_types: dict[str, Constraint], category_locations: dict[str, set[str]]):
        self.operand_types      : dict[str, Constraint] = operand_types
        self.category_locations : dict[str, set[str]]   = category_locations

    def get_type(self, name: str) -> Constraint:
        if name in self.operand_types:
            return self.operand_types[name]
        else:
            raise TranslationModelError(f"New constraints can only reference operand types defined earlier : `{name}` was not defined previously")

    def make_category_constraint(self, category: str) -> Constraint:
        if category not in self.category_locations:
            raise TranslationModelError(f"Invalid value category `{category}`")

        disjunction = set()
        for location in self.category_locations[category]:
            disjunction.add(Constraint(ConstraintType.LOCATION, location))

        return Constraint(ConstraintType.CONJUNCTION, frozenset({
            Constraint(ConstraintType.CATEGORY, category),
            Constraint(ConstraintType.DISJUNCTION, frozenset(disjunction))
        }))


CONSTRAINT_TRUE  = Constraint(ConstraintType.CONSTANT, True)
CONSTRAINT_FALSE = Constraint(ConstraintType.CONSTANT, False)

def parse_constraint_union(expressions: list[str], context: ConstraintContext) -> Constraint:
    disjunction = {parse_constraint_expression(expression, context) for expression in expressions}
    return Constraint(ConstraintType.DISJUNCTION, frozenset(disjunction))

def parse_constraint_category(categories: str|list[str], context: ConstraintContext) -> Constraint:
    if isinstance(categories, str):
        return context.make_category_constraint(categories)

    disjunction = set()
    for category in categories:
        disjunction.add(context.make_category_constraint(category))
    return Constraint(ConstraintType.DISJUNCTION, frozenset(Constraint(ConstraintType.CATEGORY, category) for category in categories))

def parse_constraint_type(types: str|list[str], context: ConstraintContext) -> Constraint:
    if isinstance(types, str):
        return Constraint(ConstraintType.TYPE, types)
    return Constraint(ConstraintType.DISJUNCTION, frozenset(Constraint(ConstraintType.TYPE, type) for type in types))

def parse_constraint_value(value, context: ConstraintContext) -> Constraint:
    conjunction = {context.make_category_constraint("constant"), Constraint(ConstraintType.VALUE_EQ, value)}
    if isinstance(value, int):
        conjunction.add(Constraint(ConstraintType.TYPE, "INTEGER"))
    elif isinstance(value, float):
        conjunction.add(Constraint(ConstraintType.TYPE, "FLOAT"))
    else:
        raise TranslationModelError(f"Invalid type for constant value `{value}`")
    return Constraint(ConstraintType.CONJUNCTION, frozenset(conjunction))

def parse_constraint_location(locations, context: ConstraintContext) -> Constraint:
    if isinstance(locations, str):
        return Constraint(ConstraintType.LOCATION, locations)

    disjunction = {Constraint(ConstraintType.LOCATION, location) for location in locations}
    return Constraint(ConstraintType.DISJUNCTION, frozenset(disjunction))

def parse_constraint_size(size: int, context: ConstraintContext) -> Constraint:
    return Constraint(ConstraintType.SIZE, size)

def parse_constraint_value_bits(bits: int, context: ConstraintContext) -> Constraint:
    min_signed_value = -2**(bits-1)
    max_unsigned_value = 2**bits - 1
    conjunction = {context.make_category_constraint("constant"), Constraint(ConstraintType.TYPE, "INTEGER"),
                   Constraint(ConstraintType.VALUE_GE, min_signed_value), Constraint(ConstraintType.VALUE_LE, max_unsigned_value)}
    return Constraint(ConstraintType.CONJUNCTION, frozenset(conjunction))

def parse_constraint_anyof(type_names: list[str], context: ConstraintContext) -> Constraint:
    if isinstance(type_names, str):
        return context.get_type(type_names)
    return Constraint(ConstraintType.DISJUNCTION, frozenset(context.get_type(name) for name in type_names))

def parse_constraint_storage(storages: str|list[str], context: ConstraintContext) -> Constraint:
    if isinstance(storages, str):
        return Constraint(ConstraintType.STORAGE, storages)
    return Constraint(ConstraintType.DISJUNCTION, frozenset(Constraint(ConstraintType.STORAGE, storage) for storage in storages))

def parse_constraint_expression(expression, context: ConstraintContext) -> Constraint:
    if isinstance(expression, bool):
        return Constraint(ConstraintType.CONSTANT, expression)

    conjunction = set()
    for operator, value in expression.items():
        subexpression = {"union"     : parse_constraint_union,
                         "category"  : parse_constraint_category,
                         "type"      : parse_constraint_type,
                         "value"     : parse_constraint_value,
                         "location"  : parse_constraint_location,
                         "size"      : parse_constraint_size,
                         "value_bits": parse_constraint_value_bits,
                         "anyof"     : parse_constraint_anyof,
                         "storage"   : parse_constraint_storage,
        }[operator](value, context)
        conjunction.add(subexpression)

    return Constraint(ConstraintType.CONJUNCTION, frozenset(conjunction))

def to_dnf(expression: Constraint) -> Constraint:
    if expression.type == ConstraintType.DISJUNCTION:
        return Constraint(ConstraintType.DISJUNCTION, frozenset(se for e in expression.parameter for se in to_dnf(e).parameter))
    elif expression.type == ConstraintType.CONJUNCTION:
        total = set()
        for c in itertools.product(*[to_dnf(e).parameter for e in expression.parameter]):
            total.add(Constraint(ConstraintType.CONJUNCTION, frozenset(se for e in c for se in e.parameter)))
        return Constraint(ConstraintType.DISJUNCTION, frozenset(total))
    else:
        return Constraint(ConstraintType.DISJUNCTION, frozenset({Constraint(ConstraintType.CONJUNCTION, frozenset({expression}))}))

def simplify_pairwise_and(left: Constraint, right: Constraint) -> Constraint|None:
    # The AND operator is commutative, sort by type to avoid duplicate cases
    if left.type > right.type:
        left, right = right, left

    match (left.type, right.type):
        case (ConstraintType.CONSTANT, _):
            if left.parameter == True:
                return right
            else:
                return CONSTRAINT_FALSE
        case (ConstraintType.CONJUNCTION, _) | (ConstraintType.DISJUNCTION, _):
            raise TranslationModelError("Constraints must be in disjunctive normal form before simplifying them")
        case (ConstraintType.CATEGORY, ConstraintType.CATEGORY) | (ConstraintType.TYPE, ConstraintType.TYPE) | (ConstraintType.LOCATION, ConstraintType.LOCATION) | (ConstraintType.SIZE, ConstraintType.SIZE) | (ConstraintType.VALUE_EQ, ConstraintType.VALUE_EQ) | (ConstraintType.STORAGE, ConstraintType.STORAGE):
            if left.parameter == right.parameter:
                return left
            else:
                return CONSTRAINT_FALSE
        case (ConstraintType.VALUE_EQ, ConstraintType.VALUE_LE):
            if left.parameter <= right.parameter:
                return left
            else:
                return CONSTRAINT_FALSE
        case (ConstraintType.VALUE_EQ, ConstraintType.VALUE_GE):
            if left.parameter >= right.parameter:
                return left
            else:
                return CONSTRAINT_FALSE
        case (ConstraintType.VALUE_LE, ConstraintType.VALUE_LE):
            return Constraint(ConstraintType.VALUE_LE, min(left.parameter, right.parameter))
        case (ConstraintType.VALUE_LE, ConstraintType.VALUE_GE):
            if left.parameter == right.parameter:
                return Constraint(ConstraintType.VALUE_EQ, left.parameter)
            elif left.parameter < right.parameter:
                return CONSTRAINT_FALSE
            elif left.parameter > right.parameter:
                return None
        case (ConstraintType.VALUE_GE, ConstraintType.VALUE_GE):
            return Constraint(ConstraintType.VALUE_GE, max(left.parameter, right.parameter))
        case _:
            return None

def simplify_pairwise_or(left: Constraint, right: Constraint) -> Constraint|None:
    # The OR operator is commutative, sort by type to avoid duplicate cases
    if left.type > right.type:
        left, right = right, left

    match (left.type, right.type):
        case (ConstraintType.CONSTANT, _):
            if left.parameter == False:
                return right
            else:
                return CONSTANT_TRUE
        case (ConstraintType.CONJUNCTION, _) | (ConstraintType.DISJUNCTION, _):
            raise TranslationModelError("Constraints must be in disjunctive normal form before simplifying them")
        case (ConstraintType.CATEGORY, ConstraintType.CATEGORY) | (ConstraintType.TYPE, ConstraintType.TYPE) | (ConstraintType.LOCATION, ConstraintType.LOCATION) | (ConstraintType.SIZE, ConstraintType.SIZE) | (ConstraintType.VALUE_EQ, ConstraintType.VALUE_EQ) | (ConstraintType.STORAGE, ConstraintType.STORAGE):
            if left.parameter == right.parameter:
                return left
            else:
                return None
        case (ConstraintType.VALUE_EQ, ConstraintType.VALUE_LE):
            if left.parameter <= right.parameter:
                return right
            else:
                return None
        case (ConstraintType.VALUE_EQ, ConstraintType.VALUE_GE):
            if left.parameter >= right.parameter:
                return right
            else:
                return None
        case (ConstraintType.VALUE_LE, ConstraintType.VALUE_LE):
            return Constraint(ConstraintType.VALUE_LE, max(left.parameter, right.parameter))
        case (ConstraintType.VALUE_LE, ConstraintType.VALUE_GE):
            if left.parameter >= right.parameter:
                return CONSTRAINT_TRUE
            else:
                return None
        case (ConstraintType.VALUE_GE, ConstraintType.VALUE_GE):
            return Constraint(ConstraintType.VALUE_GE, min(left.parameter, right.parameter))
        case _:
            return None

def simplify_conjunction(expression: Constraint) -> Constraint:
    conjunction = set(expression.parameter)

    progress = True
    while progress and len(conjunction) > 1:
        progress = False
        for pair in itertools.combinations(conjunction, 2):
            result = simplify_pairwise_and(pair[0], pair[1])
            if result is not None:
                progress = True
                conjunction = conjunction.difference(pair)
                if result.type == ConstraintType.CONSTANT:
                    if result.parameter == False:
                        return Constraint(ConstraintType.CONJUNCTION, frozenset({result}))  # In conjunctions, getting a False anywhere is a shortcut
                    else:
                        continue
                else:
                    conjunction.add(result)
                    break

    return Constraint(ConstraintType.CONJUNCTION, frozenset(conjunction))

def simplify_conj_or_conj(left: Constraint, right: Constraint) -> Constraint|None:
    intersection = set(left.parameter & right.parameter)
    left_remainder = set(left.parameter - right.parameter)
    right_remainder = set(right.parameter - left.parameter)

    progress = True
    while progress and len(left_remainder) > 0 and len(right_remainder) > 0:
        progress = False
        for left_constraint, right_constraint in itertools.product(left_remainder, right_remainder):
            result = simplify_pairwise_or(left_constraint, right_constraint)
            if result is not None:
                progress = True
                left_remainder.remove(left_constraint)
                right_remainder.remove(right_constraint)
                if result.type == ConstraintType.CONSTANT:
                    if result.parameter == False:
                        return Constraint(ConstraintType.CONJUNCTION, frozenset({result}))  # In conjunctions, getting a False anywhere is a shortcut
                    else:
                        continue
                else:
                    intersection.add(result)
                    break

    if len(left_remainder) > 0 and len(right_remainder) > 0:
        return None
    else:
        return Constraint(ConstraintType.CONJUNCTION, frozenset(intersection))

def simplify_disjunction(expression: Constraint) -> Constraint:
    disjunction = set()

    # Simplify the inner conjunctions
    for constraint in expression.parameter:
        conjunction = simplify_conjunction(constraint)
        if len(conjunction.parameter) == 1:
            subexpression = tuple(conjunction.parameter)[0]
            if subexpression.type == ConstraintType.CONSTANT:
                if subexpression.parameter == True:
                    return Constraint(ConstraintType.DISJUNCTION, frozenset({conjunction}))  # Finding a literal True in a disjunction is a shortcut
                else:
                    continue  # Skip Falses
        disjunction.add(conjunction)

    # Then simplify the disjunction
    progress = True
    while progress and len(disjunction) > 1:
        progress = False
        for pair in itertools.combinations(disjunction, 2):
            result = simplify_conj_or_conj(pair[0], pair[1])
            if result is not None:
                progress = True
                disjunction = disjunction.difference(pair)
                if result.type == ConstraintType.CONSTANT:
                    if result.parameter == True:
                        return Constraint(ConstraintType.CONJUNCTION, frozenset({result}))  # In disjunctions, getting a True anywhere is a shortcut
                    else:
                        continue
                else:
                    disjunction.add(result)
                    break
        new_length = len(disjunction)

    if len(disjunction) == 0:
        disjunction.add(CONSTRAINT_FALSE)
    return Constraint(ConstraintType.DISJUNCTION, frozenset(disjunction))

def to_simplified_constraint(constraint: Constraint) -> Constraint:
    dnf = to_dnf(constraint)

    if dnf.type == ConstraintType.CONJUNCTION:
        dnf = Constraint(ConstraintType.DISJUNCTION, frozenset({dnf}))
    elif dnf.type != ConstraintType.DISJUNCTION:
        dnf = Constraint(ConstraintType.DISJUNCTION, frozenset({Constraint(ConstraintType.CONJUNCTION, frozenset({dnf}))}))

    simplified_conjunctions = {simplify_conjunction(conjunction) for conjunction in dnf.parameter}
    result = simplify_disjunction(Constraint(ConstraintType.DISJUNCTION, frozenset(simplified_conjunctions)))
    return result

def load_constraint_expression(description, context: ConstraintContext) -> Constraint:
    constraint = parse_constraint_expression(description, context)
    return to_simplified_constraint(constraint)

def denormalize(constraint: Constraint) -> Constraint:
    if constraint.type == ConstraintType.DISJUNCTION:
        if len(constraint.parameter) == 1:
            conjunction = list(constraint.parameter)[0]
            if conjunction.type == ConstraintType.CONJUNCTION and len(conjunction.parameter) == 1:
                return list(conjunction.parameter)[0]
            else:
                return conjunction

        common = None
        for conjunction in constraint.parameter:
            if conjunction.type != ConstraintType.CONJUNCTION:
                return constraint

            if common is None:
                common = conjunction.parameter
            else:
                common &= conjunction.parameter

        if len(common) == 0:
            return constraint
        else:
            new_disjunction = set()
            for conjunction in constraint.parameter:
                new_disjunction.add(Constraint(ConstraintType.CONJUNCTION, frozenset(conjunction.parameter - common)))
            return Constraint(ConstraintType.CONJUNCTION, frozenset({Constraint(ConstraintType.DISJUNCTION, frozenset(new_disjunction))} | common))
    else:
        return constraint

# Check if a simplified expression is a literal false
def is_false(constraint: Constraint) -> bool:
    if constraint.type == ConstraintType.CONJUNCTION:
        return any(is_false(sub) for sub in constraint.parameter)
    elif constraint.type == ConstraintType.DISJUNCTION:
        return all(is_false(sub) for sub in constraint.parameter)
    else:
        return constraint == CONSTRAINT_FALSE

# Check if a simplified expression is a literal true
def is_true(constraint: Constraint) -> bool:
    if constraint.type == ConstraintType.CONJUNCTION:
        return all(is_true(sub) for sub in constraint.parameter)
    elif constraint.type == ConstraintType.DISJUNCTION:
        return any(is_true(sub) for sub in constraint.parameter)
    else:
        return constraint == CONSTRAINT_TRUE

# Get the acceptable locations for a constraint in DNF
def constraint_locations(constraint: Constraint, all_locations: set[str]) -> set[str]:
    if constraint.type == ConstraintType.DISJUNCTION:
        locations = set()
        for sub in constraint.parameter:
            locations |= constraint_locations(sub, all_locations)
        return locations
    elif constraint.type == ConstraintType.CONJUNCTION:
        locations = all_locations.copy()
        for sub in constraint.parameter:
            locations &= constraint_locations(sub, all_locations)
        return locations
    elif constraint.type == ConstraintType.LOCATION:
        return {constraint.parameter}
    else:
        return all_locations

# Get the acceptable sizes for a constraint in DNF
def constraint_sizes(constraint: Constraint) -> set[int]|None:
    if constraint.type == ConstraintType.DISJUNCTION:
        sizes = set()
        for sub in constraint.parameter:
            sizes |= constraint_sizes(sub)
        return sizes
    elif constraint.type == ConstraintType.CONJUNCTION:
        sizes = None
        for sub in constraint.parameter:
            sub_sizes = constraint_sizes(sub)
            if sub_sizes is not None:
                if sizes is None:
                    sizes = sub_sizes
                else:
                    sizes &= sub_sizes
        return sizes
    elif constraint.type == ConstraintType.SIZE:
        return {constraint.parameter}
    else:
        return None
