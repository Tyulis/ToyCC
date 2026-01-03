#include "diagnostic.h"
#include "parser/SymbolTable.h"

namespace toycc {
    SymbolTable::SymbolTable() {
        std::shared_ptr<Symbol> globalScope = std::make_shared<Symbol>("global", TypeClassification::Global);
        scopeStack.push_front(globalScope);

        Define(std::make_shared<Symbol>("auto",          TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("constexpr",     TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("extern",        TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("register",      TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("static",        TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("thread_local",  TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("_Thread_local", TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("typedef",       TypeClassification::StorageClassSpecifier));

        Define(std::make_shared<Symbol>("enum",          TypeClassification::EnumSpecifier));

        Define(std::make_shared<Symbol>("struct",        TypeClassification::StorageClassSpecifier));
        Define(std::make_shared<Symbol>("union",         TypeClassification::StorageClassSpecifier));

        Define(std::make_shared<Symbol>("const",         TypeClassification::TypeQualifier));
        Define(std::make_shared<Symbol>("restrict",      TypeClassification::TypeQualifier));
        Define(std::make_shared<Symbol>("volatile",      TypeClassification::TypeQualifier));
        Define(std::make_shared<Symbol>("_Atomic",       TypeClassification::TypeQualifier | TypeClassification::AtomicTypeSpecifier));

        Define(std::make_shared<Symbol>("void",          TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("char",          TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("short",         TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("int",           TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("long",          TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("float",         TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("double",        TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("signed",        TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("unsigned",      TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("_BitInt",       TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("bool",          TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("_Bool",         TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("_Complex",      TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("_Decimal32",    TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("_Decimal64",    TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("_Decimal128",   TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__m128",        TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__m128d",       TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__m128i",       TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__extension__", TypeClassification::TypeSpecifier));

        Define(std::make_shared<Symbol>("__builtin_va_list",                 TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_has_attribute",           TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_speculation_safe_value",  TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_types_compatible_p",      TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_choose_expr",             TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_tgmath",                  TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_constant_p",              TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_is_constant_evaluated",   TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_bit_cast",                TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_expect",                  TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_expect_with_probability", TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_trap",                    TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_unreachable",             TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_assoc_barrier",           TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_assume_aligned",          TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_LINE",                    TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_FUNCTION",                TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_FILE",                    TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin___clear_cache",           TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_prefetch",                TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_classify_type",           TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_extend_pointer",          TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_goacc_parlevel_id",       TypeClassification::TypeSpecifier));
        Define(std::make_shared<Symbol>("__builtin_goacc_parlevel_size",     TypeClassification::TypeSpecifier));

        Define(std::make_shared<Symbol>("inline",        TypeClassification::FunctionSpecifier));
        Define(std::make_shared<Symbol>("_Noreturn",     TypeClassification::FunctionSpecifier));
        Define(std::make_shared<Symbol>("__inline__",    TypeClassification::FunctionSpecifier));

        Define(std::make_shared<Symbol>("__cdecl",       TypeClassification::FunctionSpecifier)); // MS
        Define(std::make_shared<Symbol>("__clrcall",     TypeClassification::FunctionSpecifier)); // MS
        Define(std::make_shared<Symbol>("__stdcall",     TypeClassification::FunctionSpecifier)); // MS
        Define(std::make_shared<Symbol>("__fastcall",    TypeClassification::FunctionSpecifier)); // MS
        Define(std::make_shared<Symbol>("__thiscall",    TypeClassification::FunctionSpecifier)); // MS
        Define(std::make_shared<Symbol>("__vectorcall",  TypeClassification::FunctionSpecifier)); // MS

        Define(std::make_shared<Symbol>("__declspec",    TypeClassification::FunctionSpecifier)); // MS
        Define(std::make_shared<Symbol>("__attribute__", TypeClassification::FunctionSpecifier)); // GCC

        Define(std::make_shared<Symbol>("alignas",       TypeClassification::AlignmentSpecifier));
        Define(std::make_shared<Symbol>("align",         TypeClassification::AlignmentSpecifier));
    }

    void SymbolTable::EnterScope(std::shared_ptr<Symbol> newScope) {
        if (scopeStack.front().get() == newScope.get())
            return;
        scopeStack.push_front(newScope);
    }

    void SymbolTable::ExitScope() {
        if (scopeStack.size() < 2)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to pop the global scope");
        scopeStack.pop_front();
    }

    std::shared_ptr<Symbol> SymbolTable::CurrentScope() {
        return scopeStack.front();
    }

    bool SymbolTable::Define(std::shared_ptr<Symbol> symbol) {
        return DefineInScope(CurrentScope(), symbol);
    }

    bool SymbolTable::DefineInScope(std::shared_ptr<Symbol> currentScope, std::shared_ptr<Symbol> symbol) {
        if (currentScope->members.contains(symbol->name))
            return false; // Symbol already defined in the current scope

        symbol->parent = currentScope;
        currentScope->members[symbol->name] = symbol;
        return true;
    }

    std::shared_ptr<Symbol> SymbolTable::Resolve(std::string name, std::shared_ptr<Symbol> start_scope) {
        if (start_scope.get() == nullptr) {
            for (std::shared_ptr<Symbol> scope : scopeStack) {
                if (scope->members.contains(name))
                    return scope->members.at(name);
            }
            return nullptr;
        } else {
            if (start_scope->members.contains(name))
                return start_scope->members.at(name);
            return nullptr;
        }
    }
}
