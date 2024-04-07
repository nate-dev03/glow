#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "util.h"
#include "err.h"
#include "parser.h"
#include "lexer.h"

/*
 * (Lexer data is stored within the Parser structure. The comments in
 * this source file use the term "lexer" to refer to the subset of the
 * Parser structure pertaining to lexical analysis.)
 *
 * The lexical analysis stage consists of splitting the input source
 * into tokens. The source is fully tokenized upon the creation of a
 * Parser instance, which can then be queried to retrieve the tokens in
 * succession. A "Lexer" can, therefore, be thought of as a simple queue
 * of tokens.
 *
 * Specifically, the following operations are supported:
 *
 * - has_next   :  whether the given Lexer has any more tokens
 *
 * - next_token :  retrieve the next token from the given Lexer and
 *                 advance the Lexer on to the next token
 *
 * - peek_token :  retreive the next token from the given Lexer but
 *                 do not advance the Lexer on to the next token
 *
 * The Parser structure has a `pos` field and a `mark` field. When a
 * token is encountered, its first character is pointed to by `pos`,
 * and `mark` increases gradually from zero to "consume" the token.
 * Once the token has been read, `pos` is set to the start of the next
 * token and `mark` is set back to 0 so the same process can begin
 * again. This continues until tokenization is complete. Note that
 * `pos` is a character pointer and `mark` is an unsigned integer
 * indicating how many characters (starting from `pos`) have been
 * consumed as part of the token.
 */

static void lex_err_unexpected_char(GlowParser *p, const char *c);

static bool is_op_char(const char c)
{
	switch (c) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case '!':
	case '~':
	case '=':
	case '<':
	case '>':
	case '.':
	case '@':
		return true;
	default:
		return false;
	}
}

static GlowTokType str_to_op_toktype(const char *str, const size_t len)
{
	if (len == 0)
		return GLOW_TOK_NONE;

	switch (str[0]) {
	case '+':
		if (len == 1)
			return GLOW_TOK_PLUS;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_ADD;
			break;
		}
		break;
	case '-':
		if (len == 1)
			return GLOW_TOK_MINUS;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_SUB;
			break;
		}
		break;
	case '*':
		if (len == 1)
			return GLOW_TOK_MUL;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_MUL;
			break;
		case '*':
			if (len == 2)
				return GLOW_TOK_POW;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return GLOW_TOK_ASSIGN_POW;
				break;
			}
			break;
		}
		break;
	case '/':
		if (len == 1)
			return GLOW_TOK_DIV;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_DIV;
			break;
		}
		break;
	case '%':
		if (len == 1)
			return GLOW_TOK_MOD;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_MOD;
			break;
		}
		break;
	case '&':
		if (len == 1)
			return GLOW_TOK_BITAND;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_BITAND;
			break;
		case '&':
			if (len == 2)
				return GLOW_TOK_AND;
			break;
		}
		break;
	case '|':
		if (len == 1)
			return GLOW_TOK_BITOR;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_BITOR;
			break;
		case '|':
			if (len == 2)
				return GLOW_TOK_OR;
			break;
		}
		break;
	case '^':
		if (len == 1)
			return GLOW_TOK_XOR;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_XOR;
			break;
		}
		break;
	case '!':
		if (len == 1)
			return GLOW_TOK_NOT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_NOTEQ;
			break;
		}
		break;
	case '~':
		if (len == 1)
			return GLOW_TOK_BITNOT;

		switch (str[1]) {
		}
		break;
	case '=':
		if (len == 1)
			return GLOW_TOK_ASSIGN;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_EQUAL;
			break;
		}
		break;
	case '<':
		if (len == 1)
			return GLOW_TOK_LT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_LE;
			break;
		case '<':
			if (len == 2)
				return GLOW_TOK_SHIFTL;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return GLOW_TOK_ASSIGN_SHIFTL;
				break;
			}
			break;
		}
		break;
	case '>':
		if (len == 1)
			return GLOW_TOK_GT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_GE;
			break;
		case '>':
			if (len == 2)
				return GLOW_TOK_SHIFTR;

			switch (str[2]) {
			case '=':
				if (len == 3)
					return GLOW_TOK_ASSIGN_SHIFTR;
				break;
			}
			break;
		}
		break;
	case '.':
		if (len == 1)
			return GLOW_TOK_DOT;

		switch (str[1]) {
		case '.':
			if (len == 2)
				return GLOW_TOK_DOTDOT;
			break;
		}
		break;
	case '@':
		if (len == 1)
			return GLOW_TOK_AT;

		switch (str[1]) {
		case '=':
			if (len == 2)
				return GLOW_TOK_ASSIGN_AT;
			break;
		}
		break;
	}

	return GLOW_TOK_NONE;
}

