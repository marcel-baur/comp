#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
    initVM();
    Chunk chunk;
    init_chunk(&chunk);
    for (int idx = 1; idx < 10; idx++) {
        write_constant(&chunk, (Value) idx, idx + 100);
    }
    write_chunk(&chunk, OP_ADD, 111);
    write_chunk(&chunk, OP_SUBSTRACT, 111);
    write_chunk(&chunk, OP_DIVIDE, 112);
    write_chunk(&chunk, OP_MULTIPLY, 112);
    write_chunk(&chunk, OP_NEGATE, 123);
    write_chunk(&chunk, OP_RETURN, 124);
    disassemble_chunk(&chunk, "test chunk");
    interpret(&chunk);
    free_chunk(&chunk);
    freeVM();
    return 0;
}
