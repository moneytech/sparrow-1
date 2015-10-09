#include <StdInc.h>
#include "Node.h"
#include "NodeKindRegistrar.h"
#include "NodeUtils.hpp"
#include "Modifier.h"
#include <Compiler.h>
#include <Common/Alloc.h>
#include <Common/Diagnostic.hpp>

void _applyModifiers(Node* node, ModifierType modType)
{
    for ( Modifier* mod: node->modifiers )
        if ( mod->modifierType == modType )
            mod->modifierFun(mod, node);
}

/// Set the explanation of this node.
/// makes sure it has the right context, compiles it, and set the type of the current node to be the type of the
/// explanation
bool _setExplanation(Node* node, Node* explanation)
{
    if ( explanation == node->explanation )
        return true;

    node->explanation = explanation;

    if ( explanation == node )
        return true;

    // Copy all the properties marked accordingly
    for ( const auto& prop : node->properties )
        if ( prop.second.passToExpl_ )
            node->explanation->properties[prop.first] = prop.second;

    // Try to semantically check the explanation
    bool res = true;
    if ( !explanation->nodeSemanticallyChecked )
    {
        Nest_setContext(node->explanation, node->context);
        res = Nest_semanticCheck(node->explanation);
    }
    node->type = node->explanation->type;
    return res;
}


Node* Nest_createNode(int nodeKind)
{
    ASSERT(nodeKind >= 0);

    Node* res = (Node*) alloc(sizeof(Node), allocNode);
    res->nodeKind = nodeKind;
    res->nodeError = 0;
    res->nodeSemanticallyChecked = 0;
    res->computeTypeStarted = 0;
    res->semanticCheckStarted = 0;
    res->context = nullptr;
    res->childrenContext = nullptr;
    res->type = nullptr;
    res->explanation = nullptr;
    return res;
}

Node* Nest_cloneNode(Node* node)
{
    if ( !node )
        return NULL;

    ASSERT(node);
    Node* res = Nest_createNode(node->nodeKind);

    res->location = node->location;
    res->referredNodes = node->referredNodes;
    res->properties = node->properties;

    // Clone each node in the children vector
    unsigned size = Nest_nodeArraySize(node->children);
    Nest_resizeNodeArray(&res->children, size);
    for ( size_t i=0; i<size; ++i )
    {
        at(res->children, i) = Nest_cloneNode(at(node->children, i));
    }
    return res;
}

void Nest_initNode(Node* node, int nodeKind)
{
    node->nodeKind = nodeKind;
    node->nodeError = 0;
    node->nodeSemanticallyChecked = 0;
    node->computeTypeStarted = 0;
    node->semanticCheckStarted = 0;
    node->context = nullptr;
    node->childrenContext = nullptr;
    node->type = nullptr;
    node->explanation = nullptr;
}
void Nest_initCopyNode(Node* node, const Node* srcNode)
{
    node->nodeKind = srcNode->nodeKind;
    node->location = srcNode->location;
    node->referredNodes = srcNode->referredNodes;
    node->properties = srcNode->properties;

    // Clone each node in the children vector
    unsigned size = Nest_nodeArraySize(srcNode->children);
    Nest_resizeNodeArray(&node->children, size);
    for ( size_t i=0; i<size; ++i )
    {
        at(node->children, i) = Nest_cloneNode(at(srcNode->children, i));
    }
}


const char* Nest_toString(const Node* node)
{
    return Nest_getToStringFun(node->nodeKind)(node);
}

const char* Nest_nodeKindName(const Node* node)
{
    return Nest_getNodeKindName(node->nodeKind);
}

NodeRange Nest_nodeChildren(Node* node) {
    return all(node->children);
}

Node* Nest_getNodeChild(Node* node, unsigned int index) {
    NodeRange children = Nest_nodeChildren(node);
    ASSERT(index < Nest_nodeRangeSize(children));
    return children.beginPtr[index];
}

NodeRange Nest_nodeReferredNodes(Node* node) {
    return all(node->referredNodes);
}

Node* Nest_getReferredNode(Node* node, unsigned int index) {
    NodeRange refNodes = Nest_nodeReferredNodes(node);
    ASSERT(index < Nest_nodeRangeSize(refNodes));
    return refNodes.beginPtr[index];
}

