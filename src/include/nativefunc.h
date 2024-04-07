#ifndef GLOW_NATIVEFUNC_H
#define GLOW_NATIVEFUNC_H

#include "object.h"

/*
 * Note: NativeFuncObjects should be statically allocated only.
 */

extern GlowClass glow_native_func_class;

typedef GlowValue (*GlowNativeFunc)(GlowValue *args, size_t nargs);

typedef struct {
	GlowObject base;
	GlowNativeFunc func;
} GlowNativeFuncObject;

#define GLOW_NFUNC_INIT(func_) { .base = GLOW_OBJ_INIT_STATIC(&glow_native_func_class), .func = (func_) }

#endif /* GLOW_NATIVEFUNC_H */