static bool is_word_char(const char c)
{
	return (c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'));
}

static bool is_id_char(const char c)
{
	return (c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9'));
}

/*
 * The next few functions have comments that use
 * terminology from the extended comment at the
 * start of this file.
 */

/*
 * Forwards the lexer to its current mark then
 * resets the mark.
 */
static inline void fix(GlowParser *p)
{
	p->pos += p->mark + 1;
	p->mark = 0;
}

/*
 * Returns the lexer's current token based on
 * its position and mark.
 */
static GlowToken get(GlowParser *p, GlowTokType type)
{
	GlowToken tok;
	tok.value = p->pos;
	tok.length = p->mark + 1;
	tok.type = type;
	tok.lineno = p->lineno;

	return tok;
}

/*
 * Returns the lexer's current character.
 */
static inline char currc(GlowParser *p)
{
	return p->pos[p->mark];
}

/*
 * Returns the lexer's next character.
 */
static inline char nextc(GlowParser *p)
{
	return p->pos[p->mark + 1];
}

/*
 * Returns the lexer's next next character.
 */
static inline char next_nextc(GlowParser *p)
{
	return p->pos[p->mark + 2];
}

/*
 * Advances the lexer mark.
 */
static inline void adv(GlowParser *p)
{
	++p->mark;
}

/*
 * Forwards the lexer position.
 */
static inline void fwd(GlowParser *p)
{
	++p->pos;
}

/*
 * Rewinds the lexer mark.
 */
static inline void rew(GlowParser *p)
{
	--p->mark;
}

static inline bool isspace_except_newline(int c)
{
	return isspace(c) && c != '\n';
}

static void skip_spaces(GlowParser *p)
{
	while (isspace_except_newline(p->pos[0])) {
		fwd(p);
	}
}

static void read_digits(GlowParser *p)
{
	while (isdigit(nextc(p))) {
		adv(p);
	}
}

static GlowToken next_number(GlowParser *p)
{
	assert(isdigit(currc(p)));

	GlowTokType type = GLOW_TOK_INT;
	read_digits(p);

	if (nextc(p) == '.' && !is_op_char(next_nextc(p))) {
		adv(p);
		read_digits(p);
		type = GLOW_TOK_FLOAT;
	}

	GlowToken tok = get(p, type);
	fix(p);
	return tok;
}

static GlowToken next_string(GlowParser *p, const char delim)
{
	assert(delim == '"' || delim == '\'');
	assert(currc(p) == delim);
	adv(p);  // skip the first quotation character

	unsigned int escape_count = 0;

	do {
		const char c = currc(p);

		if (c == '\n') {
			GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_NEWLINE_IN_STRING);
			static GlowToken x;
			return x;
		}

		if (c == '\\') {
			++escape_count;
		} else if (c == delim && escape_count % 2 == 0) {
			break;
		} else {
			escape_count = 0;
		}

		adv(p);
	} while (true);

	GlowToken tok = get(p, GLOW_TOK_STR);

	fix(p);
	return tok;
}

static GlowToken next_op(GlowParser *p)
{
	assert(is_op_char(currc(p)));

	/*
	 * Consume all tokens that constitute simple operators,
	 * e.g. '+', '-', '*' etc.
	 */
	while (is_op_char(nextc(p))) {
		adv(p);
	}

	/*
	 * Back up until we reach a 'valid' operator,
	 * in accordance with the maximal munch rule.
	 */
	GlowTokType optype;

	while ((optype = str_to_op_toktype(p->pos, p->mark + 1)) == GLOW_TOK_NONE) {
		rew(p);
	}

	GlowToken tok = get(p, optype);
	fix(p);
	return tok;
}

static GlowToken next_word(GlowParser *p)
{
	static const struct {
		const char *keyword;
		GlowTokType type;
	} keywords[] = {
		{"null",     GLOW_TOK_NULL},
		{"echo",    GLOW_TOK_ECHO},
		{"if",       GLOW_TOK_IF},
		{"elif",     GLOW_TOK_ELIF},
		{"else",     GLOW_TOK_ELSE},
		{"while",    GLOW_TOK_WHILE},
		{"for",      GLOW_TOK_FOR},
		{"in",       GLOW_TOK_IN},
		{"fun",      GLOW_TOK_FUN},
		{"gen",      GLOW_TOK_GEN},
		{"act",      GLOW_TOK_ACT},
		{"break",    GLOW_TOK_BREAK},
		{"continue", GLOW_TOK_CONTINUE},
		{"return",   GLOW_TOK_RETURN},
		{"throw",    GLOW_TOK_THROW},
		{"produce",  GLOW_TOK_PRODUCE},
		{"receive",  GLOW_TOK_RECEIVE},
		{"try",      GLOW_TOK_TRY},
		{"catch",    GLOW_TOK_CATCH},
		{"import",   GLOW_TOK_IMPORT},
		{"export",   GLOW_TOK_EXPORT}
	};

	assert(is_word_char(currc(p)));

	while (is_id_char(nextc(p))) {
		adv(p);
	}

	GlowToken tok = get(p, GLOW_TOK_IDENT);
	fix(p);

	const size_t keywords_size = sizeof(keywords)/sizeof(keywords[0]);
	const char *word = tok.value;
	const size_t len = tok.length;

	for (size_t i = 0; i < keywords_size; i++) {
		const char *keyword = keywords[i].keyword;
		const size_t kwlen = strlen(keyword);

		if (kwlen == len && strncmp(word, keyword, len) == 0) {
			tok.type = keywords[i].type;
		}
	}

	return tok;
}

static GlowToken next_paren_open(GlowParser *p)
{
	assert(currc(p) == '(');
	GlowToken tok = get(p, GLOW_TOK_PAREN_OPEN);
	fwd(p);
	return tok;
}

static GlowToken next_paren_close(GlowParser *p)
{
	assert(currc(p) == ')');
	GlowToken tok = get(p, GLOW_TOK_PAREN_CLOSE);
	fwd(p);
	return tok;
}

static GlowToken next_brace_open(GlowParser *p)
{
	assert(currc(p) == '{');
	GlowToken tok = get(p, GLOW_TOK_BRACE_OPEN);
	fwd(p);
	return tok;
}

static GlowToken next_brace_close(GlowParser *p)
{
	assert(currc(p) == '}');
	GlowToken tok = get(p, GLOW_TOK_BRACE_CLOSE);
	fwd(p);
	return tok;
}

static GlowToken next_bracket_open(GlowParser *p)
{
	assert(currc(p) == '[');
	GlowToken tok = get(p, GLOW_TOK_BRACK_OPEN);
	fwd(p);
	return tok;
}

static GlowToken next_bracket_close(GlowParser *p)
{
	assert(currc(p) == ']');
	GlowToken tok = get(p, GLOW_TOK_BRACK_CLOSE);
	fwd(p);
	return tok;
}

static GlowToken next_comma(GlowParser *p)
{
	assert(currc(p) == ',');
	GlowToken tok = get(p, GLOW_TOK_COMMA);
	fwd(p);
	return tok;
}

static GlowToken next_colon(GlowParser *p)
{
	assert(currc(p) == ':');
	GlowToken tok = get(p, GLOW_TOK_COLON);
	fwd(p);
	return tok;
}

static GlowToken next_dollar_ident(GlowParser *p)
{
	assert(currc(p) == '$');

	if (!isdigit(nextc(p)) || nextc(p) == '0') {
		fwd(p);
		GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_UNEXPECTED_CHAR);
		static GlowToken x;
		return x;
	}

	read_digits(p);
	GlowToken tok = get(p, GLOW_TOK_DOLLAR);
	fix(p);
	return tok;
}

