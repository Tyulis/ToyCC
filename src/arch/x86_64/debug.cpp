#include "debug/generation.h"
#include "debug/unit.h"
#include "ir/declaration.h"
#include "ir/flow.h"
#include "ir/type_expressions.h"
#include "arch/x86_64/codegen.h"

namespace toycc::arch::x86_64 {
    debug::DebugInfoEntry CodeGenerator::procedure_debuginfo(const ir::Procedure& procedure, debug::CompilationUnit& debuginfo) {
        std::shared_ptr<ir::Declaration> declaration = procedure.declaration;
        std::shared_ptr<ir::FunctionType> function_type = std::static_pointer_cast<ir::FunctionType>(declaration->type);

        debug::DebugInfoEntry entry = debug::DebugInfoEntry(debug::Tag::DW_TAG_subprogram)
            .add(debug::Attribute::DW_AT_name,            debug::Form::DW_FORM_strp,  debuginfo.string(declaration->name))
            .add(debug::Attribute::DW_AT_external,        debug::Form::DW_FORM_flag,  !(declaration->storage & ir::StorageClass::STATIC))
            .add(debug::Attribute::DW_AT_main_subprogram, debug::Form::DW_FORM_flag,  declaration->name == "main")
            .add(debug::Attribute::DW_AT_low_pc,          debug::Form::DW_FORM_addr,  procedure.start_label())
            .add(debug::Attribute::DW_AT_high_pc,         debug::Form::DW_FORM_data8, std::format("{}-{}", procedure.end_label(), procedure.start_label()));

        // Return type : only if the function actually returns something
        if (function_type->return_type->category != ir::TypeCategory::VOID)
            entry.add(debug::Attribute::DW_AT_type, debug::Form::DW_FORM_ref8, debuginfo.type(function_type->return_type));

        return entry;
    }
}
