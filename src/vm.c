#include "chunk.h"
#include "common.h"
#include "value.h"
#include "vm.h"
#include "debug.h"
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
            // TODO: OP_CONSTANT_LONG
        }
    }

    #undef READ_BYTE
}

InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}
