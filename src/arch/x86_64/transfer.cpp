#include <cstdlib>
#include <variant>

#include "config.h"
#include "diagnostic.h"
#include "arch/x86_64/codegen.h"
#include "arch/x86_64/execmodel.h"
#include "gen/execmodel/x86_64/location.h"
#include "gen/execmodel/x86_64/transfer_matcher.h"
#include "gen/execmodel/x86_64/transfer_emission.h"
#include "util/sets.hpp"
#include "util/strings.h"

namespace toycc::arch::x86_64 {
    enum IdentifierType {
        DIRECT,   // Not dereferenced
        POINTER,  // Dereferenced as a pointer
        BLOCK,    // Dereferenced as a block (array, struct, ...)
    };

    static inline IdentifierType to_identifier_type(ir::TypeCategory category) {
        switch (category) {
            case ir::TypeCategory::POINTER:  return IdentifierType::POINTER;
            case ir::TypeCategory::ARRAY:    return IdentifierType::BLOCK;
            case ir::TypeCategory::STRUCT:   return IdentifierType::BLOCK;
            case ir::TypeCategory::UNION:    return IdentifierType::BLOCK;
            default: throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to take the indirect identifier type for a direct type category");
        }
    }

    struct StatementOperandIdentifier {
        bool is_input;
        IdentifierType type;
        size_t index;

        bool operator== (const StatementOperandIdentifier& rhs) const {
            return is_input == rhs.is_input && type == rhs.type && index == rhs.index;
        }
    };

    std::ostream& operator<< (std::ostream& stream, const StatementOperandIdentifier& id) {
        switch (id.type) {
            case IdentifierType::DIRECT:                     break;
            case IdentifierType::POINTER: stream << "ptr ";  break;
            case IdentifierType::BLOCK:   stream << "blk ";  break;
        }

        if (id.is_input)
            stream << "input[" << id.index << "]";
        else
            stream << "output";
        return stream;
    }

    struct TranslationOperandIdentifier {
        struct Statement {
            size_t index;
            StatementOperandIdentifier id;

            bool operator== (const Statement& rhs) const {
                return index == rhs.index && id == rhs.id;
            }
        };

        struct Allocation {
            size_t index;

            bool operator== (const Allocation& rhs) const {
                return index == rhs.index;
            }
        };

        std::variant<Statement, Allocation> id;

        bool operator== (const TranslationOperandIdentifier& rhs) const {
            return id == rhs.id;
        }

        IdentifierType type() const {
            if (std::holds_alternative<Allocation>(id))
                return IdentifierType::DIRECT;
            else
                return std::get<Statement>(id).id.type;
        }
    };

