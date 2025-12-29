#pragma once

#include <memory>
#include <string>
#include <vector>

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

    struct LValue {
        std::shared_ptr<Declaration> base_declaration;
        CodeLocation location;
        std::vector<std::shared_ptr<Declaration>> indices;  // Either variables for indices, or nullptr as a shortcut for zero

        std::string ir_code() const;
    };
}
