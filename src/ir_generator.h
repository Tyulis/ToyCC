#pragma once

#include <memory>
#include "ParserRuleContext.h"

#include "ir/scope.h"
#include "parser/CParser.h"
#include "parser/CBaseListener.h"
#include "source_map.h"

namespace toycc {
    class IRGenerator : public CBaseListener {
        private:
            const SourceMap& source_map;
            std::shared_ptr<ir::Scope> global_scope;
            std::deque<std::shared_ptr<ir::Scope>> scope_stack;

            size_t unique_id = 0;

        public:
            IRGenerator(const SourceMap& source_map);

            virtual void exitDeclarationDeclaration(CParser::DeclarationDeclarationContext* context) override;

        private:
            void register_typedef(CParser::DeclarationDeclarationContext* context);

        private:
            CodeLocation get_location(antlr4::ParserRuleContext* context) const;
            std::string anonymous_identifier();

            std::shared_ptr<ir::Type> resolve_type(std::string name) noexcept;
            std::shared_ptr<ir::Type> get_type(std::string name, CodeLocation location);
    };
}
