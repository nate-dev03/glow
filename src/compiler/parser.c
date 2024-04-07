#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "err.h"
#include "util.h"
#include "lexer.h"
#include "parser.h"

#define ERROR_CHECK(p) do { if (GLOW_PARSER_ERROR(p)) return NULL; } while (0)

#define ERROR_CHECK_AST(p, should_be_null, free_me) \
	do { \
		if (GLOW_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			glow_ast_free(free_me); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_AST2(p, should_be_null, free_me1, free_me2) \
	do { \
		if (GLOW_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			glow_ast_free(free_me1); \
			glow_ast_free(free_me2); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_AST3(p, should_be_null, free_me1, free_me2, free_me3) \
	do { \
		if (GLOW_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			glow_ast_free(free_me1); \
			glow_ast_free(free_me2); \
			glow_ast_free(free_me3); \
			return NULL; \
		} \
	} while (0)

#define ERROR_CHECK_LIST(p, should_be_null, free_me) \
	do { \
		if (GLOW_PARSER_ERROR(p)) { \
			assert((should_be_null) == NULL); \
			glow_ast_list_free(free_me); \
			return NULL; \
		} \
	} while (0)

typedef struct {
	GlowTokType type;
	unsigned int prec;  // precedence of operator
	bool assoc;         // associativity: true = left, false = right
} Op;

static Op op_from_tok_type(GlowTokType type);
static GlowNodeType nodetype_from_op(Op op);

static const Op ops[] = {
	/*
	OP,                 PREC,        ASSOC */
	{GLOW_TOK_PLUS,          70,          true},
	{GLOW_TOK_MINUS,         70,          true},
	{GLOW_TOK_MUL,           80,          true},
	{GLOW_TOK_DIV,           80,          true},
	{GLOW_TOK_MOD,           80,          true},
	{GLOW_TOK_POW,           90,          false},
	{GLOW_TOK_BITAND,        32,          true},
	{GLOW_TOK_BITOR,         30,          true},
	{GLOW_TOK_XOR,           31,          true},
	{GLOW_TOK_SHIFTL,        60,          true},
	{GLOW_TOK_SHIFTR,        60,          true},
	{GLOW_TOK_AND,           21,          true},
	{GLOW_TOK_OR,            20,          true},
	{GLOW_TOK_EQUAL,         40,          true},
	{GLOW_TOK_NOTEQ,         40,          true},
	{GLOW_TOK_LT,            50,          true},
	{GLOW_TOK_GT,            50,          true},
	{GLOW_TOK_LE,            50,          true},
	{GLOW_TOK_GE,            50,          true},
	{GLOW_TOK_ASSIGN,        10,          true},
	{GLOW_TOK_ASSIGN_ADD,    10,          true},
	{GLOW_TOK_ASSIGN_SUB,    10,          true},
	{GLOW_TOK_ASSIGN_MUL,    10,          true},
	{GLOW_TOK_ASSIGN_DIV,    10,          true},
	{GLOW_TOK_ASSIGN_MOD,    10,          true},
	{GLOW_TOK_ASSIGN_POW,    10,          true},
	{GLOW_TOK_ASSIGN_BITAND, 10,          true},
	{GLOW_TOK_ASSIGN_BITOR,  10,          true},
	{GLOW_TOK_ASSIGN_XOR,    10,          true},
	{GLOW_TOK_ASSIGN_SHIFTL, 10,          true},
	{GLOW_TOK_ASSIGN_SHIFTR, 10,          true},
	{GLOW_TOK_ASSIGN_AT,     10,          true},
	{GLOW_TOK_DOT,           99,          true},
	{GLOW_TOK_DOTDOT,        92,          true},
	{GLOW_TOK_AT,            91,          false},
	{GLOW_TOK_IN,             9,          true},
	{GLOW_TOK_IF,            22,          true},  /* ternary operator */
};

static const size_t ops_size = (sizeof(ops) / sizeof(Op));

#define FUNCTION_MAX_PARAMS 128

static GlowAST *parse_stmt(GlowParser *p);

static GlowAST *parse_expr(GlowParser *p);
static GlowAST *parse_expr_no_assign(GlowParser *p);
static GlowAST *parse_parens(GlowParser *p);
static GlowAST *parse_atom(GlowParser *p);
static GlowAST *parse_unop(GlowParser *p);

static GlowAST *parse_null(GlowParser *p);
static GlowAST *parse_int(GlowParser *p);
static GlowAST *parse_float(GlowParser *p);
static GlowAST *parse_str(GlowParser *p);
static GlowAST *parse_ident(GlowParser *p);
static GlowAST *parse_dollar_ident(GlowParser *p);

static GlowAST *parse_print(GlowParser *p);
static GlowAST *parse_if(GlowParser *p);
static GlowAST *parse_while(GlowParser *p);
static GlowAST *parse_for(GlowParser *p);
static GlowAST *parse_fun(GlowParser *p);
static GlowAST *parse_gen(GlowParser *p);
static GlowAST *parse_act(GlowParser *p);
static GlowAST *parse_break(GlowParser *p);
static GlowAST *parse_continue(GlowParser *p);
static GlowAST *parse_return(GlowParser *p);
static GlowAST *parse_throw(GlowParser *p);
static GlowAST *parse_produce(GlowParser *p);
static GlowAST *parse_receive(GlowParser *p);
static GlowAST *parse_try_catch(GlowParser *p);
static GlowAST *parse_import(GlowParser *p);
static GlowAST *parse_export(GlowParser *p);

static GlowAST *parse_block(GlowParser *p);
static GlowAST *parse_list(GlowParser *p);
static GlowAST *parse_set_or_dict(GlowParser *p);
static GlowAST *parse_lambda(GlowParser *p);

static GlowAST *parse_empty(GlowParser *p);

static struct glow_ast_list *parse_comma_separated_list(GlowParser *p,
                                                       const GlowTokType open_type, const GlowTokType close_type,
                                                       GlowAST *(*sub_element_parse_routine)(GlowParser *),
                                                       unsigned int *count);

static GlowToken *expect(GlowParser *p, GlowTokType type);

static void parse_err_unexpected_token(GlowParser *p, GlowToken *tok);
static void parse_err_not_a_statement(GlowParser *p, GlowToken *tok);
static void parse_err_unclosed(GlowParser *p, GlowToken *tok);
static void parse_err_invalid_assign(GlowParser *p, GlowToken *tok);
static void parse_err_invalid_break(GlowParser *p, GlowToken *tok);
static void parse_err_invalid_continue(GlowParser *p, GlowToken *tok);
static void parse_err_invalid_return(GlowParser *p, GlowToken *tok);
static void parse_err_invalid_produce(GlowParser *p, GlowToken *tok);
static void parse_err_invalid_receive(GlowParser *p, GlowToken *tok);
static void parse_err_too_many_params(GlowParser *p, GlowToken *tok);
static void parse_err_dup_params(GlowParser *p, GlowToken *tok, const char *param);
static void parse_err_non_default_after_default(GlowParser *p, GlowToken *tok);
static void parse_err_malformed_params(GlowParser *p, GlowToken *tok);
static void parse_err_too_many_args(GlowParser *p, GlowToken *tok);
static void parse_err_dup_named_args(GlowParser *p, GlowToken *tok, const char *name);
static void parse_err_unnamed_after_named(GlowParser *p, GlowToken *tok);
static void parse_err_malformed_args(GlowParser *p, GlowToken *tok);
static void parse_err_empty_catch(GlowParser *p, GlowToken *tok);
static void parse_err_misplaced_dollar_identifier(GlowParser *p, GlowToken *tok);
static void parse_err_inconsistent_dict_elements(GlowParser *p, GlowToken *tok);
static void parse_err_empty_for_params(GlowParser *p, GlowToken *tok);
static void parse_err_return_val_in_gen(GlowParser *p, GlowToken *tok);

GlowParser *glow_parser_new(char *str, const char *name)
{
#define INITIAL_TOKEN_ARRAY_CAPACITY 5
	GlowParser *p = glow_malloc(sizeof(GlowParser));
	p->code = str;
	p->end = &str[strlen(str) - 1];
	p->pos = &str[0];
	p->mark = 0;
	p->tokens = glow_malloc(INITIAL_TOKEN_ARRAY_CAPACITY * sizeof(GlowToken));
	p->tok_count = 0;
	p->tok_capacity = INITIAL_TOKEN_ARRAY_CAPACITY;
	p->tok_pos = 0;
	p->lineno = 1;
	p->peek = NULL;
	p->name = name;
	p->in_function = 0;
	p->in_lambda = 0;
	p->in_generator = 0;
	p->in_actor = 0;
	p->in_loop = 0;
	p->in_args = 0;
	p->error_type = GLOW_PARSE_ERR_NONE;
	p->error_msg = NULL;

	glow_parser_tokenize(p);

	return p;
#undef INITIAL_TOKEN_ARRAY_CAPACITY
}

void glow_parser_free(GlowParser *p)
{
	free(p->tokens);
	GLOW_FREE(p->error_msg);
	free(p);
}

GlowProgram *glow_parse(GlowParser *p)
{
	GlowProgram *head = glow_ast_list_new();
	struct glow_ast_list *node = head;

	while (glow_parser_has_next_token(p)) {
		GlowAST *stmt = parse_stmt(p);
		ERROR_CHECK_LIST(p, stmt, head);

		if (stmt == NULL) {
			break;
		}

		/*
		 * We don't include empty statements
		 * in the syntax tree.
		 */
		if (stmt->type == GLOW_NODE_EMPTY) {
			free(stmt);
			continue;
		}

		if (node->ast != NULL) {
			node->next = glow_ast_list_new();
			node = node->next;
		}

		node->ast = stmt;
	}

	return head;
}

/*
 * Parses a top-level statement
 */
static GlowAST *parse_stmt(GlowParser *p)
{
	GlowToken *tok = glow_parser_peek_token(p);

	GlowAST *stmt;

	switch (tok->type) {
	case GLOW_TOK_ECHO:
		stmt = parse_print(p);
		break;
	case GLOW_TOK_IF:
		stmt = parse_if(p);
		break;
	case GLOW_TOK_WHILE:
		stmt = parse_while(p);
		break;
	case GLOW_TOK_FOR:
		stmt = parse_for(p);
		break;
	case GLOW_TOK_FUN:
		stmt = parse_fun(p);
		break;
	case GLOW_TOK_GEN:
		stmt = parse_gen(p);
		break;
	case GLOW_TOK_ACT:
		stmt = parse_act(p);
		break;
	case GLOW_TOK_BREAK:
		stmt = parse_break(p);
		break;
	case GLOW_TOK_CONTINUE:
		stmt = parse_continue(p);
		break;
	case GLOW_TOK_RETURN:
		stmt = parse_return(p);
		break;
	case GLOW_TOK_THROW:
		stmt = parse_throw(p);
		break;
	case GLOW_TOK_PRODUCE:
		stmt = parse_produce(p);
		break;
	case GLOW_TOK_RECEIVE:
		stmt = parse_receive(p);
		break;
	case GLOW_TOK_TRY:
		stmt = parse_try_catch(p);
		break;
	case GLOW_TOK_IMPORT:
		stmt = parse_import(p);
		break;
	case GLOW_TOK_EXPORT:
		stmt = parse_export(p);
		break;
	case GLOW_TOK_SEMICOLON:
		return parse_empty(p);
	case GLOW_TOK_EOF:
		return NULL;
	default: {
		GlowAST *expr_stmt = parse_expr(p);
		ERROR_CHECK(p);
		const GlowNodeType type = expr_stmt->type;

		/*
		 * Not every expression is considered a statement. For
		 * example, the expression "2 + 2" on its own does not
		 * have a useful effect and is therefore not considered
		 * a valid statement. An assignment like "a = 2", on the
		 * other hand, is considered a valid statement. We must
		 * ensure that the expression we have just parsed is a
		 * valid statement.
		 */
		if (!GLOW_NODE_TYPE_IS_EXPR_STMT(type)) {
			parse_err_not_a_statement(p, tok);
			glow_ast_free(expr_stmt);
			return NULL;
		}

		stmt = expr_stmt;
	}
	}
	ERROR_CHECK(p);

	GlowToken *stmt_end = glow_parser_peek_token_direct(p);
	const GlowTokType stmt_end_type = stmt_end->type;

	if (!GLOW_TOK_TYPE_IS_STMT_TERM(stmt_end_type)) {
		parse_err_unexpected_token(p, stmt_end);
		glow_ast_free(stmt);
		return NULL;
	}

	return stmt;
}

static GlowAST *parse_expr_helper(GlowParser *p, const bool allow_assigns);

static GlowAST *parse_expr_min_prec(GlowParser *p, unsigned int min_prec, bool allow_assigns);

static GlowAST *parse_expr(GlowParser *p)
{
	return parse_expr_helper(p, true);
}

static GlowAST *parse_expr_no_assign(GlowParser *p)
{
	return parse_expr_helper(p, false);
}

static GlowAST *parse_expr_helper(GlowParser *p, const bool allow_assigns)
{
	return parse_expr_min_prec(p, 1, allow_assigns);
}

/*
 * Implementation of precedence climbing method.
 */
static GlowAST *parse_expr_min_prec(GlowParser *p, unsigned int min_prec, bool allow_assigns)
{
	GlowAST *lhs = parse_atom(p);
	ERROR_CHECK(p);

	while (glow_parser_has_next_token(p)) {

		GlowToken *tok = glow_parser_peek_token(p);
		const GlowTokType type = tok->type;
		GlowToken *next_direct = glow_parser_peek_token_direct(p);

		/*
		 * The 2nd component of the if-statement below is needed to
		 * distinguish something like:
		 *
		 *     print x if c else ...
		 *
		 * from, say:
		 *
		 *     print x
		 *     if c { ... }
		 */
		if (!GLOW_TOK_TYPE_IS_OP(type) && !(type == GLOW_TOK_IF && (tok == next_direct))) {
			break;
		}

		const Op op = op_from_tok_type(type);

		if (op.prec < min_prec) {
			break;
		}

		if (GLOW_TOK_TYPE_IS_ASSIGNMENT_TOK(op.type) &&
		    (!allow_assigns || min_prec != 1 || !GLOW_NODE_TYPE_IS_ASSIGNABLE(lhs->type))) {
			parse_err_invalid_assign(p, tok);
			glow_ast_free(lhs);
			return NULL;
		}

		const unsigned int next_min_prec = op.assoc ? (op.prec + 1) : op.prec;

		glow_parser_next_token(p);

		const bool ternary = (op.type == GLOW_TOK_IF);
		GlowAST *cond = NULL;

		if (ternary) {  /* ternary operator */
			cond = parse_expr_no_assign(p);
			expect(p, GLOW_TOK_ELSE);
			ERROR_CHECK_AST2(p, NULL, cond, lhs);
		}

		GlowAST *rhs = parse_expr_min_prec(p, next_min_prec, false);
		ERROR_CHECK_AST2(p, rhs, lhs, cond);

		GlowNodeType node_type = nodetype_from_op(op);
		GlowAST *ast = glow_ast_new(node_type, lhs, rhs, tok->lineno);

		if (ternary) {
			ast->v.middle = cond;
		}

		lhs = ast;
		allow_assigns = false;
	}

	return lhs;
}

/*
 * Parses a single unit of code. One of:
 *
 *  i.   single literal
 *           i-a) int literal
 *           i-b) float literal
 *           i-c) string literal
 *  ii.  parenthesized expression
 *  iii. variable
 *  iv.  dot operation [1]
 *
 *  Atoms can also consist of multiple postfix
 *  components:
 *
 *  i.   Call (e.g. "foo(a)(b, c)")
 *  ii.  Index (e.g. "foo[a][b][c]")
 *
 *  [1] Because of the high precedence of the dot
 *  operator, `parse_atom` treats an expression
 *  of the form `a.b.c` as one atom, and would,
 *  when parsing such an expression, return the
 *  following tree:
 *
 *          .
 *         / \
 *        .   c
 *       / \
 *      a   b
 *
 *  As a second example, consider `a.b(x.y)`. In
 *  this case, the following tree would be produced
 *  and returned:
 *
 *         ( ) --------> .
 *          |           / \
 *          .          x   y
 *         / \
 *        a   b
 *
 *  The horizontal arrow represents the parameters of
 *  the function call.
 */
static GlowAST *parse_atom(GlowParser *p)
{
	GlowToken *tok = glow_parser_peek_token(p);
	GlowAST *ast;

	switch (tok->type) {
	case GLOW_TOK_PAREN_OPEN:
		ast = parse_parens(p);
		break;
	case GLOW_TOK_NULL:
		ast = parse_null(p);
		break;
	case GLOW_TOK_INT:
		ast = parse_int(p);
		break;
	case GLOW_TOK_FLOAT:
		ast = parse_float(p);
		break;
	case GLOW_TOK_STR:
		ast = parse_str(p);
		break;
	case GLOW_TOK_IDENT:
		ast = parse_ident(p);

		/* type hints */
		tok = glow_parser_peek_token(p);
		if (p->in_args && tok->type == GLOW_TOK_COLON) {
			glow_parser_next_token(p);
			tok = glow_parser_peek_token(p);

			if (tok->type != GLOW_TOK_IDENT) {
				parse_err_unexpected_token(p, tok);
				return NULL;
			}

			GlowAST *type = parse_ident(p);
			assert(type != NULL);
			ast->left = type;
		}

		break;
	case GLOW_TOK_DOLLAR:
		ast = parse_dollar_ident(p);
		break;
	case GLOW_TOK_BRACK_OPEN:
		ast = parse_list(p);
		break;
	case GLOW_TOK_BRACE_OPEN:
		ast = parse_set_or_dict(p);
		break;
	case GLOW_TOK_NOT:
	case GLOW_TOK_BITNOT:
	case GLOW_TOK_PLUS:
	case GLOW_TOK_MINUS:
		ast = parse_unop(p);
		break;
	case GLOW_TOK_COLON:
		ast = parse_lambda(p);
		break;
	default:
		parse_err_unexpected_token(p, tok);
		ast = NULL;
		return NULL;
	}

	ERROR_CHECK(p);

	if (!glow_parser_has_next_token(p)) {
		goto end;
	}

	tok = glow_parser_peek_token(p);

	/*
	 * Deal with cases like `foo[7].bar(42)`...
	 */
	while (tok->type == GLOW_TOK_DOT || tok->type == GLOW_TOK_PAREN_OPEN || tok->type == GLOW_TOK_BRACK_OPEN) {
		switch (tok->type) {
		case GLOW_TOK_DOT: {
			GlowToken *dot_tok = expect(p, GLOW_TOK_DOT);
			ERROR_CHECK_AST(p, dot_tok, ast);
			GlowAST *ident = parse_ident(p);
			ERROR_CHECK_AST(p, ident, ast);
			GlowAST *dot = glow_ast_new(GLOW_NODE_DOT, ast, ident, dot_tok->lineno);
			ast = dot;
			break;
		}
		case GLOW_TOK_PAREN_OPEN: {
			unsigned int nargs;
			GlowParamList *params = parse_comma_separated_list(p,
			                                                  GLOW_TOK_PAREN_OPEN, GLOW_TOK_PAREN_CLOSE,
			                                                  parse_expr,
			                                                  &nargs);

			ERROR_CHECK_AST(p, params, ast);

			/* argument syntax check */
			for (struct glow_ast_list *param = params; param != NULL; param = param->next) {
				if (GLOW_NODE_TYPE_IS_ASSIGNMENT(param->ast->type) &&
				      (param->ast->type != GLOW_NODE_ASSIGN ||
				       param->ast->left->type != GLOW_NODE_IDENT)) {
					parse_err_malformed_args(p, tok);
					glow_ast_list_free(params);
					glow_ast_free(ast);
					return NULL;
				}
			}

			if (nargs > FUNCTION_MAX_PARAMS) {
				parse_err_too_many_args(p, tok);
				glow_ast_list_free(params);
				glow_ast_free(ast);
				return NULL;
			}

			/* no unnamed arguments after named ones */
			bool named = false;
			for (struct glow_ast_list *param = params; param != NULL; param = param->next) {
				if (param->ast->type == GLOW_NODE_ASSIGN) {
					named = true;
				} else if (named) {
					parse_err_unnamed_after_named(p, tok);
					glow_ast_list_free(params);
					glow_ast_free(ast);
					return NULL;
				}
			}

			/* no duplicate named arguments */
			for (struct glow_ast_list *param = params; param != NULL; param = param->next) {
				if (param->ast->type != GLOW_NODE_ASSIGN) {
					continue;
				}
				GlowAST *ast1 = param->ast->left;

				for (struct glow_ast_list *check = params; check != param; check = check->next) {
					if (check->ast->type != GLOW_NODE_ASSIGN) {
						continue;
					}
					GlowAST *ast2 = check->ast->left;

					if (glow_str_eq(ast1->v.ident, ast2->v.ident)) {
						parse_err_dup_named_args(p, tok, ast1->v.ident->value);
						glow_ast_list_free(params);
						glow_ast_free(ast);
						return NULL;
					}
				}
			}

			GlowAST *call = glow_ast_new(GLOW_NODE_CALL, ast, NULL, tok->lineno);
			call->v.params = params;
			ast = call;
			break;
		}
		case GLOW_TOK_BRACK_OPEN: {
			expect(p, GLOW_TOK_BRACK_OPEN);
			ERROR_CHECK_AST(p, NULL, ast);
			GlowAST *index = parse_expr_no_assign(p);
			ERROR_CHECK_AST(p, index, ast);
			expect(p, GLOW_TOK_BRACK_CLOSE);
			ERROR_CHECK_AST2(p, NULL, ast, index);
			GlowAST *index_expr = glow_ast_new(GLOW_NODE_INDEX, ast, index, tok->lineno);
			ast = index_expr;
			break;
		}
		default: {
			GLOW_INTERNAL_ERROR();
		}
		}

		tok = glow_parser_peek_token(p);
	}

	end:
	return ast;
}

/*
 * Parses parenthesized expression.
 */
static GlowAST *parse_parens(GlowParser *p)
{
	GlowToken *paren_open = expect(p, GLOW_TOK_PAREN_OPEN);
	ERROR_CHECK(p);
	const unsigned int lineno = paren_open->lineno;

	GlowToken *peek = glow_parser_peek_token(p);

	if (peek->type == GLOW_TOK_PAREN_CLOSE) {
		/* we have an empty tuple */

		expect(p, GLOW_TOK_PAREN_CLOSE);
		ERROR_CHECK(p);
		GlowAST *ast = glow_ast_new(GLOW_NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = NULL;
		return ast;
	}

	/* Now we either have a regular parenthesized expression
	 * OR
	 * we have a non-empty tuple.
	 */

	GlowAST *ast = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	peek = glow_parser_peek_token(p);

	if (peek->type == GLOW_TOK_COMMA) {
		/* we have a non-empty tuple */

		expect(p, GLOW_TOK_COMMA);
		ERROR_CHECK_AST(p, NULL, ast);
		struct glow_ast_list *list_head = glow_ast_list_new();
		list_head->ast = ast;
		struct glow_ast_list *list = list_head;

		do {
			GlowToken *next = glow_parser_peek_token(p);

			if (next->type == GLOW_TOK_EOF) {
				parse_err_unclosed(p, paren_open);
				ERROR_CHECK_LIST(p, NULL, list_head);
			}

			if (next->type == GLOW_TOK_PAREN_CLOSE) {
				break;
			}

			list->next = glow_ast_list_new();
			list = list->next;
			list->ast = parse_expr_no_assign(p);
			ERROR_CHECK_LIST(p, list->ast, list_head);

			next = glow_parser_peek_token(p);

			if (next->type == GLOW_TOK_COMMA) {
				expect(p, GLOW_TOK_COMMA);
				ERROR_CHECK_LIST(p, NULL, list_head);
			} else if (next->type != GLOW_TOK_PAREN_CLOSE) {
				parse_err_unexpected_token(p, next);
				ERROR_CHECK_LIST(p, NULL, list_head);
			}
		} while (true);

		ast = glow_ast_new(GLOW_NODE_TUPLE, NULL, NULL, lineno);
		ast->v.list = list_head;
	}

	expect(p, GLOW_TOK_PAREN_CLOSE);
	ERROR_CHECK_AST(p, NULL, ast);

	return ast;
}

static GlowAST *parse_unop(GlowParser *p)
{
	GlowToken *tok = glow_parser_next_token(p);

	GlowNodeType type;
	switch (tok->type) {
	case GLOW_TOK_PLUS:
		type = GLOW_NODE_UPLUS;
		break;
	case GLOW_TOK_MINUS:
		type = GLOW_NODE_UMINUS;
		break;
	case GLOW_TOK_BITNOT:
		type = GLOW_NODE_BITNOT;
		break;
	case GLOW_TOK_NOT:
		type = GLOW_NODE_NOT;
		break;
	default:
		type = GLOW_NODE_EMPTY;
		GLOW_INTERNAL_ERROR();
		break;
	}

	GlowAST *atom = parse_atom(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(type, atom, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_null(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_NULL);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_NULL, NULL, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_int(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_INT);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_INT, NULL, NULL, tok->lineno);
	ast->v.int_val = atoi(tok->value);
	return ast;
}

static GlowAST *parse_float(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_FLOAT);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_FLOAT, NULL, NULL, tok->lineno);
	ast->v.float_val = atof(tok->value);
	return ast;
}

static GlowAST *parse_str(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_STR);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_STRING, NULL, NULL, tok->lineno);

	// deal with quotes appropriately:
	ast->v.str_val = glow_str_new_copy(tok->value + 1, tok->length - 2);

	return ast;
}

static GlowAST *parse_ident(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_IDENT);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = glow_str_new_copy(tok->value, tok->length);
	return ast;
}

static GlowAST *parse_dollar_ident(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_DOLLAR);
	ERROR_CHECK(p);

	if (!p->in_lambda) {
		parse_err_misplaced_dollar_identifier(p, tok);
		return NULL;
	}

	const unsigned int value = atoi(&tok->value[1]);
	assert(value > 0);

	if (value > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(p, tok);
		return NULL;
	}

	if (value > p->max_dollar_ident) {
		p->max_dollar_ident = value;
	}

	GlowAST *ast = glow_ast_new(GLOW_NODE_IDENT, NULL, NULL, tok->lineno);
	ast->v.ident = glow_str_new_copy(tok->value, tok->length);
	return ast;
}

