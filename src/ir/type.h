#pragma once

#include <memory>
#include <string>
#include <optional>
#include <unordered_set>

#include "code_location.h"

namespace toycc::ir {
    enum class TypeTag {
        DIRECT, STRUCT, UNION, ENUM, TYPEDEF,
    };

    enum class TypeCategory {
        /* Valid storage_category() */ BOOL, INTEGER, FLOAT, ARRAY, STRUCT, UNION, FUNCTION,
        /* Others                   */ VOID, BUILTIN, LABEL, POINTER, ENUM, BITFIELD, QUALIFIED, ALIGNED,
    };

    struct TypeIdentifier {
        TypeTag tag;
        std::string name;

        std::string text() const;
        bool operator== (const TypeIdentifier& rhs) const = default;
    };

    struct Type {
        TypeCategory category;
        CodeLocation location;

        Type(TypeCategory category, CodeLocation location);

        virtual bool complete() const;  // Defaults to true
        virtual bool is_const() const;  // Defaults to false
        virtual size_t size(CodeLocation location) const;       // In bytes
        virtual size_t alignment(CodeLocation location) const;  // In bytes
        virtual bool operator== (const Type& rhs) const;  // Defaults to same category

        virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const;  // Type emitted by a dereference of this type. Defaults to a throw.
        virtual std::shared_ptr<Type> dequalify() const;                         // Type without modifiers
        virtual TypeCategory storage_category() const;                           // Physical storage type category (ex. POINTER -> INTEGER), default to the same category
        virtual TypeIdentifier identifier() const;

        bool is_arithmetic() const;
        bool is_floating_point() const;
        bool is_integral() const;
        bool is_comparable() const;
        bool has_truth_value() const;

        virtual std::string repr() const;  // Get the type representation as in C code (ex. const int* [4])
        virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const;
        virtual std::string text() const;  // Defaults to the IR code
    };

    struct PrimitiveType : public Type {
        std::string name;
        size_t size_bits;
        size_t alignment_bits;

        PrimitiveType(TypeCategory category, std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits);

        virtual size_t size(CodeLocation location) const override;
        virtual size_t alignment(CodeLocation location) const override;
        virtual bool operator== (const Type& rhs) const override;
        bool operator== (const PrimitiveType& rhs) const;

        virtual std::string repr() const override;
        virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;
    };

    struct BooleanType : public PrimitiveType {
        BooleanType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits);

        virtual std::shared_ptr<Type> dequalify() const override;
    };

    struct IntegerType : public PrimitiveType {
        bool is_signed;

        IntegerType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits, bool is_signed);

        virtual bool operator== (const Type& rhs) const override;
        bool operator== (const IntegerType& rhs) const;

        virtual std::shared_ptr<Type> dequalify() const override;

        virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;
    };

    struct FloatingPointType : public PrimitiveType {
        FloatingPointType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits);

        virtual std::shared_ptr<Type> dequalify() const override;
    };
}

template<> struct std::hash<toycc::ir::TypeIdentifier> {
    std::size_t operator()(const toycc::ir::TypeIdentifier& s) const noexcept {
        std::size_t h1 = std::hash<toycc::ir::TypeTag>{}(s.tag);
        std::size_t h2 = std::hash<std::string>{}(s.name);
        return h1 ^ (h2 << 1); // or use boost::hash_combine
    }
};
