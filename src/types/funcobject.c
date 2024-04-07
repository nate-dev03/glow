#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "code.h"
#include "compiler.h"
#include "opcodes.h"
#include "object.h"
#include "strobject.h"
#include "vm.h"
#include "util.h"
#include "exc.h"
#include "err.h"
#include "codeobject.h"
#include "funcobject.h"

static void release_defaults(GlowFuncObject *fn);

GlowValue glow_funcobj_make(GlowCodeObject *co)
{
	GlowFuncObject *fn = glow_obj_alloc(&glow_fn_class);
	glow_retaino(co);
	fn->co = co;
	fn->defaults = (struct glow_value_array){.array = NULL, .length = 0};
	return glow_makeobj(fn);
}

void funcobj_free(GlowValue *this)
{
	GlowFuncObject *fn = glow_objvalue(this);
	release_defaults(fn);
	glow_releaseo(fn->co);
	glow_obj_class.del(this);
}

void glow_funcobj_init_defaults(GlowFuncObject *fn, GlowValue *defaults, const size_t n_defaults)
{
	release_defaults(fn);
	fn->defaults.array = glow_malloc(n_defaults * sizeof(GlowValue));
	fn->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		fn->defaults.array[i] = defaults[i];
		glow_retain(&defaults[i]);
	}
}

static void release_defaults(GlowFuncObject *fn)
{
	GlowValue *defaults = fn->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = fn->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		glow_release(&defaults[i]);
	}

	free(defaults);
	fn->defaults = (struct glow_value_array){.array = NULL, .length = 0};
}

static GlowValue funcobj_call(GlowValue *this,
                             GlowValue *args,
                             GlowValue *args_named,
                             size_t nargs,
                             size_t nargs_named)
{
	GlowFuncObject *fn = glow_objvalue(this);
	GlowCodeObject *co = fn->co;
	GlowVM *vm = glow_current_vm_get();
	const unsigned int argcount = co->argcount;

	GlowValue *locals = glow_calloc(argcount, sizeof(GlowValue));
	GlowValue status = glow_codeobj_load_args(co, &fn->defaults, args, args_named, nargs, nargs_named, locals);

	if (glow_iserror(&status)) {
		free(locals);
		return status;
	}

	glow_retaino(co);
	glow_vm_push_frame(vm, co);
	GlowFrame *top = vm->callstack;
	memcpy(top->locals, locals, argcount * sizeof(GlowValue));
	free(locals);
	glow_vm_eval_frame(vm);
	GlowValue res = top->return_value;
	glow_vm_pop_frame(vm);
	return res;

#undef RELEASE_ALL
}

struct glow_num_methods fn_num_methods = {
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

struct glow_seq_methods fn_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_fn_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "FuncObject",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowFuncObject),

	.init = NULL,
	.del = funcobj_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = funcobj_call,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &fn_num_methods,
	.seq_methods = &fn_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
