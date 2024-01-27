#ifndef comp_vm_h
#define comp_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256 // @Improve: grow dynamically

typedef struct {
	Chunk* chunk;
	uint8_t* ip;
	Value stack[STACK_MAX]; // @Improve: grow dynamically
	Value* stackTop;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERR,
	INTERPRET_RUNTIME_ERR,
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret_chunk(Chunk* chunk);
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif
