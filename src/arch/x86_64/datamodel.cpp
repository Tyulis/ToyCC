#include "code_location.h"
#include "ir/type.h"
#include "ir/type_expressions.h"
#include "arch/x86_64/datamodel.h"

namespace toycc::arch::x86_64 {
    using namespace toycc::ir;

    DataModel::DataModel() {
        void_type               = std::make_shared<PrimitiveType>(TypeCategory::VOID, "void", BUILTIN_LOCATION, 0, 0);
        boolean_type            = std::make_shared<BooleanType> ("bool",                   BUILTIN_LOCATION,  8,  8);

        signed_char_type        = std::make_shared<IntegerType> ("signed char",            BUILTIN_LOCATION,  8,  8, true);
        unsigned_char_type      = std::make_shared<IntegerType> ("unsigned char",          BUILTIN_LOCATION,  8,  8, false);
        signed_short_type       = std::make_shared<IntegerType> ("signed short int",       BUILTIN_LOCATION, 16, 16, true);
        unsigned_short_type     = std::make_shared<IntegerType> ("unsigned short int",     BUILTIN_LOCATION, 16, 16, false);
        signed_int_type         = std::make_shared<IntegerType> ("signed int",             BUILTIN_LOCATION, 32, 32, true);
        unsigned_int_type       = std::make_shared<IntegerType> ("unsigned int",           BUILTIN_LOCATION, 32, 32, false);
        signed_long_type        = std::make_shared<IntegerType> ("signed long int",        BUILTIN_LOCATION, 64, 64, true);
        unsigned_long_type      = std::make_shared<IntegerType> ("unsigned long int",      BUILTIN_LOCATION, 64, 64, false);
        signed_long_long_type   = std::make_shared<IntegerType> ("signed long long int",   BUILTIN_LOCATION, 64, 64, true);
        unsigned_long_long_type = std::make_shared<IntegerType> ("unsigned long long int", BUILTIN_LOCATION, 64, 64, false);

        float_type       = std::make_shared<FloatingPointType> ("float",       BUILTIN_LOCATION,  32,  32);
        double_type      = std::make_shared<FloatingPointType> ("double",      BUILTIN_LOCATION,  64,  64);
        long_double_type = std::make_shared<FloatingPointType> ("long double", BUILTIN_LOCATION, 128, 128);

        label_type             = std::make_shared<PrimitiveType> (TypeCategory::LABEL, ".Tlabel", BUILTIN_LOCATION, 64, 64);
        literal_character_type = QualifiedType::make(BUILTIN_LOCATION, signed_char_type, TypeQualifier::CONST);
        literal_integer_type   = QualifiedType::make(BUILTIN_LOCATION, signed_int_type,  TypeQualifier::CONST);
        literal_floating_type  = QualifiedType::make(BUILTIN_LOCATION, double_type,      TypeQualifier::CONST);

        size_type            = unsigned_long_type;
        pointer_type         = unsigned_long_type;
        offset_type          = signed_long_type;
        enum_underlying_type = literal_integer_type;
        void_pointer_type    = PointerType::make(BUILTIN_LOCATION, void_type);

        pointer_size      = 8;
        pointer_alignment = 8;
    }

    DataModel DATAMODEL;
}
