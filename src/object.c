#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"
#include "value.h"

#define ALLOCATE_OBJ(type, objectType) (type*)allocate_object(sizeof(type), objectType)

static Obj* allocate_object(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    object->isMarked = false;
#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif
    return object;
}

static uint32_t hash_string(const char* chars, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619;
    }
    return hash;
}

static ObjString* allocate_string(char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    // printf("Still working here\n"); // @Hack: this seems to stop a fatal error?!
    table_set(&vm.strings,string, NIL_VAL());
    return string;
}

static void print_func(ObjFunction* func) {
    if (func->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", func->name->chars);
}

ObjString* copy_string(const char* chars, int length) {
    // char* heapChars = reallocate(NULL, 0, sizeof(char) * (length + 1));
    char* heapChars = ALLOCATE(char, length + 1);
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocate_string(heapChars, length, hash);
}

ObjUpvalue* new_upvalue(Value* slot) {
    ObjUpvalue* uv = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    uv->location = slot;
    uv->next = NULL;
    uv->closed = NIL_VAL();
    return uv;
}

void print_obj(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_FUNCTION: print_func(AS_FUNCTION(value)); break;
        case OBJ_NATIVE: printf("<native fn>"); break;
        case OBJ_CLOSURE: print_func(AS_CLOSURE(value)->fn); break;
        case OBJ_UPVALUE: printf("upvalue"); break;
    }
}

ObjString* take_string(char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    ObjString* interned = table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocate_string(chars, length, hash);
}

ObjFunction* new_function() {
    ObjFunction* func = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    func->arity = 0;
    func->name = NULL;
    func->upvalueCount = 0;
    init_chunk(&func->chunk);
    return func;
}

ObjNative* new_native(NativeFn fn) {
    printf("Make new native fn...\n");
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->fn = fn;
    printf("Made new native fn!\n");
    return native;
}

ObjClosure* new_closure(ObjFunction* fn) {
    ObjUpvalue** uvs = ALLOCATE(ObjUpvalue*, fn->upvalueCount);
    for (int i = 0; i < fn->upvalueCount; i++) {
        uvs[i] = NULL;
    }
    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->upvalues = uvs;
    closure->upvalueCount = fn->upvalueCount;
    closure->fn = fn;
    return closure;
}