static GlowToken next_semicolon(GlowParser *p)
{
	assert(currc(p) == ';');
	GlowToken tok = get(p, GLOW_TOK_SEMICOLON);
	fwd(p);
	return tok;
}

static GlowToken next_newline(GlowParser *p)
{
	assert(currc(p) == '\n');
	GlowToken tok = get(p, GLOW_TOK_NEWLINE);
	fwd(p);
	++p->lineno;
	return tok;
}

static GlowToken eof_token(void)
{
	GlowToken tok;
	tok.value = NULL;
	tok.length = 0;
	tok.type = GLOW_TOK_EOF;
	tok.lineno = 0;
	return tok;
}

static void pass_comment(GlowParser *p)
{
	assert(currc(p) == '#');
	while (currc(p) != '\n' && currc(p) != '\0') {
		fwd(p);
	}
}

static void add_token(GlowParser *p, GlowToken *tok)
{
	const size_t size = p->tok_count;
	const size_t capacity = p->tok_capacity;

	if (size == capacity) {
		const size_t new_capacity = (capacity * 3) / 2 + 1;
		p->tok_capacity = new_capacity;
		p->tokens = glow_realloc(p->tokens, new_capacity * sizeof(GlowToken));
	}

	p->tokens[p->tok_count++] = *tok;
}

