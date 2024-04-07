#ifndef GLOW_ERR_H
#define GLOW_ERR_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "code.h"
#include "object.h"

/*
 * Irrecoverable runtime-error handling facilities:
 */

#define GLOW_ERR_TYPE_LIST \
	X(GLOW_ERR_TYPE_FATAL,       "Fatal Error") \
	X(GLOW_ERR_TYPE_TYPE,        "Type Error") \
	X(GLOW_ERR_TYPE_NAME,        "Name Error") \
	X(GLOW_ERR_TYPE_DIV_BY_ZERO, "Division by Zero Error") \
	X(GLOW_ERR_TYPE_NO_MT,       "Multithreading Error")

#define X(a, b) a,
typedef enum {
	GLOW_ERR_TYPE_LIST
} GlowErrorType;
#undef X

extern const char *glow_err_type_headers[];

struct glow_traceback_stack_item {
	const char *fn;
	unsigned int lineno;
};

struct glow_traceback_manager {
	struct glow_traceback_stack_item *tb;
	size_t tb_count;
	size_t tb_cap;
};

void glow_tb_manager_init(struct glow_traceback_manager *tbm);
void glow_tb_manager_add(struct glow_traceback_manager *tbm,
                        const char *fn,
                        const unsigned int lineno);
void glow_tb_manager_print(struct glow_traceback_manager *tbm, FILE *out);
void glow_tb_manager_dealloc(struct glow_traceback_manager *tbm);

typedef struct glow_error {
	GlowErrorType type;
	char msg[1024];
	struct glow_traceback_manager tbm;
} GlowError;

GlowError *glow_err_new(GlowErrorType type, const char *msg_format, ...);
void glow_err_free(GlowError *error);
void glow_err_traceback_append(GlowError *error,
                                const char *fn,
                                const unsigned int lineno);
void glow_err_traceback_print(GlowError *error, FILE *out);

GlowError *glow_err_invalid_file_signature_error(const char *module);
GlowError *glow_err_unbound(const char *var);
GlowError *glow_type_err_invalid_catch(const GlowClass *c1);
GlowError *glow_type_err_invalid_throw(const GlowClass *c1);
GlowError *glow_err_div_by_zero(void);
GlowError *glow_err_multithreading_not_supported(void);

void glow_err_print_msg(GlowError *e, FILE *out);

#define GLOW_INTERNAL_ERROR() (assert(0))

/*
 * Syntax error message header. First format specifier
 * corresponds to the file, second corresponds to the
 * line number.
 */
#define GLOW_SYNTAX_ERROR "%s:%d: syntax error:"

const char *glow_err_on_char(const char *culprit,
                            const char *code,
                            const char *end,
                            unsigned int target_line);

#endif /* GLOW_ERR_H */
