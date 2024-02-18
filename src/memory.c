#include <stdio.h>
#include <stdlib.h>
#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"
#include "object.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

static void free_object(Obj* obj) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)obj, obj->type);
#endif
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
        case OBJ_NATIVE: {
            FREE(ObjNative, obj);
            break;
        }
        case OBJ_CLOSURE: {
            FREE(ObjClosure, obj);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, obj);
            break;
        }
    }
}

static void mark_roots() {
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
        mark_value(*slot);
    }
    for (int i = 0; i < vm.frameCount; i++) {
        mark_object((Obj*)vm.frames[i].closure);
    }
    for (ObjUpvalue* uv = vm.openUpvalues; uv != NULL; uv = uv->next) {
        mark_object((Obj*)uv);
    }
    mark_table(&vm.globals);
    mark_compiler_roots();
}

static void mark_array(ValueArray *array) {
    for (int i = 0; i < array->count; i++) {
        mark_value(array->values[i]);
    }
}

static void blacken_object(Obj *object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_UPVALUE: {
            mark_value(((ObjUpvalue*)object)->closed);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* fun = (ObjFunction*)object;
            mark_object((Obj*)fun->name);
            mark_array(&fun->chunk.constants);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure*)object;
            mark_object((Obj*)closure->fn);
            for (int i = 0; i < closure->upvalueCount; i++) {
                mark_object((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void trace_references() {
    while (vm.grayCount > 0) {
        Obj *object = vm.grayStack[--vm.grayCount];
        blacken_object(object);
    }
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif
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

    free(vm.grayStack);
}

void mark_object(Obj *object) {
    if (object == NULL) return;
    if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;

    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if (vm.grayStack == NULL) exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void mark_value(Value value) {
    if (IS_OBJ(value)) mark_object(AS_OBJ(value));
}

void collect_garbage() {
    #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    #endif
    mark_roots();
    trace_references();
}
