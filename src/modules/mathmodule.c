#include <stdlib.h>
#include <math.h>
#include "object.h"
#include "nativefunc.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"
#include "strdict.h"
#include "mathmodule.h"

static GlowValue glow_cos(GlowValue *args, size_t nargs)
{
#define NAME "cos"
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);
	if (!glow_isnumber(&args[0])) {
		return glow_type_exc_unsupported_1(NAME, glow_getclass(&args[0]));
	}
	const double d = glow_floatvalue_force(&args[0]);
	return glow_makefloat(cos(d));
#undef NAME
}

static GlowValue glow_sin(GlowValue *args, size_t nargs)
{
#define NAME "sin"
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);
	if (!glow_isnumber(&args[0])) {
		return glow_type_exc_unsupported_1(NAME, glow_getclass(&args[0]));
	}
	const double d = glow_floatvalue_force(&args[0]);
	return glow_makefloat(sin(d));
#undef NAME
}

static GlowNativeFuncObject cos_nfo = GLOW_NFUNC_INIT(glow_cos);
static GlowNativeFuncObject sin_nfo = GLOW_NFUNC_INIT(glow_sin);

#define PI 3.14159265358979323846
#define E  2.71828182845904523536

const struct glow_builtin math_builtins[] = {
		{"pi",  GLOW_MAKE_FLOAT(PI)},
		{"e",   GLOW_MAKE_FLOAT(E)},
		{"cos", GLOW_MAKE_OBJ(&cos_nfo)},
		{"sin", GLOW_MAKE_OBJ(&sin_nfo)},
		{NULL,  GLOW_MAKE_EMPTY()},
};

GlowBuiltInModule glow_math_module = GLOW_BUILTIN_MODULE_INIT_STATIC("math", &math_builtins[0]);