    std::ostream& operator<< (std::ostream& stream, const TranslationOperandIdentifier& id) {
        if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(id.id)) {
            stream << "allocation[" << std::get<TranslationOperandIdentifier::Allocation>(id.id).index << "]";
        } else {
            const TranslationOperandIdentifier::Statement& statement = std::get<TranslationOperandIdentifier::Statement>(id.id);
            stream << "statement[" << statement.index << "]." << statement.id;
        }
        return stream;
    }

    struct AllocatedValue {
        bool is_flush;
        std::shared_ptr<ir::Declaration> variable = nullptr;
        std::vector<TranslationOperandIdentifier> operands;

        bool operator== (const AllocatedValue& rhs) const {
            if (is_flush != rhs.is_flush)
                return false;
            if (variable == nullptr)
                return operands == rhs.operands;
            else
                return variable.get() == rhs.variable.get();
        }
    };

    std::ostream& operator<< (std::ostream& stream, const AllocatedValue& value) {
        if (value.is_flush)
            stream << "flush ";
        if (value.variable.get() != nullptr)
            stream << value.variable->name;

        if (value.operands.size() > 0) {
            stream << "{";
            for (const auto& [index, id] : std::ranges::enumerate_view(value.operands)) {
                if (index > 0)
                    stream << ", ";
                stream << id;
            }
            stream << "}";
        }

        return stream;
    }

    struct SpecificLocation {
        Location location;
        std::optional<AllocatedValue> value;

        SpecificLocation(Location location, const AllocatedValue& value)
                : location(location), value(location == Location::constant || location == Location::stack || location == Location::memory ? value : std::optional<AllocatedValue>{}) {}

        bool operator== (const SpecificLocation& rhs) const {
            return location == rhs.location && value == rhs.value;
        }
    };

    static OperandMatch get_operand_match(const StatementMatch& match, const StatementOperandIdentifier& id) {
        if (id.is_input)
            return match.input[id.index];
        else
            return match.output.value();
    }

    static OperandMatch get_operand_match(const TranslationMatch& match, const TranslationOperandIdentifier& id) {
        if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(id.id)) {
            return match.allocations[std::get<TranslationOperandIdentifier::Allocation>(id.id).index];
        } else {
            const TranslationOperandIdentifier::Statement& statement_id = std::get<TranslationOperandIdentifier::Statement>(id.id);
            return get_operand_match(match.statements[statement_id.index], statement_id.id);
        }
    }

    static void set_operand_match(StatementMatch& match, const StatementOperandIdentifier& id, const OperandMatch& operand_match) {
        if (id.type != IdentifierType::DIRECT)
            return;

        if (id.is_input)
            match.input[id.index] = operand_match.with_index(match.input[id.index].input_index);
        else
            match.output = operand_match.with_index(match.output->input_index);
    }

    static void set_operand_match(TranslationMatch& match, const TranslationOperandIdentifier& id, const OperandMatch& operand_match) {
        if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(id.id)) {
            OperandMatch& destination = match.allocations[std::get<TranslationOperandIdentifier::Allocation>(id.id).index];
            destination = operand_match.with_index(destination.input_index);
        } else {
            const TranslationOperandIdentifier::Statement& statement_id = std::get<TranslationOperandIdentifier::Statement>(id.id);
            set_operand_match(match.statements[statement_id.index], statement_id.id, operand_match);
        }
    }

    static ir::Operand get_operand(const StatementMatch& statement_match, const ir::Statement& statement, const StatementOperandIdentifier& id) {
        const ir::Operand& base_operand = (id.is_input ? statement.inputs[statement_match.input[id.index].input_index.value_or(id.index)] : statement.output.value());
        if (id.type == IdentifierType::DIRECT)
            return base_operand;
        else
            return base_operand.pointer();
    }

    static ir::Operand get_operand(const TranslationMatch& match, const TranslationOperandIdentifier& id) {
        if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(id.id))
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to get the operand associated to an allocation");

        const TranslationOperandIdentifier::Statement& statement_id = std::get<TranslationOperandIdentifier::Statement>(id.id);
        return get_operand(match.statements[statement_id.index], match.group_match.statements[statement_id.index]->statement(), statement_id.id);
    }

    static void set_operand(ir::Statement& statement, const StatementMatch& statement_match, const StatementOperandIdentifier& id, ir::Operand operand) {
        ir::Operand& target = (id.is_input ? statement.inputs[statement_match.input[id.index].input_index.value_or(id.index)] : statement.output.value());
        if (id.type == IdentifierType::DIRECT)
            target = operand;
        else
            target.value = operand.value;
    }

    static void set_operand(TranslationMatch& match, const TranslationOperandIdentifier& id, const ir::Operand& operand) {
        if (std::holds_alternative<TranslationOperandIdentifier::Statement>(id.id)) {
            const TranslationOperandIdentifier::Statement& statement_id = std::get<TranslationOperandIdentifier::Statement>(id.id);
            set_operand(match.group_match.statements[statement_id.index]->statement(), match.statements[statement_id.index], statement_id.id, operand);
        }
    }

    static bool is_output_operand(const TranslationOperandIdentifier& id) {
        if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(id.id))
            return false;
        const TranslationOperandIdentifier::Statement& statement_id = std::get<TranslationOperandIdentifier::Statement>(id.id);
        return !statement_id.id.is_input && statement_id.id.type == IdentifierType::DIRECT;
    }

    static bool has_output(const AllocatedValue& value) {
        for (const TranslationOperandIdentifier& id : value.operands)
            if (is_output_operand(id))
                return true;
        return false;
    }

    static bool is_pointer(const TranslationOperandIdentifier& id) {
        if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(id.id))
            return false;
        return std::get<TranslationOperandIdentifier::Statement>(id.id).id.type == IdentifierType::POINTER;
    }

    static bool has_pointer(const AllocatedValue& value) {
        for (const TranslationOperandIdentifier& id : value.operands)
            if (is_pointer(id))
                return true;
        return false;
    }
}

namespace std {
    using toycc::arch::x86_64::StatementOperandIdentifier;
    using toycc::arch::x86_64::TranslationOperandIdentifier;
    using toycc::arch::x86_64::AllocatedValue;
    using toycc::arch::x86_64::SpecificLocation;

    template<> struct hash<StatementOperandIdentifier> {
        size_t operator() (const StatementOperandIdentifier& key) const {
            return (key.index << 3) | (key.type << 1) | key.is_input;
        }
    };