static GlowAST *parse_print(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_ECHO);
	ERROR_CHECK(p);
	GlowAST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_PRINT, expr, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_if(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_IF);
	ERROR_CHECK(p);
	GlowAST *condition = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	GlowAST *body = parse_block(p);
	ERROR_CHECK_AST(p, body, condition);
	GlowAST *ast = glow_ast_new(GLOW_NODE_IF, condition, body, tok->lineno);
	ast->v.middle = NULL;

	GlowAST *else_chain_base = NULL;
	GlowAST *else_chain_last = NULL;

	while ((tok = glow_parser_peek_token(p))->type == GLOW_TOK_ELIF) {
		expect(p, GLOW_TOK_ELIF);
		ERROR_CHECK_AST2(p, NULL, ast, else_chain_base);
		GlowAST *elif_condition = parse_expr_no_assign(p);
		ERROR_CHECK_AST2(p, elif_condition, ast, else_chain_base);
		GlowAST *elif_body = parse_block(p);
		ERROR_CHECK_AST3(p, elif_body, ast, else_chain_base, elif_condition);
		GlowAST *elif = glow_ast_new(GLOW_NODE_ELIF, elif_condition, elif_body, tok->lineno);
		elif->v.middle = NULL;

		if (else_chain_base == NULL) {
			else_chain_base = else_chain_last = elif;
		} else {
			else_chain_last->v.middle = elif;
			else_chain_last = elif;
		}
	}

	if ((tok = glow_parser_peek_token(p))->type == GLOW_TOK_ELSE) {
		expect(p, GLOW_TOK_ELSE);
		ERROR_CHECK_AST2(p, NULL, ast, else_chain_base);
		GlowAST *else_body = parse_block(p);
		ERROR_CHECK_AST2(p, else_body, ast, else_chain_base);
		GlowAST *else_ast = glow_ast_new(GLOW_NODE_ELSE, else_body, NULL, tok->lineno);

		if (else_chain_base == NULL) {
			else_chain_base = else_chain_last = else_ast;
		} else {
			else_chain_last->v.middle = else_ast;
			else_chain_last = else_ast;
		}
	}

	if (else_chain_last != NULL) {
		else_chain_last->v.middle = NULL;
	}

	ast->v.middle = else_chain_base;

	return ast;
}

