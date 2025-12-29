#pragma once

#include <memory>
#include <string>

#include "code_location.h"

namespace toycc::ir {
    enum class TypeTag {
        DIRECT, STRUCT, UNION, ENUM, TYPEDEF,
    };

    enum class TypeCategory {
        VOID, BUILTIN, BOOL, INTEGER, FLOAT, POINTER, ARRAY, STRUCT, UNION, ENUM, FUNCTION, BITFIELD, QUALIFIED, ALIGNED,
    };

    struct TypeIdentifier {
        TypeTag tag;
        std::string name;

        std::string text() const;
        bool operator== (const TypeIdentifier& rhs) const = default;
    };

    struct Type {
        TypeCategory category;
        std::string name;
        CodeLocation location;

        Type(TypeCategory category, std::string name, CodeLocation location);

        virtual bool complete() const;  // Defaults to true
        virtual bool is_const() const;  // Defaults to false
        virtual size_t size(CodeLocation location) const;       // In bytes
        virtual size_t alignment(CodeLocation location) const;  // In bytes
        virtual bool operator== (const Type& rhs) const;  // Defaults to same category

        virtual std::shared_ptr<Type> dereference(CodeLocation location) const;  // Type emitted by a dereference of this type. Defaults to a throw.
        virtual std::shared_ptr<Type> dequalify() const;                         // Type without modifiers, default to the same type

        TypeIdentifier identifier() const;
        bool is_arithmetic() const;
        bool is_integral() const;
        bool is_comparable() const;
        bool has_truth_value() const;

        virtual std::string ir_code() const;
        virtual std::string text() const;  // Defaults to the IR code
    };

    struct PrimitiveType : public Type {
        size_t size_bits;
        size_t alignment_bits;

        PrimitiveType(TypeCategory category, std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits);

        virtual size_t size(CodeLocation location) const override;
        virtual size_t alignment(CodeLocation location) const override;
        virtual bool operator== (const Type& rhs) const override;
        bool operator== (const PrimitiveType& rhs) const;

        virtual std::string ir_code() const override;
    };

    struct BooleanType : public PrimitiveType {
        BooleanType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits);
    };

    struct IntegerType : public PrimitiveType {
        bool is_signed;

        IntegerType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits, bool is_signed);

        virtual bool operator== (const Type& rhs) const override;
        bool operator== (const IntegerType& rhs) const;

        virtual std::string ir_code() const override;
    };

    struct FloatingPointType : public PrimitiveType {
        FloatingPointType(std::string name, CodeLocation location, size_t size_bits, size_t alignment_bits);
    };

    std::string category_repr(TypeCategory category);
}

template<> struct std::hash<toycc::ir::TypeIdentifier> {
    std::size_t operator()(const toycc::ir::TypeIdentifier& s) const noexcept {
        std::size_t h1 = std::hash<toycc::ir::TypeTag>{}(s.tag);
        std::size_t h2 = std::hash<std::string>{}(s.name);
        return h1 ^ (h2 << 1); // or use boost::hash_combine
    }
};
