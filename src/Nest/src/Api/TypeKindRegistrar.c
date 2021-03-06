#include "Nest/Api/TypeKindRegistrar.h"
#include "Nest/Api/Type.h"
#include "Nest/Utils/Assert.h"

struct _TypeFunctions {
    FChangeTypeMode changeTypeMode;
};

/// The registered type kinds
struct _TypeFunctions _allTypeKinds[100];
unsigned int _numTypeKinds = 0;

int Nest_registerTypeKind(FChangeTypeMode funChangeTypeMode) {
    int typeKindId = _numTypeKinds++;
    struct _TypeFunctions f = {funChangeTypeMode};
    _allTypeKinds[typeKindId] = f;
    return typeKindId;
}

FChangeTypeMode Nest_getChangeTypeModeFun(int typeKind) {
    ASSERT(0 <= typeKind && typeKind < _numTypeKinds);
    return _allTypeKinds[typeKind].changeTypeMode;
}

void Nest_resetRegisteredTypeKinds() { _numTypeKinds = 0; }