static GlowAST *parse_while(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_WHILE);
	ERROR_CHECK(p);
	GlowAST *condition = parse_expr_no_assign(p);
	ERROR_CHECK(p);

	const unsigned old_in_loop = p->in_loop;
	p->in_loop = 1;
	GlowAST *body = parse_block(p);
	p->in_loop = old_in_loop;
	ERROR_CHECK_AST(p, body, condition);

	GlowAST *ast = glow_ast_new(GLOW_NODE_WHILE, condition, body, tok->lineno);
	return ast;
}

static GlowAST *parse_for(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_FOR);
	ERROR_CHECK(p);

	GlowToken *peek = glow_parser_peek_token(p);
	GlowAST *lcv;

	if (peek->type == GLOW_TOK_PAREN_OPEN) {
		unsigned int count;
		struct glow_ast_list *vars = parse_comma_separated_list(p,
		                                                       GLOW_TOK_PAREN_OPEN, GLOW_TOK_PAREN_CLOSE,
		                                                       parse_ident, &count);
		ERROR_CHECK_LIST(p, NULL, vars);

		if (vars == NULL) {
			parse_err_empty_for_params(p, peek);
			ERROR_CHECK(p);
		}

		lcv = glow_ast_new(GLOW_NODE_TUPLE, NULL, NULL, peek->lineno);
		lcv->v.list = vars;
	} else {
		lcv = parse_ident(p);  // loop-control variable
	}

	ERROR_CHECK(p);

	expect(p, GLOW_TOK_IN);
	ERROR_CHECK_AST(p, NULL, lcv);

	GlowAST *iter = parse_expr_no_assign(p);
	ERROR_CHECK_AST(p, iter, lcv);

	const unsigned old_in_loop = p->in_loop;
	p->in_loop = 1;
	GlowAST *body = parse_block(p);
	p->in_loop = old_in_loop;
	ERROR_CHECK_AST2(p, body, lcv, iter);

	GlowAST *ast = glow_ast_new(GLOW_NODE_FOR, lcv, iter, tok->lineno);
	ast->v.middle = body;
	return ast;
}

