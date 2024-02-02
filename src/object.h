#ifndef comp_object_h
#define comp_object_h

#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))

typedef enum {
	OBJ_STRING,
	OBJ_FUNCTION,
} ObjType;

struct Obj {
	ObjType type;
	struct Obj* next;
};

typedef struct {
	Obj obj;
	int arity;
	Chunk chunk;
	ObjString* name;
} ObjFunction;

struct ObjString {
	Obj obj;
	int length;
	char* chars;
	uint32_t hash;
};

static inline bool is_obj_type(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

ObjString* copy_string(const char* chars, int length);

void print_obj(Value value);

ObjString* take_string(char* chars, int length);

ObjFunction* new_function();

#endif // !comp_object_h
