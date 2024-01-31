#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"
#include "debug.h"
#include "compiler.h"
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

VM vm;

static void reset_stack() {
    vm.stackTop = vm.stack;
}

void initVM() {
    reset_stack();
    init_table(&vm.strings);
    init_table(&vm.globals);
    vm.objects = NULL;
}
void freeVM() {
    free_table(&vm.strings);
    free_table(&vm.globals);
    free_objects();
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
}

static bool is_falsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    push(OBJ_VAL(result));
}

static InterpretResult run() {
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
    #define READ_CONSTANT_LONG() (vm.chunk->constants.values[READ_BYTE() | READ_BYTE() << 8 | READ_BYTE() << 16])
    #define READ_STRING() AS_STRING(READ_CONSTANT_LONG())
    #define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
    #define BINARY_OP(value_type, op) \
        do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers. Got %s and %s", peek(0), peek(1)); \
            return INTERPRET_RUNTIME_ERR; \
        } \
            double b = AS_NUMBER(pop()); \
            double a = AS_NUMBER(pop()); \
            push(value_type(a op b)); \
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
            case OP_NEGATE: 
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number, got %d", peek(0));
                    return INTERPRET_RUNTIME_ERR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop()))); break;
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
            case OP_EQ: {
                Value a = pop();
                Value b = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error("Operands must be both numbers or strings, got %s and %s", peek(0).type, peek(1).type);
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            } 
            case OP_PRINT: {
                print_value(pop());
                printf("\n");
                break;
            }
            // case OP_RETURN: return INTERPRET_OK;
            case OP_SUBSTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NIL: push(NIL_VAL()); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_NOT: push(BOOL_VAL(is_falsey(pop()))); break;
            case OP_POP: pop(); break;
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    // @Note: allow this if we do implicit variable declaration
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm.stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm.stack[slot] = peek(0);
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0))) vm.ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm.ip += offset;
                break;
            }
            default: return INTERPRET_OK;
        }
    }

    #undef READ_BYTE
    #undef READ_STRING
    #undef READ_SHORT
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