#define PARSE_FUN 0
#define PARSE_GEN 1
#define PARSE_ACT 2

static GlowAST *parse_fun_or_gen_or_act(GlowParser *p, const int select)
{
	GlowToken *tok;

	switch (select) {
	case PARSE_FUN:
		tok = expect(p, GLOW_TOK_FUN);
		break;
	case PARSE_GEN:
		tok = expect(p, GLOW_TOK_GEN);
		break;
	case PARSE_ACT:
		tok = expect(p, GLOW_TOK_ACT);
		break;
	default:
		GLOW_INTERNAL_ERROR();
	}

	ERROR_CHECK(p);
	GlowToken *name_tok = glow_parser_peek_token(p);
	GlowAST *name = parse_ident(p);
	ERROR_CHECK(p);

	const unsigned old_in_args = p->in_args;
	p->in_args = 1;
	unsigned int nargs;
	GlowParamList *params = parse_comma_separated_list(p,
	                                                  GLOW_TOK_PAREN_OPEN, GLOW_TOK_PAREN_CLOSE,
	                                                  parse_expr,
	                                                  &nargs);
	p->in_args = old_in_args;
	ERROR_CHECK_AST(p, params, name);

	/* parameter syntax check */
	for (struct glow_ast_list *param = params; param != NULL; param = param->next) {
		if (!((param->ast->type == GLOW_NODE_ASSIGN && param->ast->left->type == GLOW_NODE_IDENT) ||
		       param->ast->type == GLOW_NODE_IDENT)) {
			parse_err_malformed_params(p, name_tok);
			glow_ast_list_free(params);
			glow_ast_free(name);
			return NULL;
		}
	}

	if (nargs > FUNCTION_MAX_PARAMS) {
		parse_err_too_many_params(p, name_tok);
		glow_ast_list_free(params);
		glow_ast_free(name);
		return NULL;
	}

	/* no non-default parameters after default ones */
	bool dflt = false;
	for (struct glow_ast_list *param = params; param != NULL; param = param->next) {
		if (param->ast->type == GLOW_NODE_ASSIGN) {
			dflt = true;
		} else if (dflt) {
			parse_err_non_default_after_default(p, name_tok);
			glow_ast_list_free(params);
			glow_ast_free(name);
			return NULL;
		}
	}

	/* no duplicate parameter names */
	for (struct glow_ast_list *param = params; param != NULL; param = param->next) {
		GlowAST *ast1 = (param->ast->type == GLOW_NODE_ASSIGN) ? param->ast->left : param->ast;
		for (struct glow_ast_list *check = params; check != param; check = check->next) {
			GlowAST *ast2 = (check->ast->type == GLOW_NODE_ASSIGN) ? check->ast->left : check->ast;
			if (glow_str_eq(ast1->v.ident, ast2->v.ident)) {
				parse_err_dup_params(p, name_tok, ast1->v.ident->value);
				glow_ast_free(name);
				glow_ast_list_free(params);
				return NULL;
			}
		}
	}

	/* return value type hint */
	if (glow_parser_peek_token(p)->type == GLOW_TOK_COLON) {
		glow_parser_next_token(p);
		GlowAST *ret_hint = parse_ident(p);

		if (GLOW_PARSER_ERROR(p)) {
			assert(ret_hint == NULL);
			glow_ast_free(name);
			glow_ast_list_free(params);
			return NULL;
		}

		name->left = ret_hint;
	}

	const unsigned old_in_function = p->in_function;
	const unsigned old_in_generator = p->in_generator;
	const unsigned old_in_actor = p->in_actor;
	const unsigned old_in_lambda = p->in_lambda;
	const unsigned old_in_loop = p->in_loop;

	switch (select) {
	case PARSE_FUN:
		p->in_function = 1;
		p->in_generator = 0;
		p->in_actor = 0;
		break;
	case PARSE_GEN:
		p->in_function = 0;
		p->in_generator = 1;
		p->in_actor = 0;
		break;
	case PARSE_ACT:
		p->in_function = 0;
		p->in_generator = 0;
		p->in_actor = 1;
		break;
	default:
		GLOW_INTERNAL_ERROR();
	}

	p->in_lambda = 0;
	p->in_loop = 0;
	GlowAST *body = parse_block(p);
	p->in_function = old_in_function;
	p->in_generator = old_in_generator;
	p->in_actor = old_in_actor;
	p->in_lambda = old_in_lambda;
	p->in_loop = old_in_loop;
	p->in_args = old_in_args;

	if (GLOW_PARSER_ERROR(p)) {
		assert(body == NULL);
		glow_ast_free(name);
		glow_ast_list_free(params);
		return NULL;
	}

	GlowAST *ast;

	switch (select) {
	case PARSE_FUN:
		ast = glow_ast_new(GLOW_NODE_FUN, name, body, tok->lineno);
		break;
	case PARSE_GEN:
		ast = glow_ast_new(GLOW_NODE_GEN, name, body, tok->lineno);
		break;
	case PARSE_ACT:
		ast = glow_ast_new(GLOW_NODE_ACT, name, body, tok->lineno);
		break;
	default:
		GLOW_INTERNAL_ERROR();
	}

	ast->v.params = params;
	return ast;
}

