#ifndef comp_vm_h
#define comp_vm_h

#include "chunk.h"

typedef struct {
	Chunk* chunk;
	uint8_t* ip;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERR,
	INTERPRET_RUNTIME_ERR,
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(Chunk* chunk);

#endif