    template<> struct hash<TranslationOperandIdentifier> {
        size_t operator() (const TranslationOperandIdentifier& key) const {
            if (std::holds_alternative<TranslationOperandIdentifier::Allocation>(key.id)) {
                return std::get<TranslationOperandIdentifier::Allocation>(key.id).index << 1;
            } else {
                const TranslationOperandIdentifier::Statement& id = std::get<TranslationOperandIdentifier::Statement>(key.id);
                return (((hash<StatementOperandIdentifier>{}(id.id) << 10) | id.index) << 1) | 1;
            }
        }
    };
}

namespace toycc::arch::x86_64 {
    enum class LocationType : size_t {
        CONSTANT = 0, MEMORY = 1, MAIN_REGISTER = 2, EXT_REGISTER = 3, MM_REGISTER = 4,
    };

    static const std::unordered_map<LocationType, std::unordered_set<Location>> LOCATION_SETS = {
        {LocationType::CONSTANT, {Location::constant}},
        {LocationType::MEMORY, {Location::stack, Location::memory}},
        {LocationType::MAIN_REGISTER, {Location::a, Location::b, Location::c, Location::d, Location::si, Location::di, Location::sp, Location::bp}},
        {LocationType::EXT_REGISTER,  {Location::r8, Location::r9, Location::r10, Location::r11, Location::r12, Location::r13, Location::r14, Location::r15}},
        {LocationType::MM_REGISTER,   {Location::mm0, Location::mm1, Location::mm2,  Location::mm3,  Location::mm4,  Location::mm5,  Location::mm6,  Location::mm7,
                                       Location::mm8, Location::mm9, Location::mm10, Location::mm11, Location::mm12, Location::mm13, Location::mm14, Location::mm15}},
    };

    static const std::unordered_map<Location, LocationType> LOCATION_TYPES = {
        {Location::constant, LocationType::CONSTANT},
        {Location::a,        LocationType::MAIN_REGISTER},
        {Location::b,        LocationType::MAIN_REGISTER},
        {Location::c,        LocationType::MAIN_REGISTER},
        {Location::d,        LocationType::MAIN_REGISTER},
        {Location::si,       LocationType::MAIN_REGISTER},
        {Location::di,       LocationType::MAIN_REGISTER},
        {Location::sp,       LocationType::MAIN_REGISTER},
        {Location::bp,       LocationType::MAIN_REGISTER},
        {Location::r8,       LocationType::EXT_REGISTER},
        {Location::r9,       LocationType::EXT_REGISTER},
        {Location::r10,      LocationType::EXT_REGISTER},
        {Location::r11,      LocationType::EXT_REGISTER},
        {Location::r12,      LocationType::EXT_REGISTER},
        {Location::r13,      LocationType::EXT_REGISTER},
        {Location::r14,      LocationType::EXT_REGISTER},
        {Location::r15,      LocationType::EXT_REGISTER},
        {Location::mm0,      LocationType::MM_REGISTER},
        {Location::mm1,      LocationType::MM_REGISTER},
        {Location::mm2,      LocationType::MM_REGISTER},
        {Location::mm3,      LocationType::MM_REGISTER},
        {Location::mm4,      LocationType::MM_REGISTER},
        {Location::mm5,      LocationType::MM_REGISTER},
        {Location::mm6,      LocationType::MM_REGISTER},
        {Location::mm7,      LocationType::MM_REGISTER},
        {Location::mm8,      LocationType::MM_REGISTER},
        {Location::mm9,      LocationType::MM_REGISTER},
        {Location::mm10,     LocationType::MM_REGISTER},
        {Location::mm11,     LocationType::MM_REGISTER},
        {Location::mm12,     LocationType::MM_REGISTER},
        {Location::mm13,     LocationType::MM_REGISTER},
        {Location::mm14,     LocationType::MM_REGISTER},
        {Location::mm15,     LocationType::MM_REGISTER},
        {Location::stack,    LocationType::MEMORY},
        {Location::memory,   LocationType::MEMORY},
    };

    static const std::unordered_set<Location> POINTER_LOCATIONS = {Location::a,  Location::b,  Location::c,   Location::d,   Location::si,  Location::di,
                                                                   Location::r8, Location::r9, Location::r10, Location::r11, Location::r12, Location::r13, Location::r14, Location::r15};

    static const arma::fmat TRANSFER_COSTS = {
        /* from / to           CONSTANT |   MEMORY | MAIN_REGISTER | EXT_REGISTER | MM_REGISTER */
        /* CONSTANT      */ {         0,       100,             10,            11,     INFINITY},
        /* MEMORY        */ {  INFINITY,  INFINITY,            100,           101,     INFINITY},
        /* MAIN_REGISTER */ {  INFINITY,       100,             10,            11,     INFINITY},
        /* EXT_REGISTER  */ {  INFINITY,       101,             11,            11,     INFINITY},
        /* MM_REGISTER   */ {  INFINITY,  INFINITY,       INFINITY,      INFINITY,     INFINITY},
    };

