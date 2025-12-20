#pragma once

#include <string>

#include "ir/type.h"

namespace toycc::ir {
    struct Identifier {
        QualifiedType type;
        std::string name;
    };

    enum class StorageClass : int {
        AUTO, STATIC, EXTERN,
        // THREAD_LOCAL, REGISTER  // Unsupported
    };

    struct Declaration : public Identifier {
        virtual ~Declaration() = default;

        size_t alignment;
        StorageClass storage_class;
    };
}
