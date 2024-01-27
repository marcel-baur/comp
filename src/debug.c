#include <stdio.h>
#include "chunk.h"
#include "debug.h"
#include "value.h"

static int simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int long_constant_instruction(const char* name, Chunk* chunk, int offset) {
    uint32_t constant = chunk->code[offset + 1] | 
        (chunk->code[offset + 2] << 8) |
        (chunk->code[offset + 3] << 16);
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

int disassemble_instruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%04d ", chunk->lines[offset]);
    }
    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
    case OP_RETURN:
        return simple_instruction("OP_RETURN", offset);
    case OP_CONSTANT:
        return constant_instruction("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG:
        return long_constant_instruction("OP_CONSTANT_LONG", chunk, offset);
    case OP_NEGATE:
        return simple_instruction("OP_NEGATE", offset);
    case OP_ADD:
        return simple_instruction("OP_ADD", offset);
    case OP_SUBSTRACT:
        return simple_instruction("OP_SUBSTRACT", offset);
    case OP_MULTIPLY:
        return simple_instruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return simple_instruction("OP_DIVIDE", offset);
    default:
        printf("Unknown opcode %d\n", instruction);
        return offset + 1;

    }
}

void disassemble_chunk(Chunk *chunk, const char *name) {
     printf("== %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
}


