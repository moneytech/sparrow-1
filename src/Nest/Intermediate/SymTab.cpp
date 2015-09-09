#include <StdInc.h>
#include "SymTab.h"

#include "Common/Alloc.h"

#include <boost/bind.hpp>

typedef unordered_multimap<string, Node*> MMap;
typedef MMap::value_type MMapPair;

struct _SymTabImpl {
    SymTab base;
    MMap entries;
    MMap copiedEntries;
};

template<class ITERATOR>
ITERATOR begin( std::pair<ITERATOR,ITERATOR> &range ) { return range.first; }

template<class ITERATOR>
ITERATOR end( std::pair<ITERATOR,ITERATOR> &range ) { return range.second; }

SymTab* Nest_mkSymTab(SymTab* parent, Node* node)
{
    _SymTabImpl* res = (_SymTabImpl*) alloc(sizeof(_SymTabImpl), allocGeneral);
    res->base.parent = parent;
    res->base.node = node;
    new (&res->entries) MMap();
    new (&res->copiedEntries) MMap();
    return (SymTab*) res;
}

void Nest_symTabEnter(SymTab* symTab, const char* name, Node* node)
{
    _SymTabImpl* st = (_SymTabImpl*) symTab;

    // Insert our entry in the normal map
    st->entries.insert(MMapPair(name, node));
    // Remove all the copied entries with the same name
    st->copiedEntries.erase(name);
}

void Nest_symTabCopyEntries(SymTab* symTab, SymTab* otherSymTab)
{
    _SymTabImpl* st = (_SymTabImpl*) symTab;
    _SymTabImpl* other = (_SymTabImpl*) otherSymTab;

    for ( const auto& entry: other->entries )
    {
        // Insert the item into our set of copied entries, only if we don't find it in our normal set
        // This way, all the copied elements are shadowed by the normal entries
        auto it = st->entries.find(entry.first);
        if ( it == st->entries.end() )
        {
            st->copiedEntries.insert(entry);
        }
    }
    for ( const auto& entry: other->copiedEntries )
    {
        // Insert the item into our set of copied entries, only if we don't find it in our normal set
        // This way, all the copied elements are shadowed by the normal entries
        auto it = st->entries.find(entry.first);
        if ( it == st->entries.end() )
        {
            st->copiedEntries.insert(entry);
        }
    }

}

NodeArray Nest_symTabAllEntries(SymTab* symTab)
{
    _SymTabImpl* st = (_SymTabImpl*) symTab;

    NodeArray result = Nest_allocNodeArray(st->entries.size() + st->copiedEntries.size());
    for ( auto entry: st->entries )
        Nest_appendNodeToArray(&result, entry.second);
    for ( auto entry: st->copiedEntries )
        Nest_appendNodeToArray(&result, entry.second);
    return result;
}

NodeArray Nest_symTabLookupCurrent(SymTab* symTab, const char* name)
{
    _SymTabImpl* st = (_SymTabImpl*) symTab;

    auto range = st->entries.equal_range(name);
    NodeArray result = Nest_allocNodeArray(distance(range.first, range.second));
    for ( auto entry: range )
        Nest_appendNodeToArray(&result, entry.second);
    if ( Nest_nodeArraySize(result) == 0 )
    {
        auto range = st->copiedEntries.equal_range(name);
        for ( auto entry: range )
            Nest_appendNodeToArray(&result, entry.second);
    }
    return result;
}

NodeArray Nest_symTabLookup(SymTab* symTab, const char* name)
{
    _SymTabImpl* st = (_SymTabImpl*) symTab;

    NodeArray res = Nest_symTabLookupCurrent(symTab, name);
    return Nest_nodeArraySize(res) > 0 || !st->base.parent ? res : Nest_symTabLookup(st->base.parent, name);
}
