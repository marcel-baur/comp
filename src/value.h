#ifndef comp_value_h
#define comp_value_h

#include "common.h"

typedef struct Obj Obj;

typedef struct ObjString ObjString;

typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
	VAL_OBJ,
} ValueType;

typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj* obj;
	} as;

} Value;

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

#define BOOL_VAL(value) ((Value) {VAL_BOOL, {.boolean = value}})
#define NIL_VAL(value) ((Value) {VAL_NIL, {.number = 0}}) // @Cleanup: we don't need a parameter
#define OBJ_VAL(value) ((Value) {VAL_OBJ, {.obj = (Obj*)value}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;

void init_value_array(ValueArray* array);

void write_value_array(ValueArray* array, Value value);

void free_value_array(ValueArray* array);

void print_value(Value value);

bool values_equal(Value v1, Value v2);

#endif
