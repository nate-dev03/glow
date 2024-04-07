#include <stdlib.h>
#include "nativefunc.h"
#include "object.h"
#include "strobject.h"
#include "vmops.h"
#include "strdict.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"

static GlowValue hash(GlowValue *args, size_t nargs);
static GlowValue str(GlowValue *args, size_t nargs);
static GlowValue len(GlowValue *args, size_t nargs);
static GlowValue iter(GlowValue *args, size_t nargs);
static GlowValue next(GlowValue *args, size_t nargs);
static GlowValue type(GlowValue *args, size_t nargs);
static GlowValue safe(GlowValue *args, size_t nargs);

static GlowNativeFuncObject hash_nfo = GLOW_NFUNC_INIT(hash);
static GlowNativeFuncObject str_nfo  = GLOW_NFUNC_INIT(str);
static GlowNativeFuncObject len_nfo  = GLOW_NFUNC_INIT(len);
static GlowNativeFuncObject iter_nfo = GLOW_NFUNC_INIT(iter);
static GlowNativeFuncObject next_nfo = GLOW_NFUNC_INIT(next);
static GlowNativeFuncObject type_nfo = GLOW_NFUNC_INIT(type);
static GlowNativeFuncObject safe_nfo = GLOW_NFUNC_INIT(safe);

const struct glow_builtin glow_builtins[] = {
		{"hash", GLOW_MAKE_OBJ(&hash_nfo)},
		{"str",  GLOW_MAKE_OBJ(&str_nfo)},
		{"len",  GLOW_MAKE_OBJ(&len_nfo)},
		{"iter", GLOW_MAKE_OBJ(&iter_nfo)},
		{"next", GLOW_MAKE_OBJ(&next_nfo)},
		{"type", GLOW_MAKE_OBJ(&type_nfo)},
		{"safe", GLOW_MAKE_OBJ(&safe_nfo)},
		{NULL,   GLOW_MAKE_EMPTY()},
};

static GlowValue hash(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("hash", nargs, 1);
	return glow_op_hash(&args[0]);
}

static GlowValue str(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("str", nargs, 1);
	return glow_op_str(&args[0]);
}

static GlowValue len(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("len", nargs, 1);
	GlowClass *class = glow_getclass(&args[0]);
	GlowUnOp len = glow_resolve_len(class);

	if (!len) {
		return glow_type_exc_unsupported_1(__func__, class);
	}

	return len(&args[0]);
}

static GlowValue iter(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("iter", nargs, 1);
	return glow_op_iter(args);
}

static GlowValue next(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("next", nargs, 1);
	return glow_op_iternext(args);
}

static GlowValue type(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("type", nargs, 1);
	return glow_makeobj(glow_getclass(&args[0]));
}

static GlowValue safe(GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK("safe", nargs, 1);

	if (!glow_isobject(args)) {
		glow_retain(args);
		return *args;
	}

	GlowObject *o = glow_objvalue(args);

	if (glow_object_set_monitor(o)) {
		glow_retain(args);
		return *args;
	} else {
		return GLOW_CONC_ACCS_EXC("safe() argument already safe'd or has multiple references to it");
	}
}

/* Built-in modules */
#include "iomodule.h"
#include "mathmodule.h"

const GlowModule *glow_builtin_modules[] = {
		(GlowModule *)&glow_io_module,
		(GlowModule *)&glow_math_module,
		NULL
};
