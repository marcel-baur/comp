#ifndef comp_object_h
#define comp_object_h

#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->fn)
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))

typedef enum {
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_NATIVE,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
} ObjType;

struct Obj {
	ObjType type;
	struct Obj* next;
	bool isMarked;
};

typedef struct ObjFunction {
	Obj obj;
	int arity;
	Chunk chunk;
	int upvalueCount;
	ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
	Obj obj;
	NativeFn fn;
} ObjNative;

struct ObjString {
	Obj obj;
	int length;
	char* chars;
	uint32_t hash;
};

typedef struct ObjUpvalue {
	Obj obj;
	Value* location;
	Value closed;
	struct ObjUpvalue* next;
} ObjUpvalue;


typedef struct {
	Obj obj;
	ObjFunction* fn;
	ObjUpvalue** upvalues;
	int upvalueCount;
} ObjClosure;

static inline bool is_obj_type(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjString* copy_string(const char* chars, int length);

ObjUpvalue* new_upvalue(Value* slot);

void print_obj(Value value);

ObjString* take_string(char* chars, int length);

ObjFunction* new_function();

ObjNative* new_native(NativeFn fn);

ObjClosure* new_closure(ObjFunction* fn);

#endif // !comp_object_h
