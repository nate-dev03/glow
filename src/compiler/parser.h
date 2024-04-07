#ifndef GLOW_PARSER_H
#define GLOW_PARSER_H

#include "ast.h"

typedef enum {
	GLOW_TOK_NONE,           // indicates absent or unknown type

	/* literals */
	GLOW_TOK_INT,            // int literal
	GLOW_TOK_FLOAT,          // float literal
	GLOW_TOK_STR,            // quoted string: ".*" (escapes are allowed too)
	GLOW_TOK_IDENT,          // [a-zA-Z_][a-zA-Z_0-9]*

	/* operators */
	GLOW_TOK_OPS_START,  /* marker */
	GLOW_TOK_PLUS,
	GLOW_TOK_MINUS,
	GLOW_TOK_MUL,
	GLOW_TOK_DIV,
	GLOW_TOK_MOD,
	GLOW_TOK_POW,
	GLOW_TOK_BITAND,
	GLOW_TOK_BITOR,
	GLOW_TOK_XOR,
	GLOW_TOK_BITNOT,
	GLOW_TOK_SHIFTL,
	GLOW_TOK_SHIFTR,
	GLOW_TOK_AND,
	GLOW_TOK_OR,
	GLOW_TOK_NOT,
	GLOW_TOK_EQUAL,
	GLOW_TOK_NOTEQ,
	GLOW_TOK_LT,
	GLOW_TOK_GT,
	GLOW_TOK_LE,
	GLOW_TOK_GE,
	GLOW_TOK_AT,
	GLOW_TOK_DOT,
	GLOW_TOK_DOTDOT,
	GLOW_TOK_IN,  // really a keyword but treated as an operator in some contexts

	/* assignments */
	GLOW_TOK_ASSIGNMENTS_START,  /* marker */
	GLOW_TOK_ASSIGN,
	GLOW_TOK_ASSIGN_ADD,
	GLOW_TOK_ASSIGN_SUB,
	GLOW_TOK_ASSIGN_MUL,
	GLOW_TOK_ASSIGN_DIV,
	GLOW_TOK_ASSIGN_MOD,
	GLOW_TOK_ASSIGN_POW,
	GLOW_TOK_ASSIGN_BITAND,
	GLOW_TOK_ASSIGN_BITOR,
	GLOW_TOK_ASSIGN_XOR,
	GLOW_TOK_ASSIGN_SHIFTL,
	GLOW_TOK_ASSIGN_SHIFTR,
	GLOW_TOK_ASSIGN_AT,
	GLOW_TOK_ASSIGNMENTS_END,    /* marker */
	GLOW_TOK_OPS_END,    /* marker */

	GLOW_TOK_PAREN_OPEN,     // literal: (
	GLOW_TOK_PAREN_CLOSE,    // literal: )
	GLOW_TOK_BRACE_OPEN,     // literal: {
	GLOW_TOK_BRACE_CLOSE,    // literal: }
	GLOW_TOK_BRACK_OPEN,     // literal: [
	GLOW_TOK_BRACK_CLOSE,    // literal: ]

	/* keywords */
	GLOW_TOK_NULL,
	GLOW_TOK_ECHO,
	GLOW_TOK_IF,
	GLOW_TOK_ELIF,
	GLOW_TOK_ELSE,
	GLOW_TOK_WHILE,
	GLOW_TOK_FOR,
	GLOW_TOK_FUN,
	GLOW_TOK_GEN,
	GLOW_TOK_ACT,
	GLOW_TOK_BREAK,
	GLOW_TOK_CONTINUE,
	GLOW_TOK_RETURN,
	GLOW_TOK_THROW,
	GLOW_TOK_PRODUCE,
	GLOW_TOK_RECEIVE,
	GLOW_TOK_TRY,
	GLOW_TOK_CATCH,
	GLOW_TOK_IMPORT,
	GLOW_TOK_EXPORT,

	/* miscellaneous tokens */
	GLOW_TOK_COMMA,
	GLOW_TOK_COLON,
	GLOW_TOK_DOLLAR,

	/* statement terminators */
	GLOW_TOK_STMT_TERMINATOR,
	GLOW_TOK_SEMICOLON,
	GLOW_TOK_NEWLINE,
	GLOW_TOK_EOF             // should always be the last token
} GlowTokType;


