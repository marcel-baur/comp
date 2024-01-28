#include "memory.h"
#include "value.h"
#include <stdio.h>

void init_value_array(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void write_value_array(ValueArray *array, Value val) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }
    array->values[array->count] = val;
    array->count++;
}

void free_value_array(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}

void print_value(Value value) {
    switch(value.type) {
        case VAL_BOOL: 
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    }
}

bool values_equal(Value v1, Value v2) {
    if (v1.type != v2.type) return false;
    switch (v1.type) {
        case VAL_BOOL: return AS_BOOL(v1) == AS_BOOL(v2); break;
        case VAL_NUMBER: return AS_NUMBER(v1) == AS_NUMBER(v2); break;
        case VAL_NIL: return true; // @Note: both are nil, thus true
        default: return false;
    }
}
