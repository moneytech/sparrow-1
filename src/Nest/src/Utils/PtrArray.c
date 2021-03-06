#include "Nest/Utils/PtrArray.h"
#include "Nest/Utils/Assert.h"

#include <memory.h>
#include <stdlib.h>

void _growPtrArray(NestUtils_PtrArray* arr, unsigned minCapacity) {
    unsigned capacity = arr->endOfStorePtr - arr->beginPtr;
    if (capacity == 0)
        capacity = 8; // initial capacity
    while (capacity < minCapacity)
        capacity += (capacity + 1) / 2; // growth factor: 1.5
    NestUtils_reservePtrArray(arr, capacity);
}

NestUtils_PtrArray NestUtils_emptyPtrArray() {
    NestUtils_PtrArray res = {0, 0, 0};
    return res;
}
NestUtils_PtrArray NestUtils_allocPtrArray(unsigned capacity) {
    NestUtils_PtrArray res = {0, 0, 0};
    if (capacity > 0) {
        res.beginPtr = calloc(capacity, sizeof(void*));
        res.endPtr = res.beginPtr;
        res.endOfStorePtr = res.beginPtr + capacity;
    }
    return res;
}

void NestUtils_freePtrArray(NestUtils_PtrArray arr) { free(arr.beginPtr); }

void NestUtils_reservePtrArray(NestUtils_PtrArray* arr, unsigned capacity) {
    if (capacity > (arr->endOfStorePtr - arr->beginPtr)) {
        unsigned curSize = arr->endPtr - arr->beginPtr;
        arr->beginPtr = realloc(arr->beginPtr, capacity * sizeof(void*));
        arr->endPtr = arr->beginPtr + curSize;
        arr->endOfStorePtr = arr->beginPtr + capacity;
    }
}

void NestUtils_resizePtrArray(NestUtils_PtrArray* arr, unsigned size) {
    unsigned curSize = arr->endPtr - arr->beginPtr;
    if (size < curSize) {
        // Simply change the end pointer
        arr->endPtr = arr->beginPtr + size;
    } else if (size > curSize) {
        // Make sure we have enough elements
        _growPtrArray(arr, size);

        // Then change the size
        arr->endPtr = arr->beginPtr + size;
    }
}

void NestUtils_appendObjectToPtrArray(NestUtils_PtrArray* arr, void* obj) {
    unsigned curSize = arr->endPtr - arr->beginPtr;
    _growPtrArray(arr, curSize + 1);
    arr->beginPtr[curSize] = obj;
    arr->endPtr++;
}

void NestUtils_appendObjectsToPtrArray(NestUtils_PtrArray* arr, Nest_PtrRange objects) {
    unsigned numNewObjects = objects.endPtr - objects.beginPtr;
    unsigned curSize = arr->endPtr - arr->beginPtr;
    _growPtrArray(arr, curSize + numNewObjects);
    memcpy(arr->endPtr, objects.beginPtr, sizeof(void*) * numNewObjects);
    arr->endPtr += numNewObjects;
}

void NestUtils_insertObjectIntoPtrArray(NestUtils_PtrArray* arr, unsigned index, void* obj) {
    unsigned curSize = arr->endPtr - arr->beginPtr;
    ASSERT(index <= curSize);
    _growPtrArray(arr, curSize + 1);
    memmove(arr->beginPtr + index + 1, arr->beginPtr + index, sizeof(void*) * (curSize - index));
    arr->beginPtr[index] = obj;
    arr->endPtr++;
}
void NestUtils_insertObjectsIntoPtrArray(
        NestUtils_PtrArray* arr, unsigned index, Nest_PtrRange objects) {
    unsigned numNewObjects = objects.endPtr - objects.beginPtr;
    unsigned curSize = arr->endPtr - arr->beginPtr;
    ASSERT(index <= curSize);
    _growPtrArray(arr, curSize + numNewObjects);
    memmove(arr->beginPtr + index + numNewObjects, arr->beginPtr + index,
            sizeof(void*) * (curSize - index));
    memcpy(arr->beginPtr + index, objects.beginPtr, sizeof(void*) * numNewObjects);
    arr->endPtr += numNewObjects;
}

void NestUtils_eraseFromPtrArray(NestUtils_PtrArray* arr, unsigned index) {
    unsigned curSize = arr->endPtr - arr->beginPtr;
    ASSERT(index <= curSize);
    memmove(arr->beginPtr + index, arr->beginPtr + index + 1,
            sizeof(void*) * (curSize - index - 1));
    arr->endPtr--;
}