static GlowAST *parse_fun(GlowParser *p)
{
	return parse_fun_or_gen_or_act(p, PARSE_FUN);
}

static GlowAST *parse_gen(GlowParser *p)
{
	return parse_fun_or_gen_or_act(p, PARSE_GEN);
}

static GlowAST *parse_act(GlowParser *p)
{
	return parse_fun_or_gen_or_act(p, PARSE_ACT);
}

#undef PARSE_FUN
#undef PARSE_GEN
#undef PARSE_ACT

static GlowAST *parse_break(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_BREAK);
	ERROR_CHECK(p);

	if (!p->in_loop) {
		parse_err_invalid_break(p, tok);
		return NULL;
	}

	GlowAST *ast = glow_ast_new(GLOW_NODE_BREAK, NULL, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_continue(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_CONTINUE);
	ERROR_CHECK(p);

	if (!p->in_loop) {
		parse_err_invalid_continue(p, tok);
		return NULL;
	}

	GlowAST *ast = glow_ast_new(GLOW_NODE_CONTINUE, NULL, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_return(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_RETURN);
	ERROR_CHECK(p);

	if (!(p->in_function || p->in_generator || p->in_actor)) {
		parse_err_invalid_return(p, tok);
		return NULL;
	}

	GlowToken *next = glow_parser_peek_token_direct(p);
	GlowAST *ast;

	if (GLOW_TOK_TYPE_IS_STMT_TERM(next->type)) {
		ast = glow_ast_new(GLOW_NODE_RETURN, NULL, NULL, tok->lineno);
	} else {
		if (p->in_generator) {
			parse_err_return_val_in_gen(p, tok);
			return NULL;
		}

		GlowAST *expr = parse_expr_no_assign(p);
		ERROR_CHECK(p);
		ast = glow_ast_new(GLOW_NODE_RETURN, expr, NULL, tok->lineno);
	}

	return ast;
}

static GlowAST *parse_throw(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_THROW);
	ERROR_CHECK(p);
	GlowAST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_THROW, expr, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_produce(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_PRODUCE);
	ERROR_CHECK(p);

	if (!p->in_generator) {
		parse_err_invalid_produce(p, tok);
		return NULL;
	}

	GlowAST *expr = parse_expr_no_assign(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_PRODUCE, expr, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_receive(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_RECEIVE);
	ERROR_CHECK(p);

	if (!p->in_actor) {
		parse_err_invalid_receive(p, tok);
		return NULL;
	}

	GlowAST *ident = parse_ident(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_RECEIVE, ident, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_try_catch(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_TRY);
	ERROR_CHECK(p);
	GlowAST *try_body = parse_block(p);
	ERROR_CHECK(p);
	GlowToken *catch = expect(p, GLOW_TOK_CATCH);
	ERROR_CHECK_AST(p, catch, try_body);

	unsigned int count;
	struct glow_ast_list *exc_list = parse_comma_separated_list(p,
	                                                           GLOW_TOK_PAREN_OPEN,
	                                                           GLOW_TOK_PAREN_CLOSE,
	                                                           parse_expr,
	                                                           &count);

	ERROR_CHECK_AST(p, exc_list, try_body);

	if (count == 0) {
		parse_err_empty_catch(p, catch);
		glow_ast_free(try_body);
		glow_ast_list_free(exc_list);
		return NULL;
	}

	GlowAST *catch_body = parse_block(p);

	if (GLOW_PARSER_ERROR(p)) {
		assert(catch_body == NULL);
		glow_ast_free(try_body);
		glow_ast_list_free(exc_list);
	}

	GlowAST *ast = glow_ast_new(GLOW_NODE_TRY_CATCH, try_body, catch_body, tok->lineno);
	ast->v.excs = exc_list;
	return ast;
}

static GlowAST *parse_import(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_IMPORT);
	ERROR_CHECK(p);
	GlowAST *ident = parse_ident(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_IMPORT, ident, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_export(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_EXPORT);
	ERROR_CHECK(p);
	GlowAST *ident = parse_ident(p);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_EXPORT, ident, NULL, tok->lineno);
	return ast;
}

static GlowAST *parse_block(GlowParser *p)
{
	GlowBlock *block_head = NULL;

	GlowToken *peek = glow_parser_peek_token(p);

	GlowToken *brace_open;

	if (peek->type == GLOW_TOK_COLON) {
		brace_open = expect(p, GLOW_TOK_COLON);
		ERROR_CHECK(p);
		block_head = glow_ast_list_new();
		block_head->ast = parse_stmt(p);
		ERROR_CHECK_LIST(p, block_head->ast, block_head);
	} else {
		brace_open = expect(p, GLOW_TOK_BRACE_OPEN);
		ERROR_CHECK(p);
		GlowBlock *block = NULL;

		do {
			GlowToken *next = glow_parser_peek_token(p);

			if (next->type == GLOW_TOK_EOF) {
				parse_err_unclosed(p, brace_open);
				return NULL;
			}

			if (next->type == GLOW_TOK_BRACE_CLOSE) {
				break;
			}

			GlowAST *stmt = parse_stmt(p);
			ERROR_CHECK_LIST(p, stmt, block_head);

			/*
			 * We don't include empty statements
			 * in the syntax tree.
			 */
			if (stmt->type == GLOW_NODE_EMPTY) {
				free(stmt);
				continue;
			}

			if (block_head == NULL) {
				block_head = glow_ast_list_new();
				block = block_head;
			}

			if (block->ast != NULL) {
				block->next = glow_ast_list_new();
				block = block->next;
			}

			block->ast = stmt;
		} while (true);

		expect(p, GLOW_TOK_BRACE_CLOSE);
		ERROR_CHECK_LIST(p, NULL, block_head);
	}

	GlowAST *ast = glow_ast_new(GLOW_NODE_BLOCK, NULL, NULL, brace_open->lineno);
	ast->v.block = block_head;
	return ast;
}

static GlowAST *parse_list(GlowParser *p)
{
	GlowToken *brack_open = glow_parser_peek_token(p);  /* parse_comma_separated_list does error checking */
	struct glow_ast_list *list_head = parse_comma_separated_list(p,
	                                                            GLOW_TOK_BRACK_OPEN,
	                                                            GLOW_TOK_BRACK_CLOSE,
	                                                            parse_expr,
	                                                            NULL);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_LIST, NULL, NULL, brack_open->lineno);
	ast->v.list = list_head;
	return ast;
}