#define GLOW_TOK_TYPE_IS_OP(type) ((GLOW_TOK_OPS_START < (type) && (type) < GLOW_TOK_OPS_END))

#define GLOW_TOK_TYPE_IS_ASSIGNMENT_TOK(type) \
	(GLOW_TOK_ASSIGNMENTS_START < (type) && (type) < GLOW_TOK_ASSIGNMENTS_END)

#define GLOW_TOK_TYPE_IS_STMT_TERM(type) (((type) > GLOW_TOK_STMT_TERMINATOR) || (type) == GLOW_TOK_BRACE_CLOSE)

typedef struct {
	const char *value;    // not null-terminated
	size_t length;
	GlowTokType type;
	unsigned int lineno;  // 1-based line number
} GlowToken;

typedef struct {
	/* source code to parse */
	const char *code;

	/* end of source code (last char) */
	const char *end;

	/* where we are in the string */
	char *pos;

	/* increases to consume token */
	unsigned int mark;

	/* tokens that have been read */
	GlowToken *tokens;
	size_t tok_count;
	size_t tok_capacity;

	/* the "peek-token" is somewhat complicated to
	   compute, so we cache it */
	GlowToken *peek;

	/* where we are in the tokens array */
	size_t tok_pos;

	/* the line number/position we are currently on */
	unsigned int lineno;

	/* name of the file out of which the source was read */
	const char *name;

	/* if an error occurred... */
	const char *error_msg;
	int error_type;

	/* maximum $N identifier in lambda */
	unsigned int max_dollar_ident;

	/* parse flags */
	unsigned in_function  : 1;
	unsigned in_lambda    : 1;
	unsigned in_generator : 1;
	unsigned in_actor     : 1;
	unsigned in_loop      : 1;
	unsigned in_args      : 1;
} GlowParser;

enum {
	GLOW_PARSE_ERR_NONE = 0,
	GLOW_PARSE_ERR_UNEXPECTED_CHAR,
	GLOW_PARSE_ERR_UNEXPECTED_TOKEN,
	GLOW_PARSE_ERR_NEWLINE_IN_STRING,
	GLOW_PARSE_ERR_NOT_A_STATEMENT,
	GLOW_PARSE_ERR_UNCLOSED,
	GLOW_PARSE_ERR_INVALID_ASSIGN,
	GLOW_PARSE_ERR_INVALID_BREAK,
	GLOW_PARSE_ERR_INVALID_CONTINUE,
	GLOW_PARSE_ERR_INVALID_RETURN,
	GLOW_PARSE_ERR_INVALID_PRODUCE,
	GLOW_PARSE_ERR_INVALID_RECEIVE,
	GLOW_PARSE_ERR_TOO_MANY_PARAMETERS,
	GLOW_PARSE_ERR_DUPLICATE_PARAMETERS,
	GLOW_PARSE_ERR_NON_DEFAULT_AFTER_DEFAULT_PARAMETERS,
	GLOW_PARSE_ERR_MALFORMED_PARAMETERS,
	GLOW_PARSE_ERR_TOO_MANY_ARGUMENTS,
	GLOW_PARSE_ERR_DUPLICATE_NAMED_ARGUMENTS,
	GLOW_PARSE_ERR_UNNAMED_AFTER_NAMED_ARGUMENTS,
	GLOW_PARSE_ERR_MALFORMED_ARGUMENTS,
	GLOW_PARSE_ERR_EMPTY_CATCH,
	GLOW_PARSE_ERR_MISPLACED_DOLLAR_IDENTIFIER,
	GLOW_PARSE_ERR_INCONSISTENT_DICT_ELEMENTS,
	GLOW_PARSE_ERR_EMPTY_FOR_PARAMETERS,
	GLOW_PARSE_ERR_RETURN_VALUE_IN_GENERATOR
};

GlowParser *glow_parser_new(char *str, const char *name);
void glow_parser_free(GlowParser *p);
GlowProgram *glow_parse(GlowParser *p);

#define GLOW_PARSER_SET_ERROR_MSG(p, msg)   (p)->error_msg = (msg)
#define GLOW_PARSER_SET_ERROR_TYPE(p, type) (p)->error_type = (type)
#define GLOW_PARSER_ERROR(p)                ((p)->error_type != GLOW_PARSE_ERR_NONE)

#endif /* GLOW_PARSER_H */
