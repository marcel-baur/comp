#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char* argv[]) {
    initVM();
    Chunk chunk;
    init_chunk(&chunk);
    // int constant = add_constant(&chunk, 1.2);
    // write_chunk(&chunk, OP_CONSTANT, 123);
    // write_chunk(&chunk, constant, 123);
    write_constant(&chunk, 1.2, 123);
    write_constant(&chunk, 1.8, 123);
    write_chunk(&chunk, OP_NEGATE, 123);
    write_chunk(&chunk, OP_RETURN, 124);
    disassemble_chunk(&chunk, "test chunk");
    interpret(&chunk);
    free_chunk(&chunk);
    freeVM();
    return 0;
}