static GlowAST *parse_dict_or_set_sub_element(GlowParser *p)
{
	GlowAST *k = parse_expr(p);
	ERROR_CHECK(p);

	GlowToken *sep = glow_parser_peek_token(p);

	if (sep->type == GLOW_TOK_COLON) {
		/* dict */
		expect(p, GLOW_TOK_COLON);
		GlowAST *v = parse_expr(p);
		ERROR_CHECK_AST(p, v, k);
		return glow_ast_new(GLOW_NODE_DICT_ELEM, k, v, k->lineno);
	} else {
		/* set */
		return k;
	}
}

static GlowAST *parse_set_or_dict(GlowParser *p)
{
	GlowToken *brace_open = glow_parser_peek_token(p);
	const unsigned int lineno = brace_open->lineno;
	struct glow_ast_list *head = parse_comma_separated_list(p,
	                                                       GLOW_TOK_BRACE_OPEN,
	                                                       GLOW_TOK_BRACE_CLOSE,
	                                                       parse_dict_or_set_sub_element,
	                                                       NULL);
	ERROR_CHECK(p);

	GlowAST *ast;

	if (head != NULL) {
		/* check if the elements are consistent */
		const bool is_dict = (head->ast->type == GLOW_NODE_DICT_ELEM);

		for (struct glow_ast_list *node = head; node != NULL; node = node->next) {
			if (is_dict ^ (node->ast->type == GLOW_NODE_DICT_ELEM)) {
				parse_err_inconsistent_dict_elements(p, brace_open);
				ERROR_CHECK_LIST(p, NULL, head);
			}
		}

		ast = glow_ast_new(is_dict ? GLOW_NODE_DICT : GLOW_NODE_SET, NULL, NULL, lineno);
	} else {
		ast = glow_ast_new(GLOW_NODE_DICT, NULL, NULL, lineno);
	}

	ast->v.list = head;
	return ast;
}

