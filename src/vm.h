#ifndef comp_vm_h
#define comp_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT) // @Improve: grow dynamically

typedef struct {
	ObjClosure* closure;
	uint8_t* ip;
	Value* slots;
} CallFrame;

typedef struct {
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	Value stack[STACK_MAX]; // @Improve: grow dynamically
	Value* stackTop;
	Table strings;
	Table globals;
	ObjUpvalue* openUpvalues;
	Obj* objects;

	size_t bytesallocated;
	size_t nextgc;

	int grayCapacity;
	int grayCount;
	Obj **grayStack;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERR,
	INTERPRET_RUNTIME_ERR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif
