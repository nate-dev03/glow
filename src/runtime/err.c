#include "../include/err.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "intobject.h"
#include "floatobject.h"
#include "object.h"
#include "metaclass.h"
#include "util.h"
#include "exc.h"

#define X(a, b) b,
const char *glow_err_type_headers[] = {
	GLOW_ERR_TYPE_LIST
};
#undef X

#define TBM_INIT_CAPACITY 5
void glow_tb_manager_init(struct glow_traceback_manager *tbm)
{
	tbm->tb = glow_malloc(TBM_INIT_CAPACITY * sizeof(struct glow_traceback_stack_item));
	tbm->tb_count = 0;
	tbm->tb_cap = TBM_INIT_CAPACITY;
}

void glow_tb_manager_add(struct glow_traceback_manager *tbm,
                    const char *fn,
                    const unsigned int lineno)
{
	const size_t cap = tbm->tb_cap;
	if (tbm->tb_count == cap) {
		const size_t new_cap = (cap * 3)/2 + 1;
		tbm->tb = glow_realloc(tbm->tb, new_cap * sizeof(struct glow_traceback_stack_item));
		tbm->tb_cap = new_cap;
	}

	tbm->tb[tbm->tb_count++] = (struct glow_traceback_stack_item){glow_util_str_dup(fn), lineno};
}

void glow_tb_manager_print(struct glow_traceback_manager *tbm, FILE *out)
{
	fprintf(out, "Traceback:\n");
	const size_t count = tbm->tb_count;

	for (size_t i = 0; i < count; i++) {
		fprintf(out, "  Line %u in %s\n", tbm->tb[i].lineno, tbm->tb[i].fn);
	}
}

void glow_tb_manager_dealloc(struct glow_traceback_manager *tbm)
{
	const size_t count = tbm->tb_count;
	for (size_t i = 0; i < count; i++) {
		GLOW_FREE(tbm->tb[i].fn);
	}
	free(tbm->tb);
}

GlowError *glow_err_new(GlowErrorType type, const char *msg_format, ...)
{
	GlowError *error = glow_malloc(sizeof(GlowError));
	error->type = type;

	va_list args;
	va_start(args, msg_format);
	vsnprintf(error->msg, sizeof(error->msg), msg_format, args);
	va_end(args);

	glow_tb_manager_init(&error->tbm);
	return error;
}

void glow_err_free(GlowError *error)
{
	if (error == NULL) {
		return;
	}

	glow_tb_manager_dealloc(&error->tbm);
	free(error);
}

void glow_err_traceback_append(GlowError *error,
                            const char *fn,
                            const unsigned int lineno)
{
	glow_tb_manager_add(&error->tbm, fn, lineno);
}

void glow_err_traceback_print(GlowError *error, FILE *out)
{
	glow_tb_manager_print(&error->tbm, out);
}

#define FUNC_ERROR_HEADER "Function Error: "
#define ATTR_ERROR_HEADER "Attribute Error: "
#define FATAL_ERROR_HEADER "Fatal Error: "

GlowError *glow_err_invalid_file_signature_error(const char *module)
{
	return glow_err_new(GLOW_ERR_TYPE_FATAL,
	                   "invalid file signature encountered when loading module '%s'",
	                   module);
}

GlowError *glow_err_unbound(const char *var)
{
	return glow_err_new(GLOW_ERR_TYPE_NAME, "cannot reference unbound variable '%s'", var);
}

GlowError *glow_type_err_invalid_catch(const GlowClass *c1)
{
	if (c1 == &glow_meta_class) {
		return glow_err_new(GLOW_ERR_TYPE_TYPE,
		                   "cannot catch non-subclass of Exception");
	} else {
		return glow_err_new(GLOW_ERR_TYPE_TYPE,
		                   "cannot catch instances of class %s",
		                   c1->name);
	}
}

GlowError *glow_type_err_invalid_throw(const GlowClass *c1)
{
	return glow_err_new(GLOW_ERR_TYPE_TYPE,
	                   "can only throw instances of a subclass of Exception, not %s",
	                   c1->name);
}

GlowError *glow_err_div_by_zero(void)
{
	return glow_err_new(GLOW_ERR_TYPE_DIV_BY_ZERO, "division or modulo by zero");
}

GlowError *glow_err_multithreading_not_supported(void)
{
	return glow_err_new(GLOW_ERR_TYPE_NO_MT,
	                   "multithreading is not supported by this build of the Glow runtime");
}

void glow_err_print_msg(GlowError *e, FILE *out)
{
	fprintf(out, "%s: %s\n", glow_err_type_headers[e->type], e->msg);
}

/* compilation errors */

const char *glow_err_on_char(const char *culprit,
                        const char *code,
                        const char *end,
                        unsigned int target_line)
{
#define MAX_LEN 1024

	char line_str[MAX_LEN];
	char mark_str[MAX_LEN];

	unsigned int lineno = 1;
	char *line = (char *)code;
	size_t line_len = 0;

	while (lineno != target_line) {
		if (*line == '\n') {
			++lineno;
		}

		++line;
	}

	while (line[line_len] != '\n' &&
	       line + line_len - 1 != end &&
	       line_len < MAX_LEN) {

		++line_len;
	}

	memcpy(line_str, line, line_len);
	line_str[line_len] = '\0';

	size_t tok_offset = 0;
	while (line + tok_offset != culprit && tok_offset < MAX_LEN) {
		++tok_offset;
	}

	memset(mark_str, ' ', tok_offset);
	mark_str[tok_offset] = '^';
	mark_str[tok_offset + 1] = '\0';

	for (size_t i = 0; i < tok_offset; i++) {
		if (line_str[i] == '\t') {
			mark_str[i] = '\t';
		}
	}

	return glow_util_str_format("%s\n%s\n", line_str, mark_str);

#undef MAX_LEN
}
