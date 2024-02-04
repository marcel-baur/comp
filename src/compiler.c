#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif
#include "scanner.h"
#include "object.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGN,
    PREC_OR,
    PREC_AND,
    PREC_EQ,
    PREC_COMPARE,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    Upvalue upvalues[UINT8_COUNT];
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* current_chunk() {
    return &current->function->chunk;
}

static void error_at(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

static void error(const char* message) {
    error_at(&parser.previous, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }

}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit_byte(uint8_t byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_constant_bytes(int idx) {
    // if (idx < 256) {
    //     emit_byte(OP_CONSTANT);
    //     emit_byte((uint8_t) idx);
    // } else {
    emit_byte(OP_CONSTANT_LONG);
    emit_byte((uint8_t) (idx & 0xff));
    emit_byte((uint8_t) ((idx >> 8) & 0xff));
    emit_byte((uint8_t) ((idx >> 16) & 0xff));
    // }
}

static void emit_bytes_by_opcode(OpCode operation, int content) {
    emit_byte(operation);
    emit_byte((uint8_t) (content & 0xff));
    emit_byte((uint8_t) ((content >> 8) & 0xff));
    emit_byte((uint8_t) ((content >> 16) & 0xff));
}

static void emit_bytes(uint8_t b1, uint8_t b2) {
    emit_byte(b1);
    emit_byte(b2);
}

static void emit_long_bytes(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    emit_byte(b1);
    emit_byte(b2);
    emit_byte(b3);
    emit_byte(b4);
}

static void emit_loop(int loopStart) {
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body to long.");
    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static void emit_return() {
    emit_byte(OP_NIL);
    emit_byte(OP_RETURN);
}

static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL; // @Note: dont generate garbage
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = new_function();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copy_string(parser.previous.start, parser.previous.length);
    }
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static ObjFunction* end_compiler() {
    emit_return();
    ObjFunction* func = current->function;
    #ifdef DEBUG_PRINT_CODE
        if (!parser.hadError) {
            disassemble_chunk(current_chunk(), func->name != NULL ? func->name->chars : "<script>");
        }
    #endif
    current = current->enclosing;
    return func;
}

static void begin_scope() {
    current->scopeDepth++;
}

static void end_scope() {
    current->scopeDepth--;
    while (current->localCount > 0 && current->locals[current->localCount -1].depth > current->scopeDepth) {
        emit_byte(OP_POP);
        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static int make_constant(Value value);
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static int emit_jump(uint8_t instruction);

static void patch_jump(int offset) {
    // @Adjustment: Long bytecodes need more than -2
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}


static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }
    bool canAssign = precedence <= PREC_ASSIGN;

    prefix_rule(canAssign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(canAssign);
    }

    if (canAssign && match(TOKEN_EQ)) {
        error("Invalid assignment target.");
    }
}

static int identifier_constant(Token* name) {
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static void add_local(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    // local->depth = current->scopeDepth;
}

static bool identifiers_equal(Token* n1, Token* n2) {
    if (n1->length != n2->length) return false;
    return memcmp(n1->start, n2->start, n1->length) == 0;
}

static int resolve_local(Compiler* comp, Token* name) {
    for (int i = comp->localCount - 1; i >= 0; i--) {
        Local* l = &comp->locals[i];
        if (identifiers_equal(name, &l->name)) {
            if (l->depth == -1) {
                error("Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static int add_upvalue(Compiler* comp, uint8_t index, bool isLocal) {
    int upvalueCount = comp->function->upvalueCount;
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* uv = &comp->upvalues[i];
        if (uv->index == index && uv->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }
    comp->upvalues[upvalueCount].isLocal = isLocal;
    comp->upvalues[upvalueCount].index = index;
    return comp->function->upvalueCount++;
}

static int resolve_upvalue(Compiler* comp, Token* name) {
    if (comp->enclosing == NULL) return -1;

    int local = resolve_local(comp->enclosing, name);
    if (local != -1) {
        return add_upvalue(comp, (uint8_t) local, true);
    }

    int uv = resolve_upvalue(comp->enclosing, name);
    if (uv != -1) {
        return add_upvalue(comp, (uint8_t) uv, false);
    }

    return -1;
}

static void declare_variable() {
    if (current->scopeDepth == 0) return;
    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifiers_equal(name, &local->name)) {
            error("There already exists a variable with the same name in this scope");
        }
    }
    add_local(*name);
}

static int parse_variable(const char* errorMsg) {
    consume(TOKEN_IDENTIFIER, errorMsg);
    declare_variable();
    if (current->scopeDepth > 0) return 0;
    return identifier_constant(&parser.previous);
}

static void mark_initialized() {
    if (current->scopeDepth == 0) {
        return;
    }
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void define_variable(uint8_t global) {
    if (current->scopeDepth > 0){
        mark_initialized();
        return;
    } 
    emit_long_bytes(OP_DEFINE_GLOBAL,(uint8_t) (global & 0xff), (uint8_t) ((global >> 8) & 0xff), (uint8_t) ((global >> 16) & 0xff)); 
    // emit_long_bytes(OP_DEFINE_GLOBAL,(uint8_t) ((global >> 16) & 0xff), (uint8_t) ((global >> 8) & 0xff), (uint8_t) (global & 0xff)); 
}

static void and_(bool canAssign) {
    int endJump = emit_jump(OP_JUMP_IF_FALSE); // If the left side is false, we know that we can ignore the rest
    emit_byte(OP_POP);
    parse_precedence(PREC_AND);
    patch_jump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emit_jump(OP_JUMP_IF_FALSE);
    int endJump = emit_jump(OP_JUMP);

    patch_jump(elseJump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(endJump);
}

static void expression() {
    parse_precedence(PREC_ASSIGN);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration(); 
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope(); // No need to end this, since the compiler just "ends" itself.

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_current("Cannot have more than 255 parameters.");
            }
            uint8_t constant = parse_variable("Expect parameter name.");
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after function parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* func = end_compiler();
    emit_bytes_by_opcode(OP_CLOSURE, make_constant(OBJ_VAL(func)));

    for (int i = 0; i < func->upvalueCount; i++) {
        emit_byte(compiler.upvalues[i].isLocal ? 1 : 0);
        emit_byte(compiler.upvalues[i].index); // @Improvement: This will need to be done in 3 bytes for LONG UVs
    }
}

static void let_declaration() {
    uint8_t global = parse_variable("Expect variable name.");
    printf("Global: %d\n", global);

    if (match(TOKEN_EQ)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    define_variable(global);
}

static void fun_declaration() {
    uint8_t global = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON,  "Expect ';' after value.");
    emit_byte(OP_POP);
}

static int emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff); // @Improvement: Use LONG jump instruction
    return current_chunk()->count - 2;
}

static void if_statement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");


    int thenJump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    int elseJump = emit_jump(OP_JUMP);
    patch_jump(thenJump); // We need to set the location of the jump, which was set to a default val
    emit_byte(OP_POP);
    if (match(TOKEN_ELSE)){
        statement(); 
    } 
    patch_jump(elseJump); // We need to set the location of the jump, which was set to a default val
}

static void while_statement() {
    int loopStart = current_chunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    
    int exitJump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loopStart);

    patch_jump(exitJump);
    emit_byte(OP_POP);
}

static void for_statement() {
    begin_scope();
    consume(TOKEN_LEFT_PAREN,  "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)){ 

    } else if (match(TOKEN_LET)) {
        let_declaration();
    } else {
        expression_statement();
    }
    consume(TOKEN_SEMICOLON, "Expect ';'.");

    int loopStart = current_chunk()->count;
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }
    consume(TOKEN_SEMICOLON, "Expect ';'.");
    // consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clause.");

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emit_jump(OP_JUMP);
        int incrementStart = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clause.");

        emit_loop(loopStart);
        loopStart = incrementStart;
        patch_jump(bodyJump);
    }

    statement();
    emit_loop(loopStart);
    if (exitJump != -1) {
        patch_jump(exitJump);
        emit_byte(OP_POP);
    }
    end_scope();
}

