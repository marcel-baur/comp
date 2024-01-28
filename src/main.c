#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf(">> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(line);
    }
}

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read file \"%s\".\n", path);
        exit(74);
    }
    // @Improve: read the entire file, RAM should be enough for most cases;
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

static void run_file(const char* path) {
    char *source = read_file(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERR) exit(65);
    if (result == INTERPRET_RUNTIME_ERR) exit(70);
}

int main(int argc, const char* argv[]) {
    initVM();
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: comp [path]\n");
        exit(64);
    }
    freeVM();
    return 0;
}

void test_stack() {
    Chunk chunk;
    init_chunk(&chunk);
    for (int idx = 1; idx < 10; idx++) {
        // write_constant(&chunk, AS_NUMBER(1.2), idx + 100);
    }
    write_chunk(&chunk, OP_ADD, 111);
    write_chunk(&chunk, OP_SUBSTRACT, 111);
    write_chunk(&chunk, OP_DIVIDE, 112);
    write_chunk(&chunk, OP_MULTIPLY, 112);
    write_chunk(&chunk, OP_NEGATE, 123);
    write_chunk(&chunk, OP_RETURN, 124);
    disassemble_chunk(&chunk, "test chunk");
    interpret_chunk(&chunk);
    free_chunk(&chunk);
}
