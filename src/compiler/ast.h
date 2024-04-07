#ifndef GLOW_AST_H
#define GLOW_AST_H

#include <stdio.h>
#include "str.h"

typedef enum {
	GLOW_NODE_EMPTY,

	GLOW_NODE_INT,
	GLOW_NODE_FLOAT,
	GLOW_NODE_STRING,
	GLOW_NODE_IDENT,

	GLOW_NODE_ADD,
	GLOW_NODE_SUB,
	GLOW_NODE_MUL,
	GLOW_NODE_DIV,
	GLOW_NODE_MOD,
	GLOW_NODE_POW,
	GLOW_NODE_BITAND,
	GLOW_NODE_BITOR,
	GLOW_NODE_XOR,
	GLOW_NODE_BITNOT,
	GLOW_NODE_SHIFTL,
	GLOW_NODE_SHIFTR,
	GLOW_NODE_AND,
	GLOW_NODE_OR,
	GLOW_NODE_NOT,
	GLOW_NODE_EQUAL,
	GLOW_NODE_NOTEQ,
	GLOW_NODE_LT,
	GLOW_NODE_GT,
	GLOW_NODE_LE,
	GLOW_NODE_GE,
	GLOW_NODE_APPLY,
	GLOW_NODE_DOT,
	GLOW_NODE_DOTDOT,
	GLOW_NODE_COND_EXPR,

	GLOW_NODE_ASSIGNMENTS_START,
	GLOW_NODE_ASSIGN,
	GLOW_NODE_ASSIGN_ADD,
	GLOW_NODE_ASSIGN_SUB,
	GLOW_NODE_ASSIGN_MUL,
	GLOW_NODE_ASSIGN_DIV,
	GLOW_NODE_ASSIGN_MOD,
	GLOW_NODE_ASSIGN_POW,
	GLOW_NODE_ASSIGN_BITAND,
	GLOW_NODE_ASSIGN_BITOR,
	GLOW_NODE_ASSIGN_XOR,
	GLOW_NODE_ASSIGN_SHIFTL,
	GLOW_NODE_ASSIGN_SHIFTR,
	GLOW_NODE_ASSIGN_APPLY,
	GLOW_NODE_ASSIGNMENTS_END,

	GLOW_NODE_UPLUS,
	GLOW_NODE_UMINUS,

	GLOW_NODE_NULL,
	GLOW_NODE_PRINT,
	GLOW_NODE_IF,
	GLOW_NODE_ELIF,
	GLOW_NODE_ELSE,
	GLOW_NODE_WHILE,
	GLOW_NODE_FOR,
	GLOW_NODE_IN,
	GLOW_NODE_FUN,
	GLOW_NODE_GEN,
	GLOW_NODE_ACT,
	GLOW_NODE_BREAK,
	GLOW_NODE_CONTINUE,
	GLOW_NODE_RETURN,
	GLOW_NODE_THROW,
	GLOW_NODE_PRODUCE,
	GLOW_NODE_RECEIVE,
	GLOW_NODE_TRY_CATCH,
	GLOW_NODE_IMPORT,
	GLOW_NODE_EXPORT,

	GLOW_NODE_BLOCK,
	GLOW_NODE_LIST,
	GLOW_NODE_TUPLE,
	GLOW_NODE_SET,
	GLOW_NODE_DICT,
	GLOW_NODE_LAMBDA,

	GLOW_NODE_CALL,
	GLOW_NODE_INDEX,
	GLOW_NODE_DICT_ELEM,
} GlowNodeType;

#define GLOW_AST_TYPE_ASSERT(ast, nodetype) assert((ast)->type == (nodetype))

struct glow_ast_list;
typedef struct glow_ast_list GlowProgram;
typedef struct glow_ast_list GlowBlock;
typedef struct glow_ast_list GlowParamList;
typedef struct glow_ast_list GlowExcList;

/*
 * Fundamental syntax tree unit
 */
typedef struct glow_ast {
	GlowNodeType type;
	unsigned int lineno;

	union {
		int int_val;
		double float_val;
		GlowStr *str_val;
		GlowStr *ident;
		struct glow_ast *middle;
		GlowBlock *block;
		GlowParamList *params;
		GlowExcList *excs;
		struct glow_ast_list *list;
		unsigned int max_dollar_ident;
	} v;

	struct glow_ast *left;
	struct glow_ast *right;
} GlowAST;

/*
 * `Block` represents a string of statements.
 */
struct glow_ast_list {
	GlowAST *ast;
	struct glow_ast_list *next;
};

GlowAST *glow_ast_new(GlowNodeType type, GlowAST *left, GlowAST *right, unsigned int lineno);
struct glow_ast_list *glow_ast_list_new(void);
void glow_ast_list_free(struct glow_ast_list *block);
void glow_ast_free(GlowAST *ast);

#define GLOW_NODE_TYPE_IS_ASSIGNMENT(type) (GLOW_NODE_ASSIGNMENTS_START < (type) && (type) < GLOW_NODE_ASSIGNMENTS_END)
#define GLOW_NODE_TYPE_IS_CALL(type)       ((type) == GLOW_NODE_CALL)
#define GLOW_NODE_TYPE_IS_EXPR_STMT(type)  (GLOW_NODE_TYPE_IS_CALL(type) || GLOW_NODE_TYPE_IS_ASSIGNMENT(type))
#define GLOW_NODE_TYPE_IS_ASSIGNABLE(type) ((type) == GLOW_NODE_IDENT || (type) == GLOW_NODE_DOT || (type) == GLOW_NODE_INDEX)

#endif /* GLOW_AST_H */