    static const std::unordered_map<LocationType, float> OUTPUT_COSTS = {
        {LocationType::CONSTANT,    INFINITY},
        {LocationType::MEMORY,           100},
        {LocationType::MAIN_REGISTER,      0},
        {LocationType::EXT_REGISTER,       1},
        {LocationType::MM_REGISTER, INFINITY},
    };

    struct WeightsMatrix {
        std::vector<std::pair<AllocatedValue, size_t>> value_rows;
        std::vector<std::pair<SpecificLocation, size_t>> location_cols;
        arma::fmat weights;

        WeightsMatrix() : weights(1, 1) {
            weights.fill(INFINITY);  // Dummy row and column 0 for the Hungarian algorithm
        }

        bool contains(const AllocatedValue& value) {
            for (auto& [existing_value, row] : value_rows)
                if (value == existing_value)
                    return true;
            return false;
        }

        bool contains(const SpecificLocation& value) {
            for (auto& [existing_location, col] : location_cols)
                if (value == existing_location)
                    return true;
            return false;
        }

        size_t add(const AllocatedValue& value) {
            for (auto& [existing_value, row] : value_rows) {
                if (value == existing_value) {
                    for (const TranslationOperandIdentifier& id : value.operands)
                        if (!std::ranges::contains(existing_value.operands, id))
                            existing_value.operands.push_back(id);
                    return row;
                }
            }

            const size_t row = weights.n_rows;
            value_rows.emplace_back(value, row);
            weights.insert_rows(row, 1);
            weights.row(row).fill(INFINITY);
            return row;
        }

        std::optional<size_t> get_row(const AllocatedValue& value) {
            for (auto& [existing_value, row] : value_rows)
                if (value == existing_value)
                    return row;
            return {};
        }

        size_t add(const SpecificLocation& location) {
            for (auto& [existing_location, col] : location_cols)
                if (location == existing_location)
                    return col;

            const size_t col = weights.n_cols;
            location_cols.emplace_back(location, col);
            weights.insert_cols(col, 1);
            weights.col(col).fill(INFINITY);
            return col;
        }

        std::optional<size_t> get_col(const SpecificLocation& location) {
            for (auto& [existing_location, col] : location_cols)
                if (location == existing_location)
                    return col;
            return {};
        }
    };

    std::ostream& operator<< (std::ostream& stream, const WeightsMatrix& weights) {
        size_t value_width = 0;
        for (const auto& [value, row] : weights.value_rows)
            value_width = std::max(value_width, dump(value).size());
        value_width += 1;

        std::vector<std::string> column_titles(weights.weights.n_cols);
        column_titles[0] = "null";
        for (const auto& [location, col] : weights.location_cols)
            column_titles[col] = dump(location.location);


        std::vector<size_t> column_widths;
        stream << justify_right("", value_width);
        for (const std::string& title : column_titles) {
            const size_t column_width = std::max(size_t(10), title.size()) + 1;
            column_widths.push_back(column_width);
            stream << center(title, column_width);
        }
        stream << "\n";

        for (size_t row = 0; row < weights.weights.n_rows; row++) {
            if (row == 0) {
                stream << justify_right("null", value_width);
            } else {
                for (const auto& [value, value_row] : weights.value_rows) {
                    if (value_row == row) {
                        stream << justify_right(dump(value), value_width);
                        break;
                    }
                }
            }

            for (const auto& [col, width] : std::ranges::enumerate_view(column_widths))
                stream << center(std::to_string(weights.weights(row, col)), width);
            stream << "\n";
        }

        return stream;
    }

    static void find_indirects(std::unordered_set<std::shared_ptr<ir::Declaration>>& indirects, std::unordered_set<std::shared_ptr<ir::Declaration>>& reads,
                               const ir::DependencyGraph& graph, const TranslationMatch& match)
    {
        for (std::shared_ptr<ir::DependencyNode> statement : match.group_match.statements) {
            for (ir::DependencyGraph::Edge edge : graph.connected_edges(statement)) {
                std::shared_ptr<ir::DependencyNode> value = (edge.entry == statement ? edge.exit : edge.entry);
                std::shared_ptr<ir::Declaration> variable = value->declaration();

                if (edge.attr.operand_group == ir::OperandGroup::INDIRECT && (edge.attr.type & (ir::DependencyType::DEREFERENCE | ir::DependencyType::CALL | ir::DependencyType::LIVE_ON_EXIT)))
                    indirects.insert(variable);

                if (edge.attr.type & ir::DependencyType::READ)
                    reads.insert(variable);
            }
        }
    }

