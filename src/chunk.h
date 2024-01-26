#ifndef comp_chunk_h
#define comp_chunk_h

#include "common.h"
#include "value.h"
#include <stdint.h>

typedef enum {
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_RETURN,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t* code;
	ValueArray constants;
	int* lines; //@Improve: Currently saves all lines. Use RunLengthEncoding to compress it a bit
} Chunk;

void init_chunk(Chunk* chunk);

void write_chunk(Chunk* chunk, uint8_t byte, int line);

void free_chunk(Chunk* chunk);

int add_constant(Chunk* chunk, Value value);

void write_constant(Chunk* chunk, Value value, int line);

#endif