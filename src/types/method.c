#include <stdlib.h>
#include "object.h"
#include "method.h"

GlowValue glow_methobj_make(GlowValue *binder, MethodFunc meth_func)
{
	GlowMethod *meth = glow_obj_alloc(&glow_method_class);
	glow_retain(binder);
	meth->binder = *binder;
	meth->method = meth_func;
	return glow_makeobj(meth);
}

static void methobj_free(GlowValue *this)
{
	GlowMethod *meth = glow_objvalue(this);
	glow_release(&meth->binder);
	glow_obj_class.del(this);
}

static GlowValue methobj_invoke(GlowValue *this,
                               GlowValue *args,
                               GlowValue *args_named,
                               size_t nargs,
                               size_t nargs_named)
{
	GlowMethod *meth = glow_objvalue(this);
	return meth->method(&meth->binder, args, args_named, nargs, nargs_named);
}

struct glow_num_methods meth_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	NULL,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* ibitand */
	NULL,    /* ibitor */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* radd */
	NULL,    /* rsub */
	NULL,    /* rmul */
	NULL,    /* rdiv */
	NULL,    /* rmod */
	NULL,    /* rpow */

	NULL,    /* rbitand */
	NULL,    /* rbitor */
	NULL,    /* rxor */
	NULL,    /* rshiftl */
	NULL,    /* rshiftr */

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct glow_seq_methods meth_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_method_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Method",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowMethod),

	.init = NULL,
	.del = methobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = methobj_invoke,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &meth_num_methods,
	.seq_methods = &meth_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
