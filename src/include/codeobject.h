#ifndef GLOW_CODEOBJECT_H
#define GLOW_CODEOBJECT_H

#include <stdbool.h>
#include "code.h"
#include "object.h"
#include "str.h"

struct glow_vm;
struct glow_frame;

extern GlowClass glow_co_class;

struct glow_code_cache {
	/* line number cache */
	unsigned int lineno;
};

typedef struct {
	GlowObject base;

	/* name of this code object */
	const char *name;

	/* code segment */
	byte *bc;

	/* number of arguments */
	unsigned int argcount;

	/* max value stack depth */
	unsigned int stack_depth;

	/* max try-catch depth */
	unsigned int try_catch_depth;

	/* enumerated variable names */
	struct glow_str_array names;

	/* enumerated attributes used */
	struct glow_str_array attrs;

	/* enumerated free variables */
	struct glow_str_array frees;

	/* enumerated constants */
	struct glow_value_array consts;

	/* type hints */
	GlowClass **hints;

	/* line number table */
	byte *lno_table;

	/* first line number */
	unsigned int first_lineno;

	/* virtual machine associated with this code object */
	struct glow_vm *vm;

	/* caches */
	struct glow_frame *frame;
	struct glow_code_cache *cache;
} GlowCodeObject;

GlowCodeObject *glow_codeobj_make(GlowCode *code,
                                const char *name,
                                unsigned int argcount,
                                int stack_depth,
                                int try_catch_depth,
                                struct glow_vm *vm);

/*
 * The stack and try-catch depths must be read out of the given
 * code at the top level.
 */
GlowCodeObject *glow_codeobj_make_toplevel(GlowCode *code,
                                         const char *name,
                                         struct glow_vm *vm);

GlowValue glow_codeobj_load_args(GlowCodeObject *co,
                               struct glow_value_array *default_args,
                               GlowValue *args,
                               GlowValue *args_named,
                               size_t nargs,
                               size_t nargs_named,
                               GlowValue *locals);

GlowValue glow_codeobj_init_hints(GlowCodeObject *co, GlowValue *types);

#define GLOW_CODEOBJ_NUM_HINTS(co) (((co)->hints != NULL) ? ((co)->argcount + 1) : 0)
#define GLOW_CODEOBJ_RET_HINT(co)  (((co)->hints != NULL) ? ((co)->hints[(co)->argcount]) : NULL)

#endif /* GLOW_CODEOBJECT_H */