static GlowAST *parse_lambda(GlowParser *p)
{
	GlowToken *colon = glow_parser_peek_token(p);
	expect(p, GLOW_TOK_COLON);
	ERROR_CHECK(p);

	const unsigned old_max_dollar_ident = p->max_dollar_ident;
	const unsigned old_in_function = p->in_function;
	const unsigned old_in_generator = p->in_generator;
	const unsigned old_in_actor = p->in_actor;
	const unsigned old_in_lambda = p->in_lambda;
	const unsigned old_in_loop = p->in_loop;
	const unsigned old_in_args = p->in_args;
	p->max_dollar_ident = 0;
	p->in_function = 1;
	p->in_generator = 0;
	p->in_actor = 0;
	p->in_loop = 0;
	p->in_lambda = 1;
	p->in_args = 0;
	GlowAST *body = parse_expr(p);
	const unsigned int max_dollar_ident = p->max_dollar_ident;
	p->max_dollar_ident = old_max_dollar_ident;
	p->in_function = old_in_function;
	p->in_generator = old_in_generator;
	p->in_actor = old_in_actor;
	p->in_lambda = old_in_lambda;
	p->in_loop = old_in_loop;
	p->in_args = old_in_args;

	ERROR_CHECK(p);

	GlowAST *ast = glow_ast_new(GLOW_NODE_LAMBDA, body, NULL, colon->lineno);
	ast->v.max_dollar_ident = max_dollar_ident;
	return ast;
}

static GlowAST *parse_empty(GlowParser *p)
{
	GlowToken *tok = expect(p, GLOW_TOK_SEMICOLON);
	ERROR_CHECK(p);
	GlowAST *ast = glow_ast_new(GLOW_NODE_EMPTY, NULL, NULL, tok->lineno);
	return ast;
}

/*
 * Parses generic comma-separated list with given start and end delimiters.
 * Returns the number of elements in this parsed list.
 */
static struct glow_ast_list *parse_comma_separated_list(GlowParser *p,
                                                       const GlowTokType open_type, const GlowTokType close_type,
                                                       GlowAST *(*sub_element_parse_routine)(GlowParser *),
                                                       unsigned int *count)
{
	GlowToken *tok_open = expect(p, open_type);
	ERROR_CHECK(p);
	struct glow_ast_list *list_head = NULL;
	struct glow_ast_list *list = NULL;

	unsigned int nelements = 0;

	do {
		GlowToken *next = glow_parser_peek_token(p);

		if (next->type == GLOW_TOK_EOF) {
			parse_err_unclosed(p, tok_open);
			ERROR_CHECK_LIST(p, NULL, list_head);
		}

		if (next->type == close_type) {
			break;
		}

		if (list_head == NULL) {
			list_head = glow_ast_list_new();
			list = list_head;
		}

		if (list->ast != NULL) {
			list->next = glow_ast_list_new();
			list = list->next;
		}

		list->ast = sub_element_parse_routine(p);
		ERROR_CHECK_LIST(p, list->ast, list_head);
		++nelements;

		next = glow_parser_peek_token(p);

		if (next->type == GLOW_TOK_COMMA) {
			expect(p, GLOW_TOK_COMMA);
			ERROR_CHECK_LIST(p, NULL, list_head);
		} else if (next->type != close_type) {
			parse_err_unexpected_token(p, next);
			ERROR_CHECK_LIST(p, NULL, list_head);
		}
	} while (true);

	expect(p, close_type);
	ERROR_CHECK_LIST(p, NULL, list_head);

	if (count != NULL) {
		*count = nelements;
	}

	return list_head;
}

static Op op_from_tok_type(GlowTokType type)
{
	for (size_t i = 0; i < ops_size; i++) {
		if (type == ops[i].type) {
			return ops[i];
		}
	}

	GLOW_INTERNAL_ERROR();
	static const Op sentinel;
	return sentinel;
}

static GlowNodeType nodetype_from_op(Op op)
{
	switch (op.type) {
	case GLOW_TOK_PLUS:
		return GLOW_NODE_ADD;
	case GLOW_TOK_MINUS:
		return GLOW_NODE_SUB;
	case GLOW_TOK_MUL:
		return GLOW_NODE_MUL;
	case GLOW_TOK_DIV:
		return GLOW_NODE_DIV;
	case GLOW_TOK_MOD:
		return GLOW_NODE_MOD;
	case GLOW_TOK_POW:
		return GLOW_NODE_POW;
	case GLOW_TOK_BITAND:
		return GLOW_NODE_BITAND;
	case GLOW_TOK_BITOR:
		return GLOW_NODE_BITOR;
	case GLOW_TOK_XOR:
		return GLOW_NODE_XOR;
	case GLOW_TOK_BITNOT:
		return GLOW_NODE_BITNOT;
	case GLOW_TOK_SHIFTL:
		return GLOW_NODE_SHIFTL;
	case GLOW_TOK_SHIFTR:
		return GLOW_NODE_SHIFTR;
	case GLOW_TOK_AND:
		return GLOW_NODE_AND;
	case GLOW_TOK_OR:
		return GLOW_NODE_OR;
	case GLOW_TOK_NOT:
		return GLOW_NODE_NOT;
	case GLOW_TOK_EQUAL:
		return GLOW_NODE_EQUAL;
	case GLOW_TOK_NOTEQ:
		return GLOW_NODE_NOTEQ;
	case GLOW_TOK_LT:
		return GLOW_NODE_LT;
	case GLOW_TOK_GT:
		return GLOW_NODE_GT;
	case GLOW_TOK_LE:
		return GLOW_NODE_LE;
	case GLOW_TOK_GE:
		return GLOW_NODE_GE;
	case GLOW_TOK_AT:
		return GLOW_NODE_APPLY;
	case GLOW_TOK_DOT:
		return GLOW_NODE_DOT;
	case GLOW_TOK_DOTDOT:
		return GLOW_NODE_DOTDOT;
	case GLOW_TOK_ASSIGN:
		return GLOW_NODE_ASSIGN;
	case GLOW_TOK_ASSIGN_ADD:
		return GLOW_NODE_ASSIGN_ADD;
	case GLOW_TOK_ASSIGN_SUB:
		return GLOW_NODE_ASSIGN_SUB;
	case GLOW_TOK_ASSIGN_MUL:
		return GLOW_NODE_ASSIGN_MUL;
	case GLOW_TOK_ASSIGN_DIV:
		return GLOW_NODE_ASSIGN_DIV;
	case GLOW_TOK_ASSIGN_MOD:
		return GLOW_NODE_ASSIGN_MOD;
	case GLOW_TOK_ASSIGN_POW:
		return GLOW_NODE_ASSIGN_POW;
	case GLOW_TOK_ASSIGN_BITAND:
		return GLOW_NODE_ASSIGN_BITAND;
	case GLOW_TOK_ASSIGN_BITOR:
		return GLOW_NODE_ASSIGN_BITOR;
	case GLOW_TOK_ASSIGN_XOR:
		return GLOW_NODE_ASSIGN_XOR;
	case GLOW_TOK_ASSIGN_SHIFTL:
		return GLOW_NODE_ASSIGN_SHIFTL;
	case GLOW_TOK_ASSIGN_SHIFTR:
		return GLOW_NODE_ASSIGN_SHIFTR;
	case GLOW_TOK_ASSIGN_AT:
		return GLOW_NODE_ASSIGN_APPLY;
	case GLOW_TOK_IN:
		return GLOW_NODE_IN;
	case GLOW_TOK_IF:
		return GLOW_NODE_COND_EXPR;
	default:
		GLOW_INTERNAL_ERROR();
		return -1;
	}
}

