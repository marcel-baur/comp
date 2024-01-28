#include "chunk.h"
#include "common.h"
#include "value.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include <stdio.h>

VM vm;

static void reset_stack() {
    vm.stackTop = vm.stack;
}

void initVM() {
    reset_stack();
}
void freeVM() {}

static InterpretResult run() {
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    #define READ_CONSTANT_LONG() (vm.chunk->constants.values[READ_BYTE() | READ_BYTE() << 8 | READ_BYTE() << 16])
    #define BINARY_OP(op) \
        do { \
            double b = pop(); \
            double a = pop(); \
            push(a op b); \
        } while (false)
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
            printf("     ");
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
                printf("[  ");
                print_value(*slot);
                printf("  ]");
            }
            printf("\n");
        #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_NEGATE: 
                push(-pop()); break;
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value constant = READ_CONSTANT_LONG();
                push(constant);
                break;
            }
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBSTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
        }
    }

    #undef READ_BYTE
    #undef READ_CONSTANT
    #undef READ_CONSTANT_LONG
    #undef BINARY_OP
}

InterpretResult interpret_chunk(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}

InterpretResult interpret(const char* source) {
    InterpretResult result;
    Chunk chunk;
    init_chunk(&chunk);
    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERR;
    }
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;
    result = run();

    free_chunk(&chunk);
    return result;
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}
