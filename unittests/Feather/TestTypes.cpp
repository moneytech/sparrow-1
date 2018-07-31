#include "StdInc.h"
#include "Common/GeneralFixture.hpp"
#include "Common/RcBasic.hpp"
#include "Common/TypeFactory.hpp"
#include "Nest/Utils/cppif/StringRef.hpp"
#include "Feather/Api/Feather.h"
#include "Feather/Utils/cppif/FeatherTypes.hpp"
#include "Nest/MockNode.hpp"

using namespace Nest;
using namespace Feather;

//! Fixture used in testing the Feather types
struct FeatherTypesFixture : SparrowGeneralFixture {
    FeatherTypesFixture();
    ~FeatherTypesFixture();
};

FeatherTypesFixture::FeatherTypesFixture() {
    using TypeFactory::g_dataTypeDecls;
    g_dataTypeDecls.push_back(createNativeDatatypeNode(StringRef("i8"), globalContext_));
    g_dataTypeDecls.push_back(createNativeDatatypeNode(StringRef("i16"), globalContext_));
    g_dataTypeDecls.push_back(createNativeDatatypeNode(StringRef("i32"), globalContext_));
    g_dataTypeDecls.push_back(createDatatypeNode(StringRef("FooType"), globalContext_));
    g_dataTypeDecls.push_back(createDatatypeNode(StringRef("BarType"), globalContext_));
    g_dataTypeDecls.push_back(createDatatypeNode(StringRef("NullType"), globalContext_));
}
FeatherTypesFixture::~FeatherTypesFixture() {}

TEST_CASE_METHOD(FeatherTypesFixture, "User can create Feather types with given properties") {
    rc::prop("Can create VoidTypes for proper mode", [](EvalMode mode) {
        auto type = VoidType::get(mode);
        REQUIRE(type);
        REQUIRE(type.kind() == Feather_getVoidTypeKind());
        REQUIRE(type.mode() == mode);
        REQUIRE(type.canBeUsedAtRt());
        REQUIRE(!type.hasStorage());
    });
    rc::prop("Can create data types", [](EvalMode mode) {
        using TypeFactory::g_dataTypeDecls;

        NodeHandle decl = g_dataTypeDecls[*rc::gen::inRange(0, (int) g_dataTypeDecls.size())];
        int numRefs = *rc::gen::inRange(0, 10);

        auto type = DataType::get(decl, numRefs, mode);
        REQUIRE(type);
        REQUIRE(type.kind() == Feather_getDataTypeKind());
        REQUIRE(type.mode() == mode);
        REQUIRE(type.canBeUsedAtRt());
        REQUIRE(type.hasStorage());
        REQUIRE(type.numReferences() == numRefs);
        REQUIRE(type.referredNode() == decl);
    });
    rc::prop("Can create LValue types", [](EvalMode mode) {
        using TypeFactory::g_dataTypeDecls;

        NodeHandle decl = g_dataTypeDecls[*rc::gen::inRange(0, (int) g_dataTypeDecls.size())];
        int numRefs = *rc::gen::inRange(0, 10);

        auto baseType = DataType::get(decl, numRefs, mode);
        auto type = LValueType::get(baseType);
        REQUIRE(type);
        REQUIRE(type.kind() == Feather_getLValueTypeKind());
        REQUIRE(type.mode() == mode);
        REQUIRE(type.canBeUsedAtRt());
        REQUIRE(type.hasStorage());
        REQUIRE(type.numReferences() == numRefs+1);
        REQUIRE(type.referredNode() == decl);

        REQUIRE(type.base() == baseType);
        REQUIRE(type.toRef().numReferences() == numRefs+1);
    });
}

TEST_CASE_METHOD(FeatherTypesFixture, "User can add or remove references") {
    rc::prop("Adding reference increases the number of references", []() {
        TypeWithStorage base = *TypeFactory::arbBasicStorageType(modeUnspecified, 0, 10);
        DataType newType = addRef(base);
        REQUIRE(newType.numReferences() == base.numReferences()+1);
    });
    rc::prop("Removing reference decreases the number of references", []() {
        TypeWithStorage base = *TypeFactory::arbBasicStorageType(modeUnspecified, 1, 10);
        DataType newType = removeRef(base);
        REQUIRE(newType.numReferences() == base.numReferences()-1);
    });
    rc::prop("Removing all references decreases the number of references to 0", []() {
        TypeWithStorage base = *TypeFactory::arbBasicStorageType(modeUnspecified, 0, 10);
        DataType newType = removeAllRefs(base);
        REQUIRE(newType.numReferences() == 0);
    });
    rc::prop("lvalueToRefIfPresent does nothing for data types", []() {
        DataType base = *TypeFactory::arbDataType(modeUnspecified, 0, 10);
        TypeBase newType = lvalueToRefIfPresent(base);
        REQUIRE(newType == base);
    });
    rc::prop("lvalueToRefIfPresent keeps the same number of references", []() {
        DataType base = *TypeFactory::arbBasicStorageType(modeUnspecified, 0, 10);
        TypeWithStorage newType = TypeWithStorage(lvalueToRefIfPresent(base));
        REQUIRE(newType.numReferences() == base.numReferences());
    });
}
