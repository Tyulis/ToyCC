#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>

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
        ADDRESSED    = 0x100,  // Something requires the memory address of this variable
    };

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

        Member to_storage_type() const;
        std::string ir_code() const;
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

    struct Constant {
        std::variant<IntegerConstant, FloatingPointConstant, std::string> value;
        CodeLocation location;
        std::shared_ptr<Type> type;

        Constant as(std::shared_ptr<Type> new_type) const;
        bool operator== (const Constant& rhs) const;

        std::string ir_code() const;
    };

    struct RValue {
        std::variant<std::shared_ptr<Declaration>, Constant> value;

        RValue(std::shared_ptr<Declaration> declaration);
        RValue(Constant value);

        bool is_constant() const;
        bool operator== (const RValue& rhs) const;

        CodeLocation location() const;
        std::shared_ptr<Type> type() const;
        Constant constant() const;
        std::shared_ptr<Declaration> declaration() const;

        std::string ir_code() const;
    };

    struct LValue {
        RValue base;
        CodeLocation location;
        std::vector<RValue> indices;

        LValue(std::shared_ptr<Declaration> declaration);
        LValue(RValue base_declaration, CodeLocation location, std::vector<RValue> indices = {});

        std::shared_ptr<Type> type() const;
        std::string ir_code() const;
    };
}
