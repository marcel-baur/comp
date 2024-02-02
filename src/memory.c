#include <stdio.h>
#include <stdlib.h>
#include "chunk.h"
#include "memory.h"
#include "vm.h"
#include "object.h"

static void free_object(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* str = (ObjString*)obj;
            FREE_ARRAY(char, str->chars, str->length + 1);
            FREE(ObjString, obj);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*)obj;
            free_chunk(&func->chunk);
            FREE(ObjFunction, func);
            break;
        }
    }
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        // printf("Reallocating failed! %zu\n", newSize);
        free(pointer);
        return NULL;
    }
    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

void free_objects() {
    Obj* obj = vm.objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        free_object(obj);
        obj = next;
    }
}