void Nest_nodeSetChildren(Node* node, NodeRange children) {
    unsigned size = Nest_nodeRangeSize(children);
    Nest_resizeNodeArray(&node->children, size);
    for ( unsigned i=0; i<size; ++i )
        at(node->children, i) = at(children, i);
}

void Nest_nodeAddChild(Node* node, Node* child) {
    Nest_appendNodeToArray(&node->children, child);
}

void Nest_nodeAddChildren(Node* node, NodeRange children) {
    Nest_appendNodesToArray(&node->children, children);
}

void Nest_nodeSetReferredNodes(Node* node, NodeRange nodes) {
    unsigned size = Nest_nodeRangeSize(nodes);
    Nest_resizeNodeArray(&node->referredNodes, size);
    for ( unsigned i=0; i<size; ++i )
        at(node->referredNodes, i) = at(nodes, i);
}




void Nest_setProperty(Node* node, const char* name, int val, bool passToExpl)
{
    node->properties[name] = Property(propInt, PropertyValue(val), passToExpl);
}
void Nest_setProperty(Node* node, const char* name, StringRef val, bool passToExpl)
{
    node->properties[name] = Property(propString, PropertyValue(val), passToExpl);
}
void Nest_setProperty(Node* node, const char* name, Node* val, bool passToExpl)
{
    node->properties[name] = Property(propNode, PropertyValue(val), passToExpl);
}
void Nest_setProperty(Node* node, const char* name, TypeRef val, bool passToExpl)
{
    node->properties[name] = Property(propType, PropertyValue(val), passToExpl);
}

bool Nest_hasProperty(const Node* node, const char* name)
{
    return node->properties.find(name) != node->properties.end();
}
const int* Nest_getPropertyInt(const Node* node, const char* name)
{
    auto it = node->properties.find(name);
    if ( it == node->properties.end() || it->second.kind_ != propInt )
        return nullptr;
    return &it->second.value_.intValue_;
}
const StringRef* Nest_getPropertyString(const Node* node, const char* name)
{
    auto it = node->properties.find(name);
    if ( it == node->properties.end() || it->second.kind_ != propString )
        return nullptr;
    return &it->second.value_.stringValue_;
}
Node*const* Nest_getPropertyNode(const Node* node, const char* name)
{
    auto it = node->properties.find(name);
    if ( it == node->properties.end() || it->second.kind_ != propNode )
        return nullptr;
    return &it->second.value_.nodeValue_;
}
const TypeRef* Nest_getPropertyType(const Node* node, const char* name)
{
    auto it = node->properties.find(name);
    if ( it == node->properties.end() || it->second.kind_ != propType )
        return nullptr;
    return &it->second.value_.typeValue_;
}

int Nest_getCheckPropertyInt(const Node* node, const char* name)
{
    const int* res = Nest_getPropertyInt(node, name);
    if ( !res )
        REP_INTERNAL(node->location, "Node of kind %1% does not have integer property %2%") % Nest_nodeKindName(node) % name;
    return *res;
}
StringRef Nest_getCheckPropertyString(const Node* node, const char* name)
{
    const StringRef* res = Nest_getPropertyString(node, name);
    if ( !res )
        REP_INTERNAL(node->location, "Node of kind %1% does not have string property %2%") % Nest_nodeKindName(node) % name;
    return *res;
}
Node* Nest_getCheckPropertyNode(const Node* node, const char* name)
{
    Node*const* res = Nest_getPropertyNode(node, name);
    if ( !res )
        REP_INTERNAL(node->location, "Node of kind %1% does not have Node property %2%") % Nest_nodeKindName(node) % name;
    return *res;
}
TypeRef Nest_getCheckPropertyType(const Node* node, const char* name)
{
    const TypeRef* res = Nest_getPropertyType(node, name);
    if ( !res )
        REP_INTERNAL(node->location, "Node of kind %1% does not have Type property %2%") % Nest_nodeKindName(node) % name;
    return *res;
}


