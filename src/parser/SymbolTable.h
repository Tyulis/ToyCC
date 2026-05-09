#pragma once

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include "util/flags.hpp"

namespace toycc {
    enum class TypeClassification {
        Global                = 0x01,
        Block                 = 0x02,
        Function              = 0x04,
        Variable              = 0x08,
        TypeSpecifier         = 0x10,
        StorageClassSpecifier = 0x20,
        TypeQualifier         = 0x40,
        FunctionSpecifier     = 0x80,
        AlignmentSpecifier    = 0x100,
        AtomicTypeSpecifier   = 0x200,
        EnumSpecifier         = 0x400,
        PrefixedTypeSpecifier = 0x800,  // Type, but defined as `struct mytype`, `union mytype` or `enum mytype`, not directly `mytype`
    };

    struct Symbol {
        std::string name;
        Flags<TypeClassification> classification;
        bool predefined = false;

        std::unordered_map<std::string, std::shared_ptr<Symbol>> members;
        std::shared_ptr<Symbol> parent = nullptr;
    };

    class SymbolTable {
        private:
            std::deque<std::shared_ptr<Symbol>> scopeStack;
            size_t blockCounter;

        public:
            SymbolTable();

            void EnterScope(std::shared_ptr<Symbol> newScope);
            void ExitScope();
            std::shared_ptr<Symbol> CurrentScope();
            bool Define(std::shared_ptr<Symbol> symbol);
            bool DefineInScope(std::shared_ptr<Symbol> currentScope, std::shared_ptr<Symbol> symbol);
            std::shared_ptr<Symbol> Resolve(std::string name, std::shared_ptr<Symbol> start_scope = nullptr);
            std::shared_ptr<Symbol> PushBlockScope();
            void PopBlockScope();
    };
}
