#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

Parser parser;
Chunk* compilingChunk;

static Chunk* current_chunk() {
    return compilingChunk;
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
    if (idx < 256) {
        emit_byte(OP_CONSTANT);
        emit_byte((uint8_t) idx);
        // write_chunk(current_chunk(), OP_CONSTANT, parser.previous.line);
        // write_chunk(current_chunk(), (uint8_t) idx, parser.previous.line);
    } else {
        emit_byte(OP_CONSTANT_LONG);
        emit_byte((uint8_t) (idx & 0xff));
        emit_byte((uint8_t) ((idx >> 8) & 0xff));
        emit_byte((uint8_t) ((idx >> 16) & 0xff));
        // write_chunk(current_chunk(), OP_CONSTANT_LONG, parser.previous.line);
        // write_chunk(current_chunk(), (uint8_t) (idx & 0xff), parser.previous.line);
        // write_chunk(current_chunk(), (uint8_t) ((idx >> 8) & 0xff), parser.previous.line);
        // write_chunk(current_chunk(), (uint8_t) ((idx >> 16) & 0xff), parser.previous.line);
    }
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

static void emit_return() {
    emit_byte(OP_RETURN);
}

static void end_compiler() {
    emit_return();
    #ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassemble_chunk(current_chunk(), "code");
    }
    #endif
}

static void expression();
static void statement();
static void declaration();
static int make_constant(Value value);
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

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

static int parse_variable(const char* errorMsg) {
    consume(TOKEN_IDENTIFIER, errorMsg);
    return identifier_constant(&parser.previous);
}

static void define_variable(uint8_t global) {
    // @Cleanup: use a CONST_16 type opcode (emit 3 bytes) for 256*256 variables. Adjust READ_STRING()!!
    emit_long_bytes(OP_DEFINE_GLOBAL,(uint8_t) (global & 0xff), (uint8_t) ((global >> 8) & 0xff), ((global >> 16) & 0xff)); 
}

static void expression() {
    parse_precedence(PREC_ASSIGN);
}

static void let_declaration() {
    uint8_t global = parse_variable("Expect variable name.");

    if (match(TOKEN_EQ)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
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
    }  else {
        expression_statement();
    }
}

static void declaration() {
    if (match(TOKEN_LET)) {
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
    int arg = identifier_constant(&name);
    if (canAssign && match(TOKEN_EQ)) {
        expression();
        emit_long_bytes(OP_SET_GLOBAL, (uint8_t) (arg & 0xff), (uint8_t) ((arg >> 8) & 0xff), ((arg >> 16) & 0xff));
    } else {
        emit_long_bytes(OP_GET_GLOBAL, (uint8_t) (arg & 0xff), (uint8_t) ((arg >> 8) & 0xff), ((arg >> 16) & 0xff));
    }
}

static void variable(bool canAssign) {
    named_variable(parser.previous, canAssign);
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
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
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
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

bool compile(const char *source, Chunk* chunk) {
    init_scanner(source);
    parser.hadError = false;
    parser.panicMode = false;
    compilingChunk = chunk;
    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    end_compiler();
    return !parser.hadError;
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