void Nest_setContext(Node* node, CompilationContext* context)
{
    if ( context == node->context )
        return;
    ASSERT(context);
    node->context = context;

    if ( node->type )
        Nest_clearCompilationState(node);

    _applyModifiers(node, modTypeBeforeSetContext);

    Nest_getSetContextForChildrenFun(node->nodeKind)(node);

    _applyModifiers(node, modTypeAfterSetContext);
}

TypeRef Nest_computeType(Node* node)
{
    if ( node->type )
        return node->type;
    if ( node->nodeError )
        return NULL;

    if ( !node->context )
        REP_INTERNAL(node->location, "No context associated with node (%1%)") % Nest_toString(node);

    // Check for recursive dependency
    if ( node->computeTypeStarted )
        REP_ERROR_RET(nullptr, node->location, "Recursive dependency detected while computing the type of the current node");
    node->computeTypeStarted = 1;

    _applyModifiers(node, modTypeBeforeComputeType);

    // Actually compute the type
    node->type = Nest_getComputeTypeFun(node->nodeKind)(node);
    if ( !node->type )
    {
        node->nodeError = 1;
        return NULL;
    }

    _applyModifiers(node, modTypeAfterComputeType);

    return node->type;
}

Node* Nest_semanticCheck(Node* node)
{
    if ( node->nodeSemanticallyChecked )
        return node->explanation;
    if ( node->nodeError )
        return NULL;

    if ( !node->context )
        REP_INTERNAL(node->location, "No context associated with node (%1%)") % Nest_toString(node);

    // Check for recursive dependency
    if ( node->semanticCheckStarted )
        REP_ERROR_RET(nullptr, node->location, "Recursive dependency detected while semantically checking the current node");
    node->semanticCheckStarted = 1;

    _applyModifiers(node, modTypeBeforeSemanticCheck);

    // Actually do the semantic check
    Node* res = Nest_getSemanticCheckFun(node->nodeKind)(node);
    if ( !res )
    {
        node->nodeError = 1;
        return NULL;
    }
    if ( !_setExplanation(node, res) )
    {
        node->nodeError = 1;
        return NULL;
    }
    if ( !node->type )
        REP_INTERNAL(node->location, "Node semantically checked, but no actual types was generated");
    node->nodeSemanticallyChecked = 1;

    _applyModifiers(node, modTypeAfterSemanticCheck);

    return node->explanation;
}

void Nest_clearCompilationState(Node* node)
{
    node->nodeError = 0;
    node->nodeSemanticallyChecked = 0;
    node->computeTypeStarted = 0;
    node->semanticCheckStarted = 0;
    node->explanation = nullptr;
    node->type = nullptr;
    node->modifiers.clear();

    for ( Node* p: node->children )
    {
        if ( p )
            Nest_clearCompilationState(p);
    }
}

void Nest_addModifier(Node* node, Modifier* mod)
{
    node->modifiers.push_back(mod);
}

CompilationContext* Nest_childrenContext(const Node* node)
{
    return node->childrenContext ? node->childrenContext : node->context;
}

Node* Nest_explanation(Node* node)
{
    return node && node->explanation && node->explanation != node ? Nest_explanation(node->explanation) : node;
}

Node* Nest_ofKind(Node* src, int desiredNodeKind)
{
    return src && src->nodeKind == desiredNodeKind ? src : nullptr;
}


const char* Nest_defaultFunToString(const Node* node)
{
    ostringstream os;
    if ( node->explanation && node != node->explanation )
        os << node->explanation;
    else
    {
        const StringRef* name = Nest_getPropertyString(node, "name");
        if ( name )
            os << name->begin;
        else
        {
            os << Nest_nodeKindName(node) << "(";
            for ( size_t i=0; i<Nest_nodeArraySize(node->children); ++i )
            {
                if ( i > 0 )
                    os << ", ";
                os << at(node->children, i);
            }

            os << ")";
        }
    }
    return dupString(os.str().c_str());
}

void Nest_defaultFunSetContextForChildren(Node* node)
{
    CompilationContext* childrenCtx = Nest_childrenContext(node);
    for ( Node* child: node->children )
    {
        if ( child )
            Nest_setContext(child, childrenCtx);
    }
}

TypeRef Nest_defaultFunComputeType(Node* node)
{
    Nest_semanticCheck(node);
    return node->type;
}