    static void set_operand_weights(WeightsMatrix& weights, const StackFrame& frame, const TranslationMatch& match) {
        for (size_t statement_index = 0; statement_index < match.statements.size(); statement_index++) {
            const StatementMatch& statement_match = match.statements[statement_index];
            const ir::Statement& statement = match.group_match.statements[statement_index]->statement();

            for (size_t input_index = 0; input_index < statement_match.input.size(); input_index++) {
                const size_t input_operand_index = statement_match.input[input_index].input_index.value_or(input_index);
                const ir::Operand& operand = statement.inputs[input_operand_index];
                AllocatedValue value = {.is_flush = false, .variable = (operand.is_variable() ? operand.declaration() : nullptr),
                                        .operands = {{.id = TranslationOperandIdentifier::Statement {.index = statement_index,
                                                      .id = {.is_input = true, .type = IdentifierType::DIRECT, .index = input_index}}}}};
                weights.add(value);

                if (operand.is_dereference()) {
                    ir::Operand pointer = operand.pointer();
                    AllocatedValue pointer_value = {.is_flush = false, .variable = (pointer.is_variable() ? pointer.declaration() : nullptr),
                                                    .operands = {{.id = TranslationOperandIdentifier::Statement {.index = statement_index,
                                                                  .id = {.is_input = true, .type = to_identifier_type(pointer.base_type()->category), .index = input_index}}}}};
                    weights.add(pointer_value);
                }
            }

            // If the output doesn't overwrite an input, it should also get allocated
            if (statement_match.output.has_value() && !statement_match.is_inout) {
                const ir::Operand& operand = statement.output.value();
                AllocatedValue value = {.is_flush = false, .variable = (operand.is_variable() ? operand.declaration() : nullptr),
                                        .operands = {{.id = TranslationOperandIdentifier::Statement {.index = statement_index,
                                                      .id = {.is_input = false, .type = IdentifierType::DIRECT, .index = {}}}}}};
                weights.add(value);

                if (operand.is_dereference()) {
                    ir::Operand pointer = operand.pointer();
                    AllocatedValue pointer_value = {.is_flush = false, .variable = (pointer.is_variable() ? pointer.declaration() : nullptr),
                                                    .operands = {{.id = TranslationOperandIdentifier::Statement {.index = statement_index,
                                                                                                                 .id = {.is_input = false, .type = to_identifier_type(pointer.base_type()->category), .index = {}}}}}};
                    weights.add(pointer_value);
                }
            }
        }

        for (const auto& [value, row] : weights.value_rows) {
            std::unordered_set<Location> allowed_locations = toycc::execmodel::x86_64::ALL_LOCATIONS;
            for (const TranslationOperandIdentifier& id : value.operands) {
                switch (id.type()) {
                    case IdentifierType::DIRECT:   allowed_locations = unordered_set_intersection(allowed_locations, get_operand_match(match, id).locations);  break;
                    case IdentifierType::POINTER:  allowed_locations = unordered_set_intersection(allowed_locations, POINTER_LOCATIONS);                       break;
                    case IdentifierType::BLOCK:    allowed_locations = unordered_set_intersection(allowed_locations, LOCATION_SETS.at(LocationType::MEMORY));  break;
                }
            }

            if (allowed_locations.empty())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Value {} has no allowed location", dump(value)));

            bool has_location = false;
            if (has_output(value)) {
                for (Location allowed_location : allowed_locations) {
                    SpecificLocation allowed_specific_location(allowed_location, value);
                    LocationType allowed_location_type = LOCATION_TYPES.at(allowed_location);
                    const float weight = OUTPUT_COSTS.at(allowed_location_type);
                    if (std::isfinite(weight)) {
                        size_t location_col = weights.add(allowed_specific_location);
                        weights.weights(row, location_col) = weight;
                        has_location = true;
                    }
                }
            } else {
                std::unordered_set<Location> current_locations = toycc::execmodel::x86_64::ALL_LOCATIONS;
                for (const TranslationOperandIdentifier& id : value.operands)
                    current_locations = unordered_set_intersection(current_locations, frame.locate(get_operand(match, id)));

                if (current_locations.empty())
                    throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Input value {} has no location", dump(value)));

                for (Location allowed_location : allowed_locations) {
                    SpecificLocation allowed_specific_location(allowed_location, value);
                    float min_weight = INFINITY;
                    if (current_locations.contains(allowed_location)) {
                        min_weight = 0;
                    } else {
                        LocationType allowed_location_type = LOCATION_TYPES.at(allowed_location);
                        for (Location current_location : current_locations) {
                            LocationType current_location_type = LOCATION_TYPES.at(current_location);
                            min_weight = std::min(TRANSFER_COSTS(std::to_underlying(current_location_type), std::to_underlying(allowed_location_type)), min_weight);
                        }
                    }

                    if (std::isfinite(min_weight)) {
                        size_t location_col = weights.add(allowed_specific_location);
                        weights.weights(row, location_col) = min_weight;
                        has_location = true;
                    }
                }
            }

            if (!has_location)
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Value {} has no allowed destination", dump(value)));
        }
    }

