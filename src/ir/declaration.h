#pragma once

#include <memory>
#include <string>
#include <variant>
#include <unordered_set>

// For some reason ANTLR silently undefines the standard EOF macro, which boost/multiprecision needs
// Redefining it as constexpr seems to be satisfactory
#ifdef EOF
#    undef EOF
#endif
constexpr char EOF = -1;

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

#include "code_location.h"
#include "ir/type.h"
#include "util/flags.hpp"

namespace toycc::ir {
    enum class StorageClass {
        AUTO         = 0x001,
        STATIC       = 0x002,
        EXTERN       = 0x004,
        REGISTER     = 0x008,
        THREAD_LOCAL = 0x010,
        TYPEDEF      = 0x020,

        PARAMETER    = 0x040,  // Function parameter
        TEMPORARY    = 0x080,  // Temporary variable internal to the IR
        INTERMEDIATE = 0x100,  // Temporary variable internal to a basic block. FIXME : Should be assigned and used exactly once
        GLOBAL       = 0x200,  // Global variable
    };

    constexpr Flags<StorageClass> INTERNAL_STORAGE = StorageClass::TEMPORARY | StorageClass::INTERMEDIATE;

    enum class FunctionSpecifier {
        INLINE      = 0x01,
        NORETURN    = 0x02,
        STDCALL     = 0x04,
    };

    struct Member {
        std::string name;
        std::shared_ptr<Type> type;
        CodeLocation location;

        Member() = default;
        Member(std::string name, std::shared_ptr<Type> type, CodeLocation location);

        bool is_anonymous() const;
        std::string ir_code(std::unordered_set<const Type*> parents = {}) const;
    };

    struct Declaration : public Member {
        Flags<StorageClass> storage;
        Flags<FunctionSpecifier> function_spec;

        Declaration() = default;
        Declaration(Member member, Flags<StorageClass> storage = {}, Flags<FunctionSpecifier> function_spec = {});
        Declaration(std::string name, std::shared_ptr<Type> type, CodeLocation location, Flags<StorageClass> storage = {}, Flags<FunctionSpecifier> function_spec = {});

        void check() const;
        std::string ir_code() const;
    };

    // For constants, use types that can hold any value of any C type, signed or unsigned, to avoid fiddling with native types of various sizes all the time
    using IntegerConstant = boost::multiprecision::int128_t;
    using FloatingPointConstant = boost::multiprecision::cpp_bin_float_quad;

    struct PointerConstant {
        std::string label;
        ssize_t offset;

        bool operator== (const PointerConstant& rhs) const;
    };

    struct Constant;
    struct UnionConstant {
        size_t index;                     // Index of the member to initialize
        std::shared_ptr<Constant> value;  // Its value

        UnionConstant(size_t index, const Constant& value);
        bool operator== (const UnionConstant& rhs) const;
    };

    struct RepeatMarker {
        // Optimization : in aggregate constants, ending with this repeats the last member value
        // This avoids allocating millions of the same object in array initializers
        inline bool operator== (const RepeatMarker&) const {
            return true;
        }
    };

    std::ostream& operator<< (std::ostream& stream, const PointerConstant& pointer);

    struct Constant {
        enum Tag {INTEGER, FLOAT, POINTER, UNION, STRING, REFERENCE, AGGREGATE, REPEAT};

        std::variant<IntegerConstant, FloatingPointConstant, PointerConstant, UnionConstant, std::string, std::shared_ptr<Declaration>, std::vector<Constant>, RepeatMarker> value;
        CodeLocation location;
        std::shared_ptr<Type> type;

        static Constant make_repeat(CodeLocation location = BUILTIN_LOCATION);

        Tag tag() const;

        IntegerConstant integer() const;
        FloatingPointConstant floating_point() const;
        PointerConstant pointer() const;
        UnionConstant unionval() const;
        std::string string() const;
        std::shared_ptr<Declaration> reference() const;
        std::vector<Constant>& aggregate();
        const std::vector<Constant>& aggregate() const;

        Constant as(std::shared_ptr<Type> new_type) const;
        bool operator== (const Constant& rhs) const;

        std::string ir_code() const;
    };

    // Statement operand : any value or dereference. Basically any lvalue or rvalue
    // Lvalues and rvalues are a semantic analysis concept, once a statement is semantically correct drop that information to uniformize everything
    struct Operand {
        enum BaseTag {VARIABLE_BASE, CONSTANT_BASE};
        enum Tag {VARIABLE, CONSTANT, DEREFERENCE};

        //           variable                      constant
        std::variant<std::shared_ptr<Declaration>, Constant> value;
        CodeLocation location;
        std::vector<Operand> indices;

        Operand(const Constant& constant, std::vector<Operand> indices = {});
        Operand(const Constant& constant, CodeLocation location, std::vector<Operand> indices = {});
        Operand(std::shared_ptr<Declaration> declaration, std::vector<Operand> indices = {});
        Operand(std::shared_ptr<Declaration> declaration, CodeLocation location, std::vector<Operand> indices = {});
        Operand(std::variant<std::shared_ptr<Declaration>, Constant> value, CodeLocation location, std::vector<Operand> indices = {});

        BaseTag base_tag() const;
        Tag tag() const;

        std::shared_ptr<Type> base_type() const;
        std::shared_ptr<Type> type() const;

        Constant constant() const;
        Constant& constant();
        std::shared_ptr<Declaration> declaration() const;
        Operand pointer() const;
        std::optional<size_t> as_index() const;

        bool operator== (const Operand& operand) const;

        std::string ir_code() const;
    };
}
