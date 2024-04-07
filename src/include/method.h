#ifndef GLOW_METHOD_H
#define GLOW_METHOD_H

#include "object.h"
#include "attr.h"

extern GlowClass glow_method_class;

typedef struct {
	GlowObject base;
	GlowValue binder;
	MethodFunc method;
} GlowMethod;

GlowValue glow_methobj_make(GlowValue *binder, MethodFunc meth_func);

#endif /* GLOW_METHOD_H */