static void return_statement() {
    if (current->type == TYPE_SCRIPT) {
        error("Cannot return from global scope.");
    }
    if (match(TOKEN_SEMICOLON)) {
        emit_return();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_LET:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
            ; // @Noop
        }

        advance();
    }
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_IF)) {
        if_statement();
    }  else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match(TOKEN_WHILE)){
        while_statement();
    } else if (match(TOKEN_FOR)){
        for_statement();
    } else if (match(TOKEN_RETURN)) {
        return_statement();
    }
    else {
        expression_statement();
    }
}

static void declaration() {
    if (match(TOKEN_FUN)) {
        fun_declaration();
    } else if (match(TOKEN_LET)) {
        let_declaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    parse_precedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        default: return;
    }
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = get_rule(operatorType);
    parse_precedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBSTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        case TOKEN_BANG_EQ: emit_bytes(OP_EQ, OP_NOT); break;
        case TOKEN_EQ_EQ: emit_byte(OP_EQ); break;
        case TOKEN_GREATER: emit_byte(OP_GREATER); break;
        case TOKEN_GEQ: emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_byte(OP_LESS); break;
        case TOKEN_LEQ: emit_bytes(OP_GREATER, OP_NOT); break;
        default: return;
    }
}

static uint8_t argument_list() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) error("Cannot have more than 255 arguments.");
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(bool canAssign) {
    uint8_t argCount = argument_list();
    emit_bytes(OP_CALL, argCount);
}