static GlowToken *expect(GlowParser *p, GlowTokType type)
{
	assert(type != GLOW_TOK_NONE);
	GlowToken *next = glow_parser_has_next_token(p) ? glow_parser_next_token(p) : NULL;
	const GlowTokType next_type = next ? next->type : GLOW_TOK_NONE;

	if (next_type != type) {
		parse_err_unexpected_token(p, next);
		ERROR_CHECK(p);
	}

	return next;
}

/*
 * Parser error functions
 */

static const char *err_on_tok(GlowParser *p, GlowToken *tok)
{
	return glow_err_on_char(tok->value, p->code, p->end, tok->lineno);
}

static void parse_err_unexpected_token(GlowParser *p, GlowToken *tok)
{
#define MAX_LEN 1024

	if (tok->type == GLOW_TOK_EOF) {
		/*
		 * This should really always be true,
		 * since we shouldn't have an unexpected
		 * token in an empty file.
		 */
		if (p->tok_count > 1) {
			const char *tok_err = "";
			bool free_tok_err = false;
			GlowToken *tokens = p->tokens;
			size_t last = p->tok_count - 2;

			while (last > 0 && tokens[last].type == GLOW_TOK_NEWLINE) {
				--last;
			}

			if (tokens[last].type != GLOW_TOK_NEWLINE) {
				tok_err = err_on_tok(p, &tokens[last]);
				free_tok_err = true;
			}

			GLOW_PARSER_SET_ERROR_MSG(p,
			                         glow_util_str_format(GLOW_SYNTAX_ERROR " unexpected end-of-file after token\n\n%s",
			                            p->name, tok->lineno, tok_err));

			if (free_tok_err) {
				GLOW_FREE(tok_err);
			}
		} else {
			GLOW_PARSER_SET_ERROR_MSG(p,
			                         glow_util_str_format(GLOW_SYNTAX_ERROR " unexpected end-of-file after token\n\n",
			                            p->name, tok->lineno));
		}
	} else {
		char tok_str[MAX_LEN];

		const size_t tok_len = (tok->length < (MAX_LEN - 1)) ? tok->length : (MAX_LEN - 1);
		memcpy(tok_str, tok->value, tok_len);
		tok_str[tok_len] = '\0';

		const char *tok_err = err_on_tok(p, tok);
		GLOW_PARSER_SET_ERROR_MSG(p,
		                         glow_util_str_format(GLOW_SYNTAX_ERROR " unexpected token: %s\n\n%s",
		                            p->name, tok->lineno, tok_str, tok_err));
		GLOW_FREE(tok_err);
	}

	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_UNEXPECTED_TOKEN);

#undef MAX_LEN
}

static void parse_err_not_a_statement(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " not a statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_NOT_A_STATEMENT);
}

static void parse_err_unclosed(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " unclosed\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_UNCLOSED);
}

static void parse_err_invalid_assign(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " misplaced assignment\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INVALID_ASSIGN);
}

static void parse_err_invalid_break(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " misplaced break statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INVALID_BREAK);
}

static void parse_err_invalid_continue(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " misplaced continue statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INVALID_CONTINUE);
}

static void parse_err_invalid_return(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " misplaced return statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INVALID_RETURN);
}

static void parse_err_invalid_produce(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " misplaced produce statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INVALID_PRODUCE);
}

static void parse_err_invalid_receive(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " misplaced receive statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INVALID_RECEIVE);
}

static void parse_err_too_many_params(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " function has too many parameters (max %d)\n\n%s",
	                            p->name, tok->lineno, FUNCTION_MAX_PARAMS, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_TOO_MANY_PARAMETERS);
}

static void parse_err_dup_params(GlowParser *p, GlowToken *tok, const char *param)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " function has duplicate parameter '%s'\n\n%s",
	                            p->name, tok->lineno, param, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_DUPLICATE_PARAMETERS);
}

static void parse_err_non_default_after_default(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " non-default parameter after default parameter\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_NON_DEFAULT_AFTER_DEFAULT_PARAMETERS);
}

static void parse_err_malformed_params(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " function has malformed parameters\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_MALFORMED_PARAMETERS);
}

static void parse_err_too_many_args(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " function call has too many arguments (max %d)\n\n%s",
	                            p->name, tok->lineno, FUNCTION_MAX_PARAMS, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_TOO_MANY_ARGUMENTS);
}

static void parse_err_dup_named_args(GlowParser *p, GlowToken *tok, const char *name)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " function call has duplicate named argument '%s'\n\n%s",
	                            p->name, tok->lineno, name, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_DUPLICATE_NAMED_ARGUMENTS);
}

static void parse_err_unnamed_after_named(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " unnamed arguments after named arguments\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_UNNAMED_AFTER_NAMED_ARGUMENTS);
}

static void parse_err_malformed_args(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " function call has malformed arguments\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_MALFORMED_ARGUMENTS);
}

static void parse_err_empty_catch(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " empty catch statement\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_EMPTY_CATCH);
}

static void parse_err_misplaced_dollar_identifier(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " dollar identifier outside lambda\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_MISPLACED_DOLLAR_IDENTIFIER);
}

static void parse_err_inconsistent_dict_elements(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " inconsistent dictionary elements\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_INCONSISTENT_DICT_ELEMENTS);
}

static void parse_err_empty_for_params(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " empty for-loop parameter list\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_EMPTY_FOR_PARAMETERS);
}

static void parse_err_return_val_in_gen(GlowParser *p, GlowToken *tok)
{
	const char *tok_err = err_on_tok(p, tok);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " generators cannot return a value\n\n%s",
	                            p->name, tok->lineno, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_RETURN_VALUE_IN_GENERATOR);
}