void glow_parser_tokenize(GlowParser *p)
{
	while (true) {
		skip_spaces(p);
		GlowToken tok;
		const char c = p->pos[0];

		if (c == '\0') {
			break;
		}

		if (isdigit(c)) {
			tok = next_number(p);
		}
		else if (is_op_char(c)) {
			tok = next_op(p);
		}
		else if (is_word_char(c)) {
			tok = next_word(p);
		}
		else {
			switch (c) {
			case '(':
				tok = next_paren_open(p);
				break;
			case ')':
				tok = next_paren_close(p);
				break;
			case '{':
				tok = next_brace_open(p);
				break;
			case '}':
				tok = next_brace_close(p);
				break;
			case '[':
				tok = next_bracket_open(p);
				break;
			case ']':
				tok = next_bracket_close(p);
				break;
			case ',':
				tok = next_comma(p);
				break;
			case '"':
			case '\'':
				tok = next_string(p, c);
				break;
			case ':':
				tok = next_colon(p);
				break;
			case '$':
				tok = next_dollar_ident(p);
				break;
			case ';':
				tok = next_semicolon(p);
				break;
			case '\n':
				tok = next_newline(p);
				break;
			case '#':
				pass_comment(p);
				continue;
			default:
				goto err;
			}
		}

		if (GLOW_PARSER_ERROR(p)) {
			goto err;
		}

		add_token(p, &tok);
	}

	GlowToken eof = eof_token();
	eof.lineno = p->lineno;
	add_token(p, &eof);
	return;

	err:
	lex_err_unexpected_char(p, p->pos);
	free(p->tokens);
	p->tokens = NULL;
	p->tok_count = 0;
	p->tok_capacity = 0;
}

/*
 * We don't care about certain tokens (e.g. newlines and
 * semicolons) except when they are required as statement
 * terminators. `parser_next_token` skips over these tokens,
 * but they can be accessed via `parser_next_token_direct`.
 *
 * `parser_peek_token` is analogous.
 */

GlowToken *glow_parser_next_token(GlowParser *p)
{
	GlowToken *tok;
	do {
		tok = glow_parser_next_token_direct(p);
	} while (tok->type == GLOW_TOK_NEWLINE);
	return tok;
}

GlowToken *glow_parser_next_token_direct(GlowParser *p)
{
	p->peek = NULL;

	GlowToken *next = &p->tokens[p->tok_pos];

	if (next->type != GLOW_TOK_EOF) {
		++p->tok_pos;
	}

	return next;
}

GlowToken *glow_parser_peek_token(GlowParser *p)
{
	if (p->peek != NULL) {
		return p->peek;
	}

	GlowToken *tokens = p->tokens;

	const size_t tok_count = p->tok_count;
	const size_t tok_pos = p->tok_pos;
	size_t offset = 0;

	while (tok_pos + offset < tok_count &&
	       tokens[tok_pos + offset].type == GLOW_TOK_NEWLINE) {

		++offset;
	}

	while (tok_pos + offset >= tok_count) {
		--offset;
	}

	return p->peek = &tokens[tok_pos + offset];
}

GlowToken *glow_parser_peek_token_direct(GlowParser *p)
{
	return &p->tokens[p->tok_pos];
}

bool glow_parser_has_next_token(GlowParser *p)
{
	return p->tokens[p->tok_pos].type != GLOW_TOK_EOF;
}

static void lex_err_unexpected_char(GlowParser *p, const char *c)
{
	const char *tok_err = glow_err_on_char(c, p->code, p->end, p->lineno);
	GLOW_PARSER_SET_ERROR_MSG(p,
	                         glow_util_str_format(GLOW_SYNTAX_ERROR " unexpected character: %c\n\n%s",
	                                             p->name, p->lineno, *c, tok_err));
	GLOW_FREE(tok_err);
	GLOW_PARSER_SET_ERROR_TYPE(p, GLOW_PARSE_ERR_UNEXPECTED_CHAR);
}