static int make_constant(Value value) {
    return add_constant(current_chunk(), value);
    // return (uint8_t)constant.pos;
}

static void emit_constant(Value value) {
    emit_constant_bytes(make_constant(value));
}

// Old implementation:
// static void emitConstant(Value value) {
//   emitBytes(OP_CONSTANT, makeConstant(value)); // @Closed: differentiate between constant, constant_long
// }

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); return;
        case TOKEN_TRUE: emit_byte(OP_TRUE); return;
        case TOKEN_NIL: emit_byte(OP_NIL); return;
        default: return;
    }
}

static void string(bool canAssign) {
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolve_local(current, &name);
    if (arg != -1) {
        // @Cleanup. This is a mess because of 8 Bit local variables. Maybe just make them LONG
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        if (canAssign && match(TOKEN_EQ)) {
            expression();
            emit_bytes(setOp, arg);
        } else {
            emit_bytes(getOp, arg);
        }
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        // @Cleanup. This is a mess because of 8 Bit local variables. Not AS important
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
        if (canAssign && match(TOKEN_EQ)) {
            expression();
            emit_bytes(setOp, arg);
        } else {
            emit_bytes(getOp, arg);
        }
    }
    else {
        int arg = identifier_constant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        if (canAssign && match(TOKEN_EQ)) {
            expression();
            emit_long_bytes(setOp, (uint8_t) (arg & 0xff), (uint8_t) ((arg >> 8) & 0xff), ((arg >> 16) & 0xff));
        } else {
            emit_long_bytes(getOp, (uint8_t) (arg & 0xff), (uint8_t) ((arg >> 8) & 0xff), ((arg >> 16) & 0xff));
        }
    }
}

static void variable(bool canAssign) {
    named_variable(parser.previous, canAssign);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQ] = {NULL, binary, PREC_EQ},
    [TOKEN_EQ] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQ_EQ] = {NULL, binary, PREC_EQ},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARE},
    [TOKEN_GEQ] = {NULL, binary, PREC_COMPARE},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARE},
    [TOKEN_LEQ] = {NULL, binary, PREC_COMPARE},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

ObjFunction* compile(const char *source) {
    init_scanner(source);
    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    ObjFunction* func = end_compiler();
    return !parser.hadError ? func : NULL;
    // int line = -1;
    // for (;;) {
    //     Token token = scan_token();
    //     if (token.line != line) {
    //         printf("%4d ", token.line);
    //         line = token.line;
    //     } else {
    //         printf("   | ");
    //     }
    //     printf("%2d '%.*s'\n", token.type, token.length, token.start);
    //     if (token.type == TOKEN_EOF) break;
    // }
}