    static void set_allocation_weights(WeightsMatrix& weights, const TranslationMatch& match) {
        for (size_t allocation_index = 0; allocation_index < match.allocations.size(); allocation_index++) {
            const OperandMatch& allocation_match = match.allocations[allocation_index];
            AllocatedValue value = {.is_flush = false, .variable = nullptr, .operands = {{.id = TranslationOperandIdentifier::Allocation {.index = allocation_index}}}};

            if (allocation_match.locations.empty())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Allocation {} has no allowed location", allocation_index));

            const size_t value_row = weights.add(value);
            for (Location allowed_location : allocation_match.locations) {
                SpecificLocation allowed_specific_location(allowed_location, value);
                size_t location_col = weights.add(allowed_specific_location);
                weights.weights(value_row, location_col) = 0;
            }
        }
    }

    // Before a dereference or call, flush all indirect operands to their respective memory locations
    static void set_indirect_flushes(WeightsMatrix& weights, const StackFrame& frame, const std::unordered_set<std::shared_ptr<ir::Declaration>>& indirects) {
        for (std::shared_ptr<ir::Declaration> variable : indirects) {
            const std::unordered_set<Location> locations = frame.locate(variable);

            Location destination = Location::stack;
            if (locations.contains(Location::memory) && !locations.contains(Location::stack))
                destination = Location::memory;

            if (locations.empty())
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Indirect dependency variable {} has no location", variable->name));

            if (locations.contains(destination))
                continue;

            // Move the variable to memory if necessary
            if (variable->storage & ir::StorageClass::GLOBAL)
                throw Diagnostic(DiagnosticLevel::NOT_IMPLEMENTED, "Flushing indirect global variables is not implemented", variable->location);

            AllocatedValue value = {.is_flush = true, .variable = variable, .operands = {}};
            SpecificLocation specific_location(destination, value);
            size_t value_row = weights.add(value);
            size_t location_col = weights.add(specific_location);

            LocationType destination_location_type = LOCATION_TYPES.at(destination);
            float min_weight = INFINITY;
            for (Location current_location : locations) {
                LocationType current_location_type = LOCATION_TYPES.at(current_location);
                min_weight = std::min(TRANSFER_COSTS(std::to_underlying(current_location_type), std::to_underlying(destination_location_type)), min_weight);
            }
            weights.weights(value_row, location_col) = min_weight;
        }
    }

    static void set_variable_weights(WeightsMatrix& weights, const StackFrame& frame) {
        for (std::shared_ptr<ir::Declaration> variable : frame.allocated_variables()) {
            AllocatedValue value = {.is_flush = false, .variable = variable, .operands = {}};
            const std::unordered_set<Location> current_locations = frame.locate(variable);

            const std::optional<size_t> value_row = weights.get_row(value);
            if (value_row.has_value()) {
                for (Location current_location : current_locations) {
                    SpecificLocation specific_location(current_location, value);

                    // If it is an operand, don't add a forbidden location, just set the allowed ones to zero
                    if (!weights.contains(specific_location))
                        continue;

                    const size_t location_col = weights.add(specific_location);
                    if (std::isfinite(weights.weights(*value_row, location_col)))
                        weights.weights(*value_row, location_col) = 0;
                }
            } else {
                // Not an operand -> set existing locations
                bool is_on_stack = false;
                const size_t value_row = weights.add(value);
                for (Location current_location : current_locations) {
                    if (current_location == Location::stack)
                        is_on_stack = true;

                    SpecificLocation specific_location(current_location, value);
                    size_t location_col = weights.add(specific_location);
                    weights.weights(value_row, location_col) = 0;  // It's already there, so no transfer cost
                }

                // Add a possible spill to the stack of non-operand values
                if (!is_on_stack) {
                    SpecificLocation stack_location(Location::stack, value);
                    size_t location_col = weights.add(stack_location);

                    float min_weight = INFINITY;
                    for (Location current_location : current_locations) {
                        LocationType current_location_type = LOCATION_TYPES.at(current_location);
                        min_weight = std::min(TRANSFER_COSTS(std::to_underlying(current_location_type), std::to_underlying(LocationType::MEMORY)), min_weight);
                    }
                    weights.weights(value_row, location_col) = min_weight;
                }
            }
        }
    }

