#ifndef GLOW_FUNCOBJECT_H
#define GLOW_FUNCOBJECT_H

#include <stdlib.h>
#include "object.h"
#include "codeobject.h"

extern GlowClass glow_fn_class;

typedef struct {
	GlowObject base;
	GlowCodeObject *co;

	/* default arguments */
	struct glow_value_array defaults;
} GlowFuncObject;

GlowValue glow_funcobj_make(GlowCodeObject *co);

void glow_funcobj_init_defaults(GlowFuncObject *co, GlowValue *defaults, const size_t n_defaults);

#endif /* GLOW_FUNCOBJECT_H */
