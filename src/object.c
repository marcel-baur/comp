#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "vm.h"
#include "value.h"

static Obj* allocate_object(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

#define ALLOCATE_OBJ(type, objectType) (type*)allocate_object(sizeof(type), objectType)

static ObjString* allocate_string(char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* copy_string(const char* chars, int length) {
    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocate_string(heapChars, length);
}

void print_obj(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}

ObjString* take_string(char* chars, int length) {
    return allocate_string(chars, length);
}