    static WeightsMatrix build_weights_matrix(const StackFrame& frame, const TranslationMatch& match, const std::unordered_set<std::shared_ptr<ir::Declaration>>& indirects) {
        WeightsMatrix weights;
        set_operand_weights(weights, frame, match);
        set_allocation_weights(weights, match);
        set_indirect_flushes(weights, frame, indirects);
        set_variable_weights(weights, frame);
        return weights;
    }

    static std::vector<std::pair<AllocatedValue, SpecificLocation>> find_matching(const WeightsMatrix& weights) {
        std::vector<float> value_potential(weights.weights.n_rows);
        std::vector<float> location_potential(weights.weights.n_cols);
        std::vector<size_t> matching(weights.weights.n_cols);
        std::vector<size_t> way(weights.weights.n_cols);

        for (size_t row = 1; row < weights.weights.n_rows; row++) {
            matching[0] = row;
            size_t j0 = 0;
            std::vector<float> minv(weights.weights.n_cols, INFINITY);
            std::vector<bool> used(weights.weights.n_cols, false);

            do {
                used[j0] = true;
                size_t i0 = matching[j0], j1 = 0;
                float delta = INFINITY;
                for (size_t j = 1; j < weights.weights.n_cols; j++) {
                    if (!used[j]) {
                        float cur = weights.weights(i0, j) - value_potential[i0] - location_potential[j];
                        if (cur < minv[j]) {
                            minv[j] = cur;
                            way[j] = j0;
                        }

                        if (minv[j] < delta) {
                            delta = minv[j];
                            j1 = j;
                        }
                    }
                }

                for (size_t j = 0; j < weights.weights.n_cols; j++) {
                    if (used[j]) {
                        value_potential[matching[j]] += delta;
                        location_potential[j] -= delta;
                    } else {
                        minv[j] -= delta;
                    }
                }

                j0 = j1;
            } while (matching[j0] != 0);

            do {
                size_t j1 = way[j0];
                matching[j0] = matching[j1];
                j0 = j1;
            } while (j0);
        }

        std::vector<std::pair<AllocatedValue, SpecificLocation>> allocation;
        for (const auto [location_col, value_row] : std::ranges::enumerate_view(matching)) {
            if (location_col == 0 || value_row == 0)
                continue;

            std::optional<AllocatedValue> allocated_value;
            std::optional<SpecificLocation> allocated_location;

            for (const auto& [value, row] : weights.value_rows) {
                if (row == value_row) {
                    allocated_value = value;
                    break;
                }
            }

            for (const auto& [location, col] : weights.location_cols) {
                if (col == static_cast<size_t>(location_col)) {
                    allocated_location = location;
                    break;
                }
            }

            if (allocated_value.has_value() && allocated_location.has_value())
                allocation.emplace_back(*allocated_value, *allocated_location);
            else
                throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Allocated a non-existing value");
        }

        return allocation;
    }

    // Sort allocations in order of transfer emission :
    // - Pointers and flushes : pointers are needed to transfer their dereferences, and flushed variables may be referenced by dereferences
    // - Dereferences
    // - Then ormal transfers
    static void sort_transfers(std::vector<std::pair<AllocatedValue, SpecificLocation>>& allocation_map) {
        auto operand_comparator = [](const TranslationOperandIdentifier& left, const TranslationOperandIdentifier& right) {
            if (is_pointer(left))
                return true;
            if (is_pointer(right))
                return false;
            return false;
        };

        for (auto& [value, location] : allocation_map)
            std::ranges::sort(value.operands, operand_comparator);

        auto value_comparator = [](const std::pair<AllocatedValue, SpecificLocation>& left_pair, const std::pair<AllocatedValue, SpecificLocation>& right_pair) {
            const AllocatedValue& left  = left_pair.first;
            const AllocatedValue& right = right_pair.first;

            // True is left first, false is right first
            if (has_pointer(left) || left.is_flush)
                return true;
            if (has_pointer(right) || right.is_flush)
                return false;
            if (left.is_flush)
                return true;
            if (right.is_flush)
                return false;
            return false;
        };

        std::ranges::sort(allocation_map, value_comparator);
    }

