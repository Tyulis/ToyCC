#pragma once

#include <vector>
#include <unordered_map>

#include "code_location.h"
#include "ir/type.h"
#include "ir/declaration.h"
#include "util/flags.hpp"

namespace toycc::ir {
    // NOTE : That TypeExpression::make(...) scheme allows for type expression simplifications under the hood
    struct PointerType : public Type {
        public:
            std::shared_ptr<Type> referenced_type;

            static std::shared_ptr<PointerType> make(CodeLocation location, std::shared_ptr<Type> referenced_type);

            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const PointerType& rhs) const;

            virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const override;
            virtual std::shared_ptr<Type> dequalify() const override;
            virtual TypeCategory storage_category() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            PointerType(CodeLocation location, std::shared_ptr<Type> referenced_type);
    };

    struct ArrayType : public Type {
        public:
            std::shared_ptr<Type> element_type;
            Operand length;

            static std::shared_ptr<ArrayType> make(CodeLocation location, std::shared_ptr<Type> element_type, Operand length);

            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const ArrayType& rhs) const;

            virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const override;
            virtual std::shared_ptr<Type> dequalify() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            ArrayType(CodeLocation location, std::shared_ptr<Type> element_type, Operand length);
    };

    struct CompoundType : public Type {
        public:
            bool is_complete = false;
            std::string name;
            std::vector<Member> members;

            virtual bool complete() const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const CompoundType& rhs) const;

            virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const override;

            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            CompoundType(TypeCategory category, std::string name, CodeLocation location, bool is_complete = false, std::vector<Member> members = {});
    };

    struct StructType : public CompoundType {
        public:
            static std::shared_ptr<StructType> make(std::string name, CodeLocation location, bool is_complete = false, std::vector<Member> members = {});

            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;

            size_t member_offset(size_t member_index) const;

            virtual std::shared_ptr<Type> dequalify() const override;
            virtual TypeIdentifier identifier() const override;

            virtual std::string repr() const override;
        protected:
            StructType(std::string name, CodeLocation location, bool is_complete = false, std::vector<Member> members = {});
    };

    struct UnionType : public CompoundType {
        public:
            static std::shared_ptr<UnionType> make(std::string name, CodeLocation location, bool is_complete = false, std::vector<Member> members = {});

            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;

            virtual std::shared_ptr<Type> dequalify() const override;
            virtual TypeIdentifier identifier() const override;

            virtual std::string repr() const override;
        protected:
            UnionType(std::string name, CodeLocation location, bool is_complete = false, std::vector<Member> members = {});
    };

    struct EnumType : public Type {
        public:
            std::string name;
            std::shared_ptr<Type> underlying_type;
            std::unordered_map<std::string, ssize_t> values;

            static std::shared_ptr<EnumType> make(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, std::unordered_map<std::string, ssize_t> values = {});

            virtual bool complete() const override;
            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const EnumType& rhs) const;

            virtual std::shared_ptr<Type> dequalify() const override;
            virtual TypeCategory storage_category() const override;
            virtual TypeIdentifier identifier() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            EnumType(std::string name, CodeLocation location, std::shared_ptr<Type> underlying_type, std::unordered_map<std::string, ssize_t> values = {});
    };

    struct FunctionType : public Type {
        public:
            std::shared_ptr<Type> return_type;
            std::vector<Member> parameters;

            static std::shared_ptr<FunctionType> make(CodeLocation location, std::shared_ptr<Type> return_type, std::vector<Member> parameters = {});

            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const FunctionType& rhs) const;

            virtual std::shared_ptr<Type> dequalify() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            FunctionType(CodeLocation location, std::shared_ptr<Type> return_type, std::vector<Member> parameters = {});
    };

    struct TypeModifier : public Type {
        public:
            std::shared_ptr<Type> underlying_type;

            // By default, defer to the underlying type
            virtual bool is_const() const override;
            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;
            virtual TypeCategory storage_category() const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const TypeModifier& rhs) const;

        protected:
            TypeModifier(TypeCategory category, CodeLocation location, std::shared_ptr<Type> underlying_type);
    };

    struct BitfieldType : public TypeModifier {
        public:
            size_t size_bits;

            static std::shared_ptr<BitfieldType> make(CodeLocation location, std::shared_ptr<Type> underlying_type, size_t size_bits);

            virtual size_t size(CodeLocation location) const override;
            virtual size_t alignment(CodeLocation location) const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const BitfieldType& rhs) const;

            virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const override;
            virtual std::shared_ptr<Type> dequalify() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            BitfieldType(CodeLocation location, std::shared_ptr<Type> underlying_type, size_t size_bits);
    };

    struct AlignedType : public TypeModifier {
        public:
            size_t alignment_bits;

            static std::shared_ptr<Type> make(CodeLocation location, std::shared_ptr<Type> underlying_type, size_t alignment_bits);

            virtual size_t alignment(CodeLocation location) const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const AlignedType& rhs) const;

            virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const override;
            virtual std::shared_ptr<Type> dequalify() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            AlignedType(CodeLocation location, std::shared_ptr<Type> underlying_type, size_t alignment_bits);
    };

    enum class TypeQualifier {
        CONST    = 0x01,
        VOLATILE = 0x02,
        RESTRICT = 0x04,
        ATOMIC   = 0x08,
    };

    struct QualifiedType : public TypeModifier {
        public:
            Flags<TypeQualifier> qualifiers;

            static std::shared_ptr<QualifiedType> make(CodeLocation location, std::shared_ptr<Type> underlying_type, Flags<TypeQualifier> qualifiers);

            virtual bool is_const() const override;
            virtual bool operator== (const Type& rhs) const override;
            bool operator== (const QualifiedType& rhs) const;

            virtual std::shared_ptr<Type> dereference(std::optional<size_t> index, CodeLocation location) const override;
            virtual std::shared_ptr<Type> dequalify() const override;

            virtual std::string repr() const override;
            virtual std::string ir_code(std::unordered_set<const Type*> parents = {}) const override;

        protected:
            QualifiedType(CodeLocation location, std::shared_ptr<Type> underlying_type, Flags<TypeQualifier> qualifiers);
    };
}
