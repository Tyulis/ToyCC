#include "semantic/analyzer.h"

namespace toycc::semantic {
    SemanticAnalyzer::TemporaryGenerator::TemporaryGenerator(std::shared_ptr<Type> type, CodeLocation location, SemanticAnalyzer& analyzer)
        : type(type), location(location), analyzer(analyzer) {}

    std::shared_ptr<Declaration> SemanticAnalyzer::TemporaryGenerator::operator()() const {
        return analyzer.declare_temporary(type, location);
    }

    SemanticAnalyzer::TemporaryGenerator SemanticAnalyzer::make_temporary_generator(std::shared_ptr<Type> type, CodeLocation location) {
        return TemporaryGenerator {type, location, *this};
    }
}
