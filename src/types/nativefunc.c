#include <stdlib.h>
#include "object.h"
#include "util.h"
#include "exc.h"
#include "nativefunc.h"

static void nativefunc_free(GlowValue *nfunc)
{
	/*
	 * Purposefully do not call super-destructor,
	 * since NativeFuncObjects should be statically
	 * allocated.
	 */
	GLOW_UNUSED(nfunc);
}

static GlowValue nativefunc_call(GlowValue *this,
                             GlowValue *args,
                             GlowValue *args_named,
                             size_t nargs,
                             size_t nargs_named)
{
	GLOW_UNUSED(args_named);

	if (nargs_named > 0) {
		return glow_call_exc_native_named_args();
	}

	GlowNativeFuncObject *nfunc = glow_objvalue(this);
	return nfunc->func(args, nargs);
}

GlowClass glow_native_func_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "NativeFunction",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowNativeFuncObject),

	.init = NULL,
	.del = nativefunc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = nativefunc_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
