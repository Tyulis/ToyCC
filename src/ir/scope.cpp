#include <sstream>

#include "ir/scope.h"
#include "util/strings.h"

namespace toycc::ir {
    std::string Scope::ir_code() const {
        std::stringstream code;
        for (std::pair<TypeIdentifier, std::shared_ptr<Type>> item : types)
            code << item.second->ir_code() << ";\n";
        for (std::pair<std::string, std::shared_ptr<Declaration>> item : typedefs)
            code << item.second->ir_code() << ";\n";
        for (std::pair<std::string, std::shared_ptr<Declaration>> item : locals)
            code << item.second->ir_code() << ";\n";
        for (std::shared_ptr<Statement> item : statements)
            code << item->ir_code() << ";\n";
        return rtrim(code.str());
    }
}