    void CodeGenerator::emit_transfers(StackFrame& frame, const ir::DependencyGraph& graph, TranslationMatch& match) {
        std::unordered_set<std::shared_ptr<ir::Declaration>> indirects;
        std::unordered_set<std::shared_ptr<ir::Declaration>> reads;
        find_indirects(indirects, reads, graph, match);

        WeightsMatrix weights = build_weights_matrix(frame, match, indirects);
        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Location weights matrix :\n";
            std::cerr << indent(dump(weights), true, "            ");
        }

        std::vector<std::pair<AllocatedValue, SpecificLocation>> allocation_map = find_matching(weights);
        sort_transfers(allocation_map);

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Location matching :\n";
            for (const auto& [value, specific_location] : allocation_map)
                std::cerr << "            " << value << " -> " << specific_location.location << "\n";
        }

        for (const auto& [value, specific_location] : allocation_map) {
            if (value.variable.get() != nullptr) {
                bool is_only_output = false;
                for (const TranslationOperandIdentifier& id : value.operands) {
                    set_operand_match(match, id, {OperandMatch::OK, {specific_location.location}});
                    is_only_output = is_only_output || is_output_operand(id);
                }

                if (!is_only_output)
                    transfer(frame, value.variable, specific_location.location);
            } else {
                for (const TranslationOperandIdentifier& id : value.operands) {
                    set_operand_match(match, id, {OperandMatch::OK, {specific_location.location}});
                    if (std::holds_alternative<TranslationOperandIdentifier::Statement>(id.id) && !is_output_operand(id)) {
                        ir::Operand operand = get_operand(match, id);
                        transfer(frame, operand, specific_location.location);
                        set_operand(match, id, operand);
                    }
                }
            }
        }

        // For indirect flushes that are not required as inputs, remove possible other
        // Do this in a separate loop to avoid ordering problems : a variable may have both an allocation a flush
        // For instance, (x: di -> di) and (flush x : di -> stack)
        // If the flush goes through first, the sequence should be (di --flush--> di,stack --alloc--> di,stack --move--> stack)
        // not (di --flush--> di,stack --move--> stack --alloc--> di,stack) which would trigger an unnecessary transfer and leave `di` valid even though it's not because of the flush
        for (const auto& [value, specific_location] : allocation_map) {
            if (value.is_flush && indirects.contains(value.variable) && !reads.contains(value.variable))
                frame.move(value.variable, specific_location.location);
        }

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Allocated translation match :\n";
            std::cerr << indent(dump(match), true, "            ") << "\n";
        }

        if (!match.matches())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "There are still non-matching operands in the translation match after transfers");
    }

    void CodeGenerator::transfer(StackFrame& frame, std::shared_ptr<ir::Declaration> variable, Location destination) {
        ir::Operand operand(variable, variable->location);
        transfer(frame, operand, destination);
    }

    void CodeGenerator::transfer(StackFrame& frame, ir::Operand& operand, Location destination) {
        const std::unordered_set<Location> current_locations = frame.locate(operand);
        if (current_locations.empty())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("Attempted a transfer on operand {} that has no location", operand.ir_code()), operand.location);

        if (current_locations.contains(destination))
            return;  // The operand is already in the right location -> skip

        std::optional<TransferMatch> match = toycc::execmodel::x86_64::match_transfers(frame, operand, destination);
        if (!match.has_value())
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, std::format("No transfer match for operand {}", operand.ir_code()), operand.location);

        Location source = *match->source_locations.begin();
        ir::Operand source_operand = operand;
        if (operand.is_constant() || operand.is_dereference()) {
            std::shared_ptr<ir::Declaration> temporary = frame.declare_intermediate(operand.type(), operand.location);
            frame.copy(temporary, destination);
            operand = temporary;
        } else if (operand.is_label()) {
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Transferring labels is not supported", operand.location);
        } else if (operand.is_variable()) {
            frame.copy(operand.declaration(), destination);
        } else throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Unknown operand type", operand.location);

        if (toycc::config::debug::with_comment_trace) {
            std::stringstream comment;
            comment << "TRANSFER " << source_operand.ir_code() << "(" << source << ") -> " << operand.ir_code() << "(" << destination << ")";
            frame.comment(comment.str());
        }

        if (toycc::config::debug::with_translation_trace) {
            std::cerr << "        Transfer " << source_operand.ir_code() << "(" << source << ") -> " << operand.ir_code() << "(" << destination << ")" << "\n";
            std::cerr << indent(dump(match.value()), true, "            ") << "\n";
        }


        toycc::execmodel::x86_64::emit_transfer(frame, source_operand, operand, match.value(), source, destination);
    }
}
