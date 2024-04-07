#ifndef GLOW_GENERATOR_H
#define GLOW_GENERATOR_H

#include <stdlib.h>
#include "object.h"
#include "codeobject.h"
#include "vm.h"

extern GlowClass glow_gen_proxy_class;
extern GlowClass glow_gen_class;

typedef struct {
	GlowObject base;
	GlowCodeObject *co;
	struct glow_value_array defaults;
} GlowGeneratorProxy;

typedef struct {
	GlowObject base;
	GlowCodeObject *co;
	GlowFrame *frame;
	GLOW_SAVED_TID_FIELD
} GlowGeneratorObject;

GlowValue glow_gen_proxy_make(GlowCodeObject *co);
GlowValue glow_gen_make(GlowGeneratorProxy *gp);
void glow_gen_proxy_init_defaults(GlowGeneratorProxy *gp, GlowValue *defaults, const size_t n_defaults);

#endif /* GLOW_GENERATOR_H */
