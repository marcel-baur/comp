#include <stdlib.h>
#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

void init_chunk(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    init_value_array(&chunk->constants);
}

void write_chunk(Chunk *chunk, uint8_t byte, int line){
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void free_chunk(Chunk *chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    free_value_array(&chunk->constants);
    init_chunk(chunk);
}

int add_constant(Chunk *chunk, Value value) {
    push(value);
    write_value_array(&chunk->constants, value);
    pop();
    return chunk->constants.count - 1;
}

int add_constant_generic(Chunk *chunk, Value value) {
    int idx = add_constant(chunk, value);
    if (idx < 256) {
        return idx;
    }
    return idx;
}

void write_constant(Chunk *chunk, Value value, int line) {
    int idx = add_constant(chunk, value);
    write_chunk(chunk, OP_CONSTANT_LONG, line);
    write_chunk(chunk, (uint8_t) (idx & 0xff), line);
    write_chunk(chunk, (uint8_t) ((idx >> 8) & 0xff), line);
    write_chunk(chunk, (uint8_t) ((idx >> 16) & 0xff), line);
}
