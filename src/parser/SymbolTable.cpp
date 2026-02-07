#include <format>
#include "diagnostic.h"
#include "parser/SymbolTable.h"

namespace toycc {
    SymbolTable::SymbolTable() {
        std::shared_ptr<Symbol> globalScope = std::make_shared<Symbol>("global", TypeClassification::Global);
        scopeStack.push_front(globalScope);

        Define(std::make_shared<Symbol>("auto",          TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("constexpr",     TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("extern",        TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("register",      TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("static",        TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("thread_local",  TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("_Thread_local", TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("typedef",       TypeClassification::StorageClassSpecifier, true));

        Define(std::make_shared<Symbol>("enum",   TypeClassification::EnumSpecifier, true));

        Define(std::make_shared<Symbol>("struct", TypeClassification::StorageClassSpecifier, true));
        Define(std::make_shared<Symbol>("union",  TypeClassification::StorageClassSpecifier, true));

        Define(std::make_shared<Symbol>("const",    TypeClassification::TypeQualifier, true));
        Define(std::make_shared<Symbol>("restrict", TypeClassification::TypeQualifier, true));
        Define(std::make_shared<Symbol>("volatile", TypeClassification::TypeQualifier, true));
        Define(std::make_shared<Symbol>("_Atomic",  TypeClassification::TypeQualifier | TypeClassification::AtomicTypeSpecifier, true));

        Define(std::make_shared<Symbol>("void",          TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("char",          TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("short",         TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("int",           TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("long",          TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("float",         TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("double",        TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("signed",        TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("unsigned",      TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("_BitInt",       TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("bool",          TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("_Bool",         TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("_Complex",      TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("_Decimal32",    TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("_Decimal64",    TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("_Decimal128",   TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__m128",        TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__m128d",       TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__m128i",       TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__extension__", TypeClassification::TypeSpecifier, true));

        Define(std::make_shared<Symbol>("__builtin_va_list",                 TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_has_attribute",           TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_speculation_safe_value",  TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_types_compatible_p",      TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_choose_expr",             TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_tgmath",                  TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_constant_p",              TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_is_constant_evaluated",   TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_bit_cast",                TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_expect",                  TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_expect_with_probability", TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_trap",                    TypeClassification::TypeSpecifier, true));
        // Define(std::make_shared<Symbol>("__builtin_unreachable", TypeClassification::FunctionSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_assoc_barrier",           TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_assume_aligned",          TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_LINE",                    TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_FUNCTION",                TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_FILE",                    TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin___clear_cache",           TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_prefetch",                TypeClassification::Function, true));
        Define(std::make_shared<Symbol>("__builtin_classify_type",           TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_extend_pointer",          TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_goacc_parlevel_id",       TypeClassification::TypeSpecifier, true));
        Define(std::make_shared<Symbol>("__builtin_goacc_parlevel_size",     TypeClassification::TypeSpecifier, true));

        Define(std::make_shared<Symbol>("inline",     TypeClassification::FunctionSpecifier, true));
        Define(std::make_shared<Symbol>("_Noreturn",  TypeClassification::FunctionSpecifier, true));
        Define(std::make_shared<Symbol>("__inline__", TypeClassification::FunctionSpecifier, true));

        Define(std::make_shared<Symbol>("__cdecl",      TypeClassification::FunctionSpecifier, true)); // MS
        Define(std::make_shared<Symbol>("__clrcall",    TypeClassification::FunctionSpecifier, true)); // MS
        Define(std::make_shared<Symbol>("__stdcall",    TypeClassification::FunctionSpecifier, true)); // MS
        Define(std::make_shared<Symbol>("__fastcall",   TypeClassification::FunctionSpecifier, true)); // MS
        Define(std::make_shared<Symbol>("__thiscall",   TypeClassification::FunctionSpecifier, true)); // MS
        Define(std::make_shared<Symbol>("__vectorcall", TypeClassification::FunctionSpecifier, true)); // MS

        Define(std::make_shared<Symbol>("_purecall",                  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_purecall_handler",          TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_onexit_t",                  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_locale_t",                  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_invalid_parameter_handler", TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__inline",                   TypeClassification::TypeSpecifier, true)); // gcc

        Define(std::make_shared<Symbol>("__int8",    TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__int16",   TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__int32",   TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__int64",   TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__int128",  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_Float16",  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_Float32",  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_Float64",  TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("_Float128", TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__v8hf",    TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__bf16",    TypeClassification::TypeSpecifier, true)); // gcc
        Define(std::make_shared<Symbol>("__v16bf",   TypeClassification::TypeSpecifier, true)); // gcc

        Define(std::make_shared<Symbol>("__declspec",    TypeClassification::FunctionSpecifier, true)); // MS
        Define(std::make_shared<Symbol>("__attribute__", TypeClassification::FunctionSpecifier, true)); // GCC

        Define(std::make_shared<Symbol>("alignas", TypeClassification::AlignmentSpecifier, true));
        Define(std::make_shared<Symbol>("align",   TypeClassification::AlignmentSpecifier, true));
    }

    void SymbolTable::EnterScope(std::shared_ptr<Symbol> newScope) {
        if (scopeStack.front().get() != newScope.get())
            scopeStack.push_front(newScope);
    }

    void SymbolTable::ExitScope() {
        if (scopeStack.size() < 2)
            throw Diagnostic(DiagnosticLevel::INTERNAL_ERROR, "Attempted to pop the global scope");

        std::shared_ptr<Symbol> current = scopeStack.front();
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
                auto it = scope->members.find(name);
                if (it != scope->members.end())
                    return it->second;
            }
            return nullptr;
        } else {
            auto it = start_scope->members.find(name);
            if (it == start_scope->members.end())  return nullptr;
            else                                   return it->second;
        }
    }

    std::shared_ptr<Symbol> SymbolTable::PushBlockScope() {
        std::shared_ptr<Symbol> blockScope = std::make_shared<Symbol>(std::format("block{}", ++blockCounter), TypeClassification::Block, true);
        EnterScope(blockScope);
        return blockScope;
    }

    void SymbolTable::PopBlockScope() {
        ExitScope();
    }
}
