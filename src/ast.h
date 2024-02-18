#ifndef comp_ast_h
#define comp_ast_h

typedef enum {
	AstAdd,
} ExprType;

typedef struct {
	ExprType type;
	int line;
} Expr;

typedef struct {
	Expr base;
	char op;
	Expr *left, *right;
} BinaryExpr;

#endif // comp_ast_h
