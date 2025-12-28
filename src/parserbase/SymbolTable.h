#pragma once

#include <map>
#include <deque>
#include <memory>
#include <string>
#include "util/flags.hpp"

namespace toycc {
    enum class TypeClassification {
        Global                = 0x01,
        Function              = 0x02,
        Variable              = 0x04,
        TypeSpecifier         = 0x08,
        StorageClassSpecifier = 0x10,
        TypeQualifier         = 0x20,
        FunctionSpecifier     = 0x40,
        AlignmentSpecifier    = 0x80,
        AtomicTypeSpecifier   = 0x100,
        EnumSpecifier         = 0x200,
    };

    struct Symbol {
        std::string name;
        Flags<TypeClassification> classification;
        std::map<std::string, std::shared_ptr<Symbol>> members;
        std::shared_ptr<Symbol> parent = nullptr;
    };

    class SymbolTable {
        private:
            std::deque<std::shared_ptr<Symbol>> scopeStack;

        public:
            SymbolTable();

            void EnterScope(std::shared_ptr<Symbol> newScope);
            void ExitScope();
            std::shared_ptr<Symbol> CurrentScope();
            bool Define(std::shared_ptr<Symbol> symbol);
            bool DefineInScope(std::shared_ptr<Symbol> currentScope, std::shared_ptr<Symbol> symbol);
            std::shared_ptr<Symbol> Resolve(std::string name, std::shared_ptr<Symbol> start_scope = nullptr);
    };
}
