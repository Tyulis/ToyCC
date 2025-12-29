#include "ir/generator.h"

namespace toycc::ir {
    Generator::TemporaryGenerator::TemporaryGenerator(std::shared_ptr<Type> type, CodeLocation location, Generator& generator)
        : type(type), location(location), generator(generator) {}

    std::shared_ptr<Declaration> Generator::TemporaryGenerator::operator()() const {
        return generator.declare_temporary(type, location);
    }

    Generator::TemporaryGenerator Generator::make_temporary_generator(std::shared_ptr<Type> type, CodeLocation location) {
        return TemporaryGenerator {type, location, *this};
    }
}
