#include "Feather/src/StdInc.h"
#include "Feather/src/Api/Feather_Nodes.h"
#include "Feather/src/FeatherNodeCommonsCpp.h"

#include "Feather/Api/Feather.h"

#include "Feather/Utils/FeatherUtils.hpp"
#include "Feather/Utils/cppif/FeatherTypes.hpp"
#include "Feather/Utils/cppif/FeatherNodes.hpp"

#include "Nest/Utils/Diagnostic.hpp"
#include "Nest/Utils/Alloc.h"
#include "Nest/Utils/cppif/StringRef.hpp"
#include "Nest/Utils/NodeUtils.h"
#include "Nest/Utils/cppif/NodeHandle.hpp"
#include "Nest/Utils/cppif/NodeUtils.hpp"
#include "Nest/Utils/cppif/NodeRange.hpp"
#include "Nest/Api/Node.h"
#include "Nest/Api/NodeKindRegistrar.h"
#include "Nest/Api/Modifier.h"

using namespace Feather;

#define REQUIRE_NODE(loc, node)                                                                    \
    if (node)                                                                                      \
        ;                                                                                          \
    else                                                                                           \
        REP_INTERNAL((loc), "Expected AST node (%1%)") % (#node)
#define REQUIRE_TYPE(loc, type)                                                                    \
    if (type)                                                                                      \
        ;                                                                                          \
    else                                                                                           \
        REP_INTERNAL((loc), "Expected type (%1%)") % (#type)

// namespace
// {

void CtProcessMod_afterSemanticCheck(Nest_Modifier* mod, Node* node) { Nest_ctProcess(node); }
Nest_Modifier ctProcessMod = {modTypeAfterSemanticCheck, &CtProcessMod_afterSemanticCheck};

const char* propResultVoid = "nodeList.resultVoid";

bool isField(Node* node) {
    if (node->nodeKind != nkFeatherDeclVar)
        return false;

    // Check the direct parent is a class that contains the given node
    Nest_SymTab* st = node->context->currentSymTab;
    Node* parent = st ? st->node : nullptr;
    parent = parent ? Nest_explanation(parent) : nullptr;
    if (parent && parent->nodeKind == nkFeatherDeclClass) {
        for (Node* f : parent->children)
            if (f == node)
                return true;
    }
    return false;
}

bool _getBoolCtValue(Node* ctVal) { return (0 != *Feather_getCtValueData<unsigned char>(ctVal)); }

Node* Nop_SemanticCheck(Node* node) {
    node->type = Feather_getVoidType(node->context->evalMode);
    return node;
}

Node* TypeNode_SemanticCheck(Node* node) {
    node->type = Nest_getCheckPropertyType(node, "givenType");
    return node;
}
const char* TypeNode_toString(Node* node) {
    ostringstream os;
    os << "type(" << Nest_getCheckPropertyType(node, "givenType") << ")";
    return dupString(os.str().c_str());
}

Node* BackendCode_SemanticCheck(Node* node) {
    EvalMode mode = Feather_BackendCode_getEvalMode(node);
    if (!node->type)
        node->type = Feather_getVoidType(mode);

    // CT process this node right after semantic check
    Nest_addModifier(node, &ctProcessMod);
    return node;
}
const char* BackendCode_toString(Node* node) {
    ostringstream os;
    os << "backendCode(" << Feather_BackendCode_getCode(node).begin << ")";
    return dupString(os.str().c_str());
}

TypeRef NodeList_ComputeType(Node* node) {
    if (Nest_hasProperty(node, propResultVoid))
        node->type = Feather_getVoidType(node->context->evalMode);

    // Compute the type for all the children
    for (Node* c : node->children) {
        if (c && !Nest_computeType(c))
            continue;
    }

    // Get the type of the last node
    unsigned numChildren = Nest_nodeArraySize(node->children);
    TypeRef res = (Nest_hasProperty(node, propResultVoid) || numChildren == 0 ||
                          !at(node->children, numChildren - 1)->type)
                          ? Feather_getVoidType(node->context->evalMode)
                          : at(node->children, numChildren - 1)->type;
    res = Feather_adjustMode(res, node->context, node->location);
    return res;
}
Node* NodeList_SemanticCheck(Node* node) {
    // Semantic check each of the children
    bool hasNonCtChildren = false;
    for (Node* c : node->children) {
        if (c) {
            if (!Nest_semanticCheck(c))
                return nullptr;
            hasNonCtChildren = hasNonCtChildren || !Feather_isCt(c);
        }
    }

    // Make sure the type is computed
    if (!node->type) {
        // Get the type of the last node
        unsigned numChildren = Nest_nodeArraySize(node->children);
        TypeRef t = (Nest_hasProperty(node, propResultVoid) || numChildren == 0 ||
                            !at(node->children, numChildren - 1)->type)
                            ? Feather_getVoidType(node->context->evalMode)
                            : at(node->children, numChildren - 1)->type;
        t = Feather_adjustMode(t, node->context, node->location);
        node->type = t;
        Feather_checkEvalMode(node);
    }
    return node;
}

void LocalSpace_SetContextForChildren(Node* node) {
    node->childrenContext = Nest_mkChildContextWithSymTab(node->context, node, modeUnspecified);
    Nest_defaultFunSetContextForChildren(node);
}
TypeRef LocalSpace_ComputeType(Node* node) { return Feather_getVoidType(node->context->evalMode); }
Node* LocalSpace_SemanticCheck(Node* node) {
    // Compute type first
    Nest_computeType(node);

    // Semantic check each of the children
    for (Node* c : node->children) {
        Nest_semanticCheck(c); // Ignore possible errors
    }
    Feather_checkEvalMode(node);
    return node;
}

Node* GlobalConstructAction_SemanticCheck(Node* node) {
    Node* act = at(node->children, 0);
    if (!Nest_semanticCheck(act))
        return nullptr;

    node->type = Feather_getVoidType(node->context->evalMode);

    // For CT construct actions, evaluate them asap
    if (Feather_isCt(act)) {
        Nest_ctEval(act);
        return Feather_mkNop(node->location);
    }
    return node;
}

Node* GlobalDestructAction_SemanticCheck(Node* node) {
    Node* act = at(node->children, 0);
    if (!Nest_semanticCheck(act))
        return nullptr;

    node->type = Feather_getVoidType(node->context->evalMode);

    // We never CT evaluate global destruct actions
    if (Feather_isCt(act)) {
        return Feather_mkNop(node->location);
    }
    return node;
}

// Both for ScopeDestructAction and for TempDestructAction
Node* ScopeTempDestructAction_SemanticCheck(Node* node) {
    Node* act = at(node->children, 0);
    if (!Nest_semanticCheck(act))
        return nullptr;
    node->type = Feather_getVoidType(node->context->evalMode);
    return node;
}

void ChangeMode_SetContextForChildren(Node* node) {
    auto curMode = (EvalMode)Nest_getCheckPropertyInt(node, propEvalMode);
    EvalMode newMode = curMode != modeUnspecified ? curMode : node->context->evalMode;
    node->childrenContext = Nest_mkChildContext(node->context, newMode);
    Nest_defaultFunSetContextForChildren(node);
}
Node* ChangeMode_SemanticCheck(Node* node) {
    Node* exp = at(node->children, 0);

    // Make sure we are allowed to change the mode
    EvalMode baseMode = node->context->evalMode;
    auto curMode = (EvalMode)Nest_getCheckPropertyInt(node, propEvalMode);
    EvalMode newMode = curMode != modeUnspecified ? curMode : node->context->evalMode;
    if (newMode == modeUnspecified)
        REP_INTERNAL(node->location, "Cannot change the mode to Unspecified");
    if (newMode == modeRt && baseMode != modeRt)
        REP_ERROR_RET(nullptr, node->location, "Cannot change mode to RT in a CT context (%1%)") %
                baseMode;

    if (!exp)
        REP_INTERNAL(node->location, "No node specified as child to a ChangeMode node");

    return Nest_semanticCheck(exp);
}
const char* ChangeMode_toString(Node* node) {
    Node* exp = at(node->children, 0);
    ostringstream os;
    auto curMode = (EvalMode)Nest_getCheckPropertyInt(node, propEvalMode);
    EvalMode newMode = curMode != modeUnspecified ? curMode : node->context->evalMode;
    os << "changeMode(" << exp << ", " << newMode << ")";
    return dupString(os.str().c_str());
}

/// Checks if the given node has some possible instructions
/// Does not need to be 100% accurate, can be conservative
bool canHaveInstructions(Node* node) {
    if (!node)
        return false;
    switch (node->nodeKind - Feather_getFirstFeatherNodeKind()) {
    case nkRelFeatherNodeList:
    case nkRelFeatherLocalSpace:
        for (Node* n : node->children) {
            n = Nest_explanation(n);
            if (canHaveInstructions(n))
                return true;
        }
        return false;
    case nkRelFeatherExpFunCall: {
        Node* fun = at(node->referredNodes, 0);
        if (!Nest_hasProperty(fun, propEmptyBody))
            return true;
        // Check the args for side-effects
        for (Node* arg : node->children) {
            arg = Nest_explanation(arg);
            if (canHaveInstructions(arg))
                return true;
        }
        return false;
    }
    // These can have side effects if the first child has side effects
    case nkRelFeatherExpBitcast:
    case nkRelFeatherExpMemLoad:
    case nkRelFeatherExpFieldRef: {
        Node* exp = at(node->children, 0);
        exp = Nest_explanation(exp);
        return canHaveInstructions(exp);
    }

    // These cannot have side effects
    case nkRelFeatherNop:
    case nkRelFeatherExpCtValue:
    case nkRelFeatherExpNull:
    case nkRelFeatherExpFunRef:
    case nkRelFeatherExpVarRef:
        return false;

    default:
        return true;
    }
}

void Function_SetContextForChildren(Node* node) {
    // If we don't have a children context, create one
    if (!node->childrenContext)
        node->childrenContext =
                Nest_mkChildContextWithSymTab(node->context, node, Feather_effectiveEvalMode(node));

    Nest_defaultFunSetContextForChildren(node);

    Feather_addToSymTab(node);
}
TypeRef Function_ComputeType(Node* node) {
    if (StringRef(Feather_getName(node)).empty())
        REP_ERROR_RET(nullptr, node->location, "No name given to function declaration");

    // We must have a result type
    Node* resultType = at(node->children, 0);
    TypeRef resType = Nest_computeType(resultType);
    if (!resType)
        REP_ERROR_RET(nullptr, node->location, "No result type given to function %1%") %
                Feather_getName(node);

    Nest_NodeRange params = all(node->children);

    vector<TypeRef> subTypes;
    subTypes.reserve(Nest_nodeRangeSize(params) - 1);
    subTypes.push_back(resType);

    // Get the type for all the parameters
    params.beginPtr += 2;
    for (Node* param : params) {
        if (!param)
            REP_ERROR_RET(nullptr, node->location, "Invalid parameter");
        if (!Nest_computeType(param))
            return nullptr;
        subTypes.push_back(param->type);
    }

    return Feather_getFunctionType(&subTypes[0], subTypes.size(), Feather_effectiveEvalMode(node));
}
Node* Function_SemanticCheck(Node* node) {
    // Make sure the type is computed
    if (!Nest_computeType(node))
        return nullptr;

    // Semantically check all the parameters
    Nest_NodeRange params = all(node->children);
    params.beginPtr += 2;
    for (Node* param : params) {
        if (!Nest_semanticCheck(param))
            return nullptr;
    }

    // Semantically check the body, if we have one
    Node* body = at(node->children, 1);
    if (body)
        Nest_semanticCheck(body); // Ignore possible errors

    // Do we have some valid instructions in the body?
    // Could we mark this function as having an empty body?
    Node* resultType = at(node->children, 0);
    bool noRetValue = !resultType || !resultType->type->hasStorage;
    bool couldInline =
            !Nest_hasProperty(node, propNativeName) || !Nest_hasProperty(node, propNoInline);
    if (noRetValue && couldInline && body && !canHaveInstructions(body)) {
        Nest_setPropertyInt(node, propEmptyBody, 1);
    }

    // TODO (function): Check that all the paths return a value
    return node;
}
const char* Function_toString(Node* node) {
    ostringstream os;
    os << Feather_getName(node);
    if (node->type) {
        os << '(';
        bool hasResultParam = Nest_hasProperty(node, propResultParam);
        unsigned startIdx = hasResultParam ? 3 : 2;
        for (unsigned i = startIdx; i < Nest_nodeArraySize(node->children); ++i) {
            if (i > startIdx)
                os << ", ";
            os << at(node->children, i)->type;
        }
        os << "): "
           << (hasResultParam ? removeRef(TypeWithStorage(at(node->children, 2)->type)).type_
                              : at(node->children, 0)->type);
    }
    return dupString(os.str().c_str());
}

void Class_SetContextForChildren(Node* node) {
    // If we don't have a children context, create one
    if (!node->childrenContext)
        node->childrenContext =
                Nest_mkChildContextWithSymTab(node->context, node, Feather_effectiveEvalMode(node));

    Nest_defaultFunSetContextForChildren(node);

    Feather_addToSymTab(node);
}
TypeRef Class_ComputeType(Node* node) {
    if (StringRef(Feather_getName(node)).empty())
        REP_ERROR_RET(nullptr, node->location, "No name given to class");

    // Compute the type for all the fields
    for (Node* field : node->children) {
        // Ignore errors from children
        Nest_computeType(field);
    }
    return Feather_getDataType(node, 0, Feather_effectiveEvalMode(node));
}
Node* Class_SemanticCheck(Node* node) {
    if (!Nest_computeType(node))
        return nullptr;

    // Semantically check all the fields
    for (Node* field : node->children) {
        Nest_semanticCheck(field); // Ignore possible errors
    }
    return node;
}

void Var_SetContextForChildren(Node* node) {
    Nest_defaultFunSetContextForChildren(node);
    Feather_addToSymTab(node);
}
TypeRef Var_ComputeType(Node* node) {
    // Make sure the variable has a type
    ASSERT(Nest_nodeArraySize(node->children) == 1);
    Node* typeNode = at(node->children, 0);
    if (!Nest_computeType(typeNode))
        return nullptr;

    // Adjust the mode of the type
    return Feather_adjustMode(typeNode->type, node->context, node->location);
}
Node* Var_SemanticCheck(Node* node) {
    if (!Nest_computeType(node))
        return nullptr;

    // Make sure that the type has storage
    if (!node->type->hasStorage)
        REP_ERROR_RET(nullptr, node->location, "Variable type has no storage (%1%)") % node->type;

    auto datatypeDecl = Feather_classForType(node->type);
    if (!datatypeDecl)
        REP_ERROR_RET(nullptr, node->location, "Invalid type for variable: %1%") % node->type;

    Nest_computeType(datatypeDecl); // Make sure the type of the decl is computed
    return node;
}

Node* CtValue_SemanticCheck(Node* node) {
    // Check the type
    if (!node->type)
        node->type = Nest_getCheckPropertyType(node, "valueType");
    if (!node->type || !node->type->hasStorage)
        REP_ERROR_RET(nullptr, node->location,
                "Type specified for Ct Value cannot be used at compile-time (%1%)") %
                node->type;

    // Make sure data size matches the size reported by the type
    unsigned valueSize = Nest_sizeOf(node->type);
    StringRef data = Nest_getCheckPropertyString(node, "valueData");
    if (valueSize != data.size()) {
        REP_ERROR_RET(nullptr, node->location,
                "Read value size (%1%) differs from declared size of the value (%2%) - type: %3%") %
                data.size() % valueSize % node->type;
    }

    node->type = TypeBase(node->type).changeMode(modeCt, node->location);
    return node;
}
void writeHex(ostringstream& os, StringRef data) {
    os << "0x" << hex;
    for (const char* p = data.begin; p != data.end; p++) {
        unsigned char uc = *p;
        if (uc < 16)
            os << '0';
        os << (int)uc;
    }
    os << dec;
}

template <typename T> T extractValue(StringRef valueData) {
    ASSERT(size(valueData) == sizeof(T));
    // NOLINTNEXTLINE
    return *reinterpret_cast<const T*>(valueData.begin);
}

const char* CtValue_toString(Node* node) {
    TypeRef type = node->type;
    if (!node->type)
        type = Nest_getCheckPropertyType(node, "valueType");
    if (!type)
        return "ctValue";
    ostringstream os;
    os << "ctValue(" << type << ": ";

    StringRef valueDataStr = Nest_getCheckPropertyString(node, "valueData");
    // const void* valueData = valueDataStr.begin;

    StringRef nativeName =
            type->hasStorage ? Feather_nativeName(type) : StringRef{nullptr, nullptr};
    if (0 == strcmp(type->description, "Type/ct")) {
        auto t = extractValue<TypeRef>(valueDataStr);
        os << t;
    } else if (nativeName && type->numReferences == 0) {
        if (nativeName == "i1" || nativeName == "u1") {
            bool val = 0 != extractValue<uint8_t>(valueDataStr);
            os << (val ? "true" : "false");
        } else if (nativeName == "i16")
            os << extractValue<int16_t>(valueDataStr);
        else if (nativeName == "u16")
            os << extractValue<uint16_t>(valueDataStr);
        else if (nativeName == "i32")
            os << extractValue<int32_t>(valueDataStr);
        else if (nativeName == "u32")
            os << extractValue<uint32_t>(valueDataStr);
        else if (nativeName == "i64")
            os << extractValue<int64_t>(valueDataStr);
        else if (nativeName == "u64")
            os << extractValue<uint64_t>(valueDataStr);
        else if (nativeName == "float")
            os << extractValue<float>(valueDataStr);
        else if (nativeName == "double")
            os << extractValue<double>(valueDataStr);
        else if (nativeName == "StringRef") {
            os << "'" << extractValue<StringRef>(valueDataStr) << "'";
        } else
            writeHex(os, valueDataStr);
    } else
        writeHex(os, valueDataStr);
    os << ")";
    return dupString(os.str().c_str());
}

Node* Null_SemanticCheck(Node* node) {
    ASSERT(Nest_nodeArraySize(node->children) == 1);
    Node* typeNode = at(node->children, 0);
    TypeRef t = Nest_computeType(typeNode);
    if (!t)
        return nullptr;

    // Make sure that the type is a reference
    if (!t->hasStorage)
        REP_ERROR_RET(nullptr, node->location,
                "Null node should have a type with storage (cur type: %1%") %
                t;
    if (t->numReferences == 0)
        REP_ERROR_RET(
                nullptr, node->location, "Null node should have a reference type (cur type: %1%)") %
                t;

    node->type = Feather_adjustMode(t, node->context, node->location);
    return node;
}
const char* Null_toString(Node* node) {
    ostringstream os;
    os << "null(" << node->type << ")";
    return dupString(os.str().c_str());
}

Node* VarRef_SemanticCheck(Node* node) {
    Node* var = at(node->referredNodes, 0);
    ASSERT(var);
    if (var->nodeKind != nkFeatherDeclVar)
        REP_INTERNAL(node->location, "VarRef object needs to point to a Field (node kind: %1%)") %
                NodeHandle(var).kindName();
    if (!Nest_computeType(var))
        return nullptr;
    if (isField(var))
        REP_INTERNAL(node->location, "VarRef used on a field (%1%). Use FieldRef instead") %
                Feather_getName(var);
    if (!var->type->hasStorage)
        REP_ERROR_RET(
                nullptr, node->location, "Variable type doesn't have a storage type (type: %1%)") %
                var->type;
    node->type =
            Feather_adjustMode(Feather_getLValueType(var->type), node->context, node->location);
    Feather_checkEvalModeWithExpected(node, var->type->mode);
    return node;
}
const char* VarRef_toString(Node* node) {
    Node* var = at(node->referredNodes, 0);
    ostringstream os;
    os << "varRef(" << Feather_getName(var) << ")";
    return dupString(os.str().c_str());
}

Node* FieldRef_SemanticCheck(Node* node) {
    Node* obj = at(node->children, 0);
    Node* field = at(node->referredNodes, 0);
    ASSERT(obj);
    ASSERT(field);
    if (field->nodeKind != nkFeatherDeclVar)
        REP_INTERNAL(node->location, "FieldRef object needs to point to a Field (node kind: %1%)") %
                NodeHandle(field).kindName();

    // Semantic check the object node - make sure it's a reference to a data type
    if (!Nest_semanticCheck(obj))
        return nullptr;
    ASSERT(obj->type);
    if (!obj->type || !obj->type->hasStorage || obj->type->numReferences != 1)
        REP_ERROR_RET(nullptr, node->location,
                "Field access should be done on a reference to a data type (type: %1%)") %
                obj->type;
    Node* cls = Feather_classForType(obj->type);
    ASSERT(cls);
    if (!Nest_computeType(cls))
        return nullptr;

    // Compute the type of the field
    if (!Nest_computeType(field))
        return nullptr;

    // Make sure that the type of a object is a data type that refers to a class the contains the
    // given field
    bool fieldFound = false;
    for (auto field : cls->children) {
        if (&*field == field) {
            fieldFound = true;
            break;
        }
    }
    if (!fieldFound)
        REP_ERROR_RET(nullptr, node->location, "Field '%1%' not found when accessing object") %
                Feather_getName(field);

    // Set the correct type for this node
    ASSERT(field->type);
    ASSERT(field->type->hasStorage);
    node->type = Feather_getLValueType(field->type);
    EvalMode mode = Feather_combineMode(obj->type->mode, node->context->evalMode);
    node->type = TypeBase(node->type).changeMode(mode, node->location);
    return node;
}
const char* FieldRef_toString(Node* node) {
    Node* obj = at(node->children, 0);
    Node* field = at(node->referredNodes, 0);
    ostringstream os;
    os << "fieldRef(" << obj << ", " << field << ")";
    return dupString(os.str().c_str());
}

Node* FunRef_SemanticCheck(Node* node) {
    ASSERT(Nest_nodeArraySize(node->children) == 1);
    Node* resType = at(node->children, 0);
    Node* fun = at(node->referredNodes, 0);
    if (!Nest_computeType(resType))
        return nullptr;

    if (!Nest_computeType(fun))
        return nullptr;
    node->type = Feather_adjustMode(resType->type, node->context, node->location);
    return node;
}
const char* FunRef_toString(Node* node) {
    Node* fun = at(node->referredNodes, 0);
    ostringstream os;
    os << "FunRef(" << fun << ")";
    return dupString(os.str().c_str());
}

Node* FunCall_SemanticCheck(Node* node) {
    Node* fun = at(node->referredNodes, 0);

    // Make sure the function declaration is has a valid type
    if (!Nest_computeType(fun))
        return nullptr;

    // Check argument count
    unsigned numParameters = Feather_Function_numParameters(fun);
    if (Nest_nodeArraySize(node->children) != numParameters)
        REP_ERROR_RET(nullptr, node->location,
                "Invalid function call: expecting %1% parameters, given %2%") %
                numParameters % Nest_nodeArraySize(node->children);

    // Semantic check the arguments
    // Also check that their type matches the corresponding type from the function decl
    bool allParamsAreCtAvailable = true;
    for (unsigned i = 0; i < Nest_nodeArraySize(node->children); ++i) {
        Node* arg = at(node->children, i);

        // Semantically check the argument
        if (!Nest_semanticCheck(arg))
            return nullptr;
        if (!Feather_isCt(arg))
            allParamsAreCtAvailable = false;

        // Compare types
        TypeRef argType = arg->type;
        TypeRef paramType = Feather_Function_getParameter(fun, i)->type;
        if (!Feather::sameTypeIgnoreMode(argType, paramType))
            REP_ERROR_RET(nullptr, arg->location,
                    "Invalid function call: argument %1% is expected to have type %2% (actual "
                    "type: %3%)") %
                    (i + 1) % paramType % argType;
    }

    // CT availability checks
    EvalMode curMode = node->context->evalMode;
    EvalMode calledFunMode = Feather_effectiveEvalMode(fun);
    ASSERT(curMode != modeUnspecified);
    ASSERT(calledFunMode != modeUnspecified);
    if (calledFunMode == modeCt && curMode != modeCt && !allParamsAreCtAvailable) {
        REP_ERROR(node->location,
                "Not all arguments are compile-time, when calling a compile time function");
        REP_INFO(fun->location, "See called function");
        return nullptr;
    }

    // Get the type from the function decl
    node->type = Feather_Function_resultType(fun);

    // Handle autoCt case
    if (allParamsAreCtAvailable && node->type->mode == modeRt &&
            Nest_hasProperty(fun, propAutoCt)) {
        node->type = TypeBase(node->type).changeMode(modeCt, node->location);
    }

    // Make sure we yield a type with the right mode
    node->type = Feather_adjustMode(node->type, node->context, node->location);

    // Check if this node can have some meaningful instructions generated for it
    bool noRetValue = !node->type->hasStorage;
    if (noRetValue && !canHaveInstructions(node)) {
        Nest_setPropertyInt(node, propEmptyBody, 1);
    }

    Feather_checkEvalModeWithExpected(node, calledFunMode);
    return node;
}
const char* FunCall_toString(Node* node) {
    ostringstream os;
    os << "funCall-" << Feather_getName(at(node->referredNodes, 0)) << "(";
    for (unsigned i = 0; i < Nest_nodeArraySize(node->children); ++i) {
        if (i != 0)
            os << ", ";
        os << at(node->children, i);
    }
    os << ")";
    return dupString(os.str().c_str());
}

Node* MemLoad_SemanticCheck(Node* node) {
    Node* exp = at(node->children, 0);

    // Semantic check the argument
    if (!Nest_semanticCheck(exp))
        return nullptr;

    // Check if the type of the argument is a ref
    if (!exp->type->hasStorage || exp->type->numReferences == 0)
        REP_ERROR_RET(
                nullptr, node->location, "Cannot load from a non-reference (%1%, type: %2%)") %
                exp % exp->type;

    // Check flags
    auto ordering = (AtomicOrdering)Nest_getCheckPropertyInt(node, "atomicOrdering");
    if (ordering == atomicRelease)
        REP_ERROR_RET(nullptr, node->location, "Cannot use atomic release with a load instruction");
    if (ordering == atomicAcquireRelease)
        REP_ERROR_RET(nullptr, node->location,
                "Cannot use atomic acquire-release with a load instruction");

    // Remove the 'ref' from the type and get the base type
    node->type = removeRef(TypeWithStorage(exp->type));
    node->type = Feather_adjustMode(node->type, node->context, node->location);
    return node;
}

Node* MemStore_SemanticCheck(Node* node) {
    Node* value = at(node->children, 0);
    Node* address = at(node->children, 1);
    ASSERT(value);
    ASSERT(address);

    // Semantic check the arguments
    if (!Nest_semanticCheck(value) || !Nest_semanticCheck(address))
        return nullptr;

    // Check if the type of the address is a ref
    if (!address->type->hasStorage || address->type->numReferences == 0)
        REP_ERROR_RET(nullptr, node->location,
                "The address of a memory store is not a reference, nor VarRef nor FieldRef (type: "
                "%1%)") %
                address->type;
    TypeRef baseAddressType = removeRef(TypeWithStorage(address->type));

    // Check the equivalence of types
    if (!Feather::sameTypeIgnoreMode(value->type, baseAddressType)) {
        // Try again, getting rid of l-values
        TypeRef t1 = Feather::lvalueToRefIfPresent(value->type);
        if (!Feather::sameTypeIgnoreMode(t1, baseAddressType))
            REP_ERROR_RET(nullptr, node->location,
                    "The type of the value doesn't match the type of the address in a memory store "
                    "(%1% != %2%)") %
                    value->type % baseAddressType;
    }

    // The resulting type is Void
    node->type = Feather_getVoidType(address->type->mode);
    return node;
}

Node* Bitcast_SemanticCheck(Node* node) {
    Node* exp = at(node->children, 0);
    Node* resType = at(node->children, 1);
    if (!Nest_semanticCheck(exp))
        return nullptr;
    TypeRef tDest = Nest_computeType(resType);
    if (!tDest)
        return nullptr;

    // Make sure both types have storage
    TypeRef srcType = exp->type;
    if (!srcType->hasStorage)
        REP_ERROR_RET(nullptr, node->location,
                "The source of a bitcast is not a type with storage (%1%)") %
                srcType;
    if (!tDest->hasStorage)
        REP_ERROR_RET(nullptr, node->location,
                "The destination type of a bitcast is not a type with storage (%1%)") %
                tDest;

    // Make sure both types are references
    if (srcType->numReferences == 0)
        REP_ERROR_RET(nullptr, node->location, "The source of a bitcast is not a reference (%1%)") %
                srcType;
    if (tDest->numReferences == 0)
        REP_ERROR_RET(nullptr, node->location,
                "The destination type of a bitcast is not a reference (%1%)") %
                tDest;

    node->type = Feather_adjustMode(tDest, node->context, node->location);
    return node;
}
const char* Bitcast_toString(Node* node) {
    Node* exp = at(node->children, 0);
    Node* resType = at(node->children, 1);
    ostringstream os;
    if (resType->type)
        os << "bitcast(" << resType->type << ", " << exp << ")";
    else
        os << "bitcast(type(" << resType << "), " << exp << ")";
    return dupString(os.str().c_str());
}

Node* Conditional_SemanticCheck(Node* node) {
    Node*& cond = at(node->children, 0);
    Node* alt1 = at(node->children, 1);
    Node* alt2 = at(node->children, 2);

    // Semantic check the condition
    if (!Nest_semanticCheck(cond))
        return nullptr;

    // Check that the type of the condition is 'Testable'
    if (!Feather_isTestable(cond))
        REP_ERROR_RET(nullptr, cond->location,
                "The condition of the conditional expression is not Testable");

    // Dereference the condition as much as possible
    while (cond->type && cond->type->numReferences > 0) {
        cond = Feather_mkMemLoad(cond->location, cond);
        Nest_setContext(cond, Nest_childrenContext(node));
        if (!Nest_semanticCheck(cond))
            return nullptr;
    }
    ASSERT(cond->type);
    // TODO (conditional): This shouldn't be performed here

    // Semantic check the alternatives
    if (!Nest_semanticCheck(alt1) || !Nest_semanticCheck(alt2))
        return nullptr;

    // Make sure the types of the alternatives are equal
    if (!Feather::sameTypeIgnoreMode(alt1->type, alt2->type))
        REP_ERROR_RET(nullptr, node->location,
                "The types of the alternatives of a conditional must be equal (%1% != %2%)") %
                alt1->type % alt2->type;

    EvalMode mode = Feather_combineModeBottom(alt1->type->mode, cond->type->mode);
    mode = Feather_combineMode(mode, node->context->evalMode);
    node->type = TypeBase(alt1->type).changeMode(mode, node->location);
    return node;
}

void If_SetContextForChildren(Node* node) {
    Node* condition = at(node->children, 0);
    Node* thenClause = at(node->children, 1);
    Node* elseClause = at(node->children, 2);

    node->childrenContext = Nest_mkChildContextWithSymTab(node->context, node, modeUnspecified);

    Nest_setContext(condition, node->childrenContext);
    if (thenClause)
        Nest_setContext(thenClause, node->childrenContext);
    if (elseClause)
        Nest_setContext(elseClause, node->childrenContext);
}
Node* If_SemanticCheck(Node* node) {
    Node* condition = at(node->children, 0);
    Node* thenClause = at(node->children, 1);
    Node* elseClause = at(node->children, 2);

    // The resulting type is Void
    node->type = Feather_getVoidType(node->context->evalMode);

    // Semantic check the condition
    if (!Nest_semanticCheck(condition))
        return nullptr;

    // Check that the type of the condition is 'Testable'
    if (!Feather_isTestable(condition))
        REP_ERROR_RET(
                nullptr, condition->location, "The condition of the if is not Testable; type=%1%") %
                condition->type;

    // Dereference the condition as much as possible
    while (condition->type && condition->type->numReferences > 0) {
        condition = Feather_mkMemLoad(condition->location, condition);
        Nest_setContext(condition, Nest_childrenContext(node));
        if (!Nest_semanticCheck(condition))
            return nullptr;
    }
    at(node->children, 0) = condition;
    // TODO (if): Remove this dereference from here

    if (Feather_nodeEvalMode(node) == modeCt) {
        if (!Feather_isCt(condition))
            REP_ERROR_RET(nullptr, condition->location,
                    "The condition of the ct if should be available at compile-time (%1%)") %
                    condition->type;

        // Get the CT value from the condition, and select an active branch
        Node* c = Nest_ctEval(condition);
        Node* selectedBranch = _getBoolCtValue(c) ? thenClause : elseClause;

        // Expand only the selected branch
        if (selectedBranch)
            return selectedBranch;
        else
            return Feather_mkNop(node->location);
    }

    // Semantic check the clauses
    if (thenClause && !Nest_semanticCheck(thenClause))
        return nullptr;
    if (elseClause && !Nest_semanticCheck(elseClause))
        return nullptr;
    return node;
}

void While_SetContextForChildren(Node* node) {
    Node* condition = at(node->children, 0);
    Node* step = at(node->children, 1);
    Node* body = at(node->children, 2);

    node->childrenContext = Nest_mkChildContextWithSymTab(node->context, node, modeUnspecified);
    CompilationContext* condContext = Feather_nodeEvalMode(node) == modeCt
                                              ? Nest_mkChildContext(node->context, modeCt)
                                              : node->childrenContext;

    Nest_setContext(condition, condContext); // condition
    if (step)
        Nest_setContext(step, condContext); // step
    if (body)
        Nest_setContext(body, node->childrenContext); // body
    // Nest_defaultFunSetContextForChildren(node);
}
Node* While_SemanticCheck(Node* node) {
    Node* condition = at(node->children, 0);
    Node* step = at(node->children, 1);
    Node* body = at(node->children, 2);

    // Semantic check the condition
    if (!Nest_semanticCheck(condition))
        return nullptr;
    if (step && !Nest_computeType(step))
        return nullptr;

    // Check that the type of the condition is 'Testable'
    if (!Feather_isTestable(condition))
        REP_ERROR_RET(nullptr, condition->location, "The condition of the while is not Testable");

    // Dereference the condition as much as possible
    while (condition->type && condition->type->numReferences > 0) {
        condition = Feather_mkMemLoad(condition->location, condition);
        Nest_setContext(condition, Nest_childrenContext(node));
        if (!Nest_semanticCheck(condition))
            return nullptr;
    }
    at(node->children, 0) = condition;
    // TODO (while): Remove this dereference from here

    if (Feather_nodeEvalMode(node) == modeCt) {
        if (!Feather_isCt(condition))
            REP_ERROR_RET(nullptr, condition->location,
                    "The condition of the ct while should be available at compile-time (%1%)") %
                    condition->type;
        if (step && !Feather_isCt(step))
            REP_ERROR_RET(nullptr, step->location,
                    "The step of the ct while should be available at compile-time (%1%)") %
                    step->type;

        // Create a node-list that will be our explanation
        auto result = Nest_allocNodeArray(0);

        // Do the while
        while (true) {
            // CT-evaluate the condition; if the condition evaluates to false, exit the while
            if (!_getBoolCtValue(Nest_ctEval(condition)))
                break;

            // Put (a copy of) the body in the resulting node-list
            if (body) {
                Node* curBody = Nest_cloneNode(body);
                Nest_setContext(curBody, node->context);
                if (!Nest_semanticCheck(curBody))
                    return nullptr;
                Nest_appendNodeToArray(&result, curBody);
            }

            // If we have a step, make sure to evaluate it
            if (step) {
                Nest_ctEval(step); // We don't need the actual result
            }

            // Unfortunately, we don't treat 'break' and 'continue' instructions inside the ct while
            // instructions
        }
        Nest_appendNodeToArray(
                &result, Feather_mkNop(node->location)); // Make sure our resulting type is Void

        // Set the explanation and exit
        Node* res = Feather_mkNodeList(node->location, all(result));
        Nest_freeNodeArray(result);
        return res;
    }

    // Semantic check the body and the step
    if (body && !Nest_semanticCheck(body))
        return nullptr;
    if (step && !Nest_semanticCheck(step))
        return nullptr;

    // The resulting type is Void
    node->type = Feather_getVoidType(node->context->evalMode);
    return node;
}

Node* Break_SemanticCheck(Node* node) {
    // Get the outer-most loop from the context
    Node* loop = Feather_getParentLoop(node->context);
    if (!loop)
        REP_ERROR_RET(nullptr, node->location, "Break found outside any loop");
    Nest_setPropertyNode(node, "loop", loop);

    // The resulting type is Void
    node->type = Feather_getVoidType(node->context->evalMode);
    return node;
}

Node* Continue_SemanticCheck(Node* node) {
    // Get the outer-most loop from the context
    Node* loop = Feather_getParentLoop(node->context);
    if (!loop)
        REP_ERROR_RET(nullptr, node->location, "Continue found outside any loop");
    Nest_setPropertyNode(node, "loop", loop);

    // The resulting type is Void
    node->type = Feather_getVoidType(node->context->evalMode);
    return node;
}

Node* Return_SemanticCheck(Node* node) {
    Node* exp = at(node->children, 0);

    // If we have an expression argument, semantically check it
    if (exp && !Nest_semanticCheck(exp))
        return nullptr;

    // Get the parent function of this return
    Node* parentFun = Feather_getParentFun(node->context);
    if (!parentFun)
        REP_ERROR_RET(nullptr, node->location, "Return found outside any function");
    TypeRef resultType = Feather_Function_resultType(parentFun);
    ASSERT(resultType);
    Nest_setPropertyNode(node, "parentFun", parentFun);

    // If the return has an expression, check that has the same type as the function result type
    if (exp) {
        if (!Feather::sameTypeIgnoreMode(exp->type, resultType))
            REP_ERROR_RET(nullptr, node->location,
                    "Returned expression's type is not the same as function's return type");
    } else {
        // Make sure that the function has a void return type
        if (resultType->typeKind != typeKindVoid)
            REP_ERROR_RET(nullptr, node->location,
                    "You must return something in a function that has non-Void result type");
    }

    // The resulting type is Void
    node->type = Feather_getVoidType(node->context->evalMode);
    return node;
}

// }

int _firstFeatherNodeKind = 0;

int nkFeatherNop = 0;
int nkFeatherTypeNode = 0;
int nkFeatherBackendCode = 0;
int nkFeatherNodeList = 0;
int nkFeatherLocalSpace = 0;
int nkFeatherGlobalConstructAction = 0;
int nkFeatherGlobalDestructAction = 0;
int nkFeatherScopeDestructAction = 0;
int nkFeatherTempDestructAction = 0;
int nkFeatherChangeMode = 0;

int nkFeatherDeclFunction = 0;
int nkFeatherDeclClass = 0;
int nkFeatherDeclVar = 0;

int nkFeatherExpCtValue = 0;
int nkFeatherExpNull = 0;
int nkFeatherExpVarRef = 0;
int nkFeatherExpFieldRef = 0;
int nkFeatherExpFunRef = 0;
int nkFeatherExpFunCall = 0;
int nkFeatherExpMemLoad = 0;
int nkFeatherExpMemStore = 0;
int nkFeatherExpBitcast = 0;
int nkFeatherExpConditional = 0;

int nkFeatherStmtIf = 0;
int nkFeatherStmtWhile = 0;
int nkFeatherStmtBreak = 0;
int nkFeatherStmtContinue = 0;
int nkFeatherStmtReturn = 0;

void initFeatherNodeKinds() {
    nkFeatherNop = Nest_registerNodeKind("nop", &Nop_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherTypeNode = Nest_registerNodeKind(
            "typeNode", &TypeNode_SemanticCheck, nullptr, nullptr, &TypeNode_toString);
    nkFeatherBackendCode = Nest_registerNodeKind(
            "backendCode", &BackendCode_SemanticCheck, nullptr, nullptr, &BackendCode_toString);
    nkFeatherNodeList = Nest_registerNodeKind(
            "nodeList", &NodeList_SemanticCheck, &NodeList_ComputeType, nullptr, nullptr);
    nkFeatherLocalSpace = Nest_registerNodeKind("localSpace", &LocalSpace_SemanticCheck,
            &LocalSpace_ComputeType, &LocalSpace_SetContextForChildren, nullptr);
    nkFeatherGlobalConstructAction = Nest_registerNodeKind("globalConstructAction",
            &GlobalConstructAction_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherGlobalDestructAction = Nest_registerNodeKind(
            "globalDestructAction", &GlobalDestructAction_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherScopeDestructAction = Nest_registerNodeKind("scopelDestructAction",
            &ScopeTempDestructAction_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherTempDestructAction = Nest_registerNodeKind("tempDestructAction",
            &ScopeTempDestructAction_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherChangeMode = Nest_registerNodeKind("changeMode", &ChangeMode_SemanticCheck, nullptr,
            &ChangeMode_SetContextForChildren, &ChangeMode_toString);

    nkFeatherDeclFunction = Nest_registerNodeKind("fun", &Function_SemanticCheck,
            &Function_ComputeType, &Function_SetContextForChildren, &Function_toString);
    nkFeatherDeclClass = Nest_registerNodeKind("class", &Class_SemanticCheck, &Class_ComputeType,
            &Class_SetContextForChildren, nullptr);
    nkFeatherDeclVar = Nest_registerNodeKind(
            "var", &Var_SemanticCheck, &Var_ComputeType, &Var_SetContextForChildren, nullptr);

    nkFeatherExpCtValue = Nest_registerNodeKind(
            "ctValue", &CtValue_SemanticCheck, nullptr, nullptr, &CtValue_toString);
    nkFeatherExpNull =
            Nest_registerNodeKind("null", &Null_SemanticCheck, nullptr, nullptr, &Null_toString);
    nkFeatherExpVarRef = Nest_registerNodeKind(
            "varRef", &VarRef_SemanticCheck, nullptr, nullptr, &VarRef_toString);
    nkFeatherExpFieldRef = Nest_registerNodeKind(
            "fieldRef", &FieldRef_SemanticCheck, nullptr, nullptr, &FieldRef_toString);
    nkFeatherExpFunRef = Nest_registerNodeKind(
            "funRef", &FunRef_SemanticCheck, nullptr, nullptr, &FunRef_toString);
    nkFeatherExpFunCall = Nest_registerNodeKind(
            "funCall", &FunCall_SemanticCheck, nullptr, nullptr, &FunCall_toString);
    nkFeatherExpMemLoad =
            Nest_registerNodeKind("memLoad", &MemLoad_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherExpMemStore =
            Nest_registerNodeKind("memStore", &MemStore_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherExpBitcast = Nest_registerNodeKind(
            "bitcast", &Bitcast_SemanticCheck, nullptr, nullptr, &Bitcast_toString);
    nkFeatherExpConditional = Nest_registerNodeKind(
            "conditional", &Conditional_SemanticCheck, nullptr, nullptr, nullptr);

    nkFeatherStmtIf = Nest_registerNodeKind(
            "if", &If_SemanticCheck, nullptr, &If_SetContextForChildren, nullptr);
    nkFeatherStmtWhile = Nest_registerNodeKind(
            "while", &While_SemanticCheck, nullptr, &While_SetContextForChildren, nullptr);
    nkFeatherStmtBreak =
            Nest_registerNodeKind("break", &Break_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherStmtContinue =
            Nest_registerNodeKind("continue", &Continue_SemanticCheck, nullptr, nullptr, nullptr);
    nkFeatherStmtReturn =
            Nest_registerNodeKind("return", &Return_SemanticCheck, nullptr, nullptr, nullptr);

    _firstFeatherNodeKind = nkFeatherNop;
}

int Feather_getFirstFeatherNodeKind() { return _firstFeatherNodeKind; }

Node* Feather_mkNodeList(Location loc, Nest_NodeRange children) {
    return NodeList::create(loc, children);
}
Node* Feather_mkNodeListVoid(Location loc, Nest_NodeRange children) {
    return NodeList::create(loc, children, true);
}
Node* Feather_addToNodeList(Node* prevList, Node* element) {
    return NodeList::append(NodeList(prevList), NodeHandle(element));
}
Node* Feather_appendNodeList(Node* list, Node* newNodes) {
    return NodeList::append(NodeList(list), NodeList(newNodes));
}

Node* Feather_mkNop(Location loc) {
    return Nop::create(loc);
}
Node* Feather_mkTypeNode(Location loc, TypeRef type) {
    return TypeNode::create(loc, type);
}
Node* Feather_mkBackendCode(Location loc, Nest_StringRef code, EvalMode evalMode) {
    return BackendCode::create(loc, code, evalMode);
}
Node* Feather_mkLocalSpace(Location loc, Nest_NodeRange children) {
    return LocalSpace::create(loc, children);
}
Node* Feather_mkGlobalConstructAction(Location loc, Node* action) {
    return GlobalConstructAction::create(loc, action);
}
Node* Feather_mkGlobalDestructAction(Location loc, Node* action) {
    return GlobalDestructAction::create(loc, action);
}
Node* Feather_mkScopeDestructAction(Location loc, Node* action) {
    return ScopeDestructAction::create(loc, action);
}
Node* Feather_mkTempDestructAction(Location loc, Node* action) {
    return TempDestructAction::create(loc, action);
}

Node* Feather_mkFunction(
        Location loc, Nest_StringRef name, Node* resType, Nest_NodeRange params, Node* body) {
    return FunctionDecl::create(loc, name, resType, params, body);
}
Node* Feather_mkClass(Location loc, Nest_StringRef name, Nest_NodeRange fields) {
    return StructDecl::create(loc, name, fields);
}
Node* Feather_mkVar(Location loc, Nest_StringRef name, Node* typeNode) {
    return VarDecl::create(loc, name, typeNode);
}

Node* Feather_mkCtValue(Location loc, TypeRef type, Nest_StringRef data) {
    return CtValueExp::create(loc, type, data);
}
Node* Feather_mkNull(Location loc, Node* typeNode) {
    return NullExp::create(loc, typeNode);
}
Node* Feather_mkVarRef(Location loc, Node* varDecl) {
    return VarRefExp::create(loc, varDecl);
}
Node* Feather_mkFieldRef(Location loc, Node* obj, Node* fieldDecl) {
    return FieldRefExp::create(loc, obj, fieldDecl);
}
Node* Feather_mkFunRef(Location loc, Node* funDecl, Node* resType) {
    return FunRefExp::create(loc, funDecl, resType);
}
Node* Feather_mkFunCall(Location loc, Node* funDecl, Nest_NodeRange args) {
    return FunCallExp::create(loc, funDecl, args);
}
Node* Feather_mkMemLoad(Location loc, Node* exp) {
    return MemLoadExp::create(loc, exp);
}
Node* Feather_mkMemStore(Location loc, Node* value, Node* address) {
    return MemStoreExp::create(loc, value, address);
}
Node* Feather_mkBitcast(Location loc, Node* destType, Node* exp) {
    return BitcastExp::create(loc, destType, exp);
}
Node* Feather_mkConditional(Location loc, Node* condition, Node* alt1, Node* alt2) {
    return ConditionalExp::create(loc, condition, alt1, alt2);
}
Node* Feather_mkChangeMode(Location loc, Node* child, EvalMode mode) {
    return ChangeMode::create(loc, child, mode);
}

Node* Feather_mkIf(Location loc, Node* condition, Node* thenClause, Node* elseClause) {
    return IfStmt::create(loc, condition, thenClause, elseClause);
}
Node* Feather_mkWhile(Location loc, Node* condition, Node* body, Node* step) {
    return WhileStmt::create(loc, condition, body, step);
}
Node* Feather_mkBreak(Location loc) {
    return BreakStmt::create(loc);
}
Node* Feather_mkContinue(Location loc) {
    return ContinueStmt::create(loc);
}
Node* Feather_mkReturn(Location loc, Node* exp) {
    return ReturnStmt::create(loc, exp);
}

Nest_StringRef Feather_BackendCode_getCode(Node* node) {
    ASSERT(node->nodeKind == nkFeatherBackendCode);
    return Nest_getCheckPropertyString(node, propCode);
}
EvalMode Feather_BackendCode_getEvalMode(Node* node) {
    ASSERT(node->nodeKind == nkFeatherBackendCode);
    auto curMode = (EvalMode)Nest_getCheckPropertyInt(node, propEvalMode);
    return curMode != modeUnspecified ? curMode : node->context->evalMode;
}

void Feather_ChangeMode_setChild(Node* node, Node* child) {
    ASSERT(node);
    Nest_nodeSetChildren(node, fromIniList({child}));

    if (node->childrenContext)
        Nest_setContext(child, node->childrenContext);
}
EvalMode Feather_ChangeMode_getEvalMode(Node* node) {
    auto curMode = (EvalMode)Nest_getCheckPropertyInt(node, propEvalMode);
    return curMode != modeUnspecified ? curMode : node->context->evalMode;
}

void Feather_Function_addParameter(Node* node, Node* parameter) {
    if (Nest_explanation(parameter)->nodeKind != nkFeatherDeclVar)
        REP_INTERNAL(parameter->location, "Node %1% must be a parameter") % parameter;

    Nest_appendNodeToArray(&node->children, parameter);
}
void Feather_Function_addParameterFirst(Node* node, Node* parameter) {
    if (Nest_explanation(parameter)->nodeKind != nkFeatherDeclVar)
        REP_INTERNAL(parameter->location, "Node %1% must be a parameter") % parameter;

    Nest_insertNodeIntoArray(&node->children, 2, parameter);
}
void Feather_Function_setResultType(Node* node, Node* resultType) {
    at(node->children, 0) = resultType;
    Nest_setContext(resultType, node->childrenContext);
}
void Feather_Function_setBody(Node* node, Node* body) { at(node->children, 1) = body; }
unsigned Feather_Function_numParameters(Node* node) {
    return Nest_nodeArraySize(node->children) - 2;
}
Node* Feather_Function_getParameter(Node* node, unsigned idx) {
    return at(node->children, idx + 2);
}
Nest_NodeRange Feather_Function_getParameters(Node* node) {
    return Nest_NodeRange{node->children.beginPtr + 2, node->children.endPtr};
}
TypeRef Feather_Function_resultType(Node* node) { return at(node->children, 0)->type; }
Node* Feather_Function_body(Node* node) { return at(node->children, 1); }
CallConvention Feather_Function_callConvention(Node* node) {
    return (CallConvention)Nest_getCheckPropertyInt(node, "callConvention");
}
