#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "codeobject.h"
#include "funcobject.h"
#include "iter.h"
#include "exc.h"
#include "vm.h"
#include "util.h"
#include "generator.h"

GlowValue glow_gen_proxy_make(GlowCodeObject *co)
{
	GlowGeneratorProxy *gp = glow_obj_alloc(&glow_gen_proxy_class);
	glow_retaino(co);
	gp->co = co;
	gp->defaults = (struct glow_value_array){.array = NULL, .length = 0};
	return glow_makeobj(gp);
}

GlowValue glow_gen_make(GlowGeneratorProxy *gp)
{
	GlowGeneratorObject *go = glow_obj_alloc(&glow_gen_class);
	GLOW_INIT_SAVED_TID_FIELD(go);

	GlowCodeObject *co = gp->co;
	GlowFrame *frame = glow_frame_make(co);
	frame->persistent = 1;

	glow_retaino(co);
	go->co = gp->co;
	go->frame = frame;
	return glow_makeobj(go);
}

static void release_defaults(GlowGeneratorProxy *gp);

static void gen_proxy_free(GlowValue *this)
{
	GlowGeneratorProxy *gp = glow_objvalue(this);
	glow_releaseo(gp->co);
	release_defaults(gp);
	glow_obj_class.del(this);
}

static void gen_free(GlowValue *this)
{
	GlowGeneratorObject *go = glow_objvalue(this);
	glow_releaseo(go->co);
	glow_frame_free(go->frame);
	glow_obj_class.del(this);
}

void glow_gen_proxy_init_defaults(GlowGeneratorProxy *gp, GlowValue *defaults, const size_t n_defaults)
{
	release_defaults(gp);
	gp->defaults.array = glow_malloc(n_defaults * sizeof(GlowValue));
	gp->defaults.length = n_defaults;
	for (size_t i = 0; i < n_defaults; i++) {
		gp->defaults.array[i] = defaults[i];
		glow_retain(&defaults[i]);
	}
}

static void release_defaults(GlowGeneratorProxy *gp)
{
	GlowValue *defaults = gp->defaults.array;

	if (defaults == NULL) {
		return;
	}

	const unsigned int n_defaults = gp->defaults.length;
	for (size_t i = 0; i < n_defaults; i++) {
		glow_release(&defaults[i]);
	}

	free(defaults);
	gp->defaults = (struct glow_value_array){.array = NULL, .length = 0};
}

static GlowValue gen_proxy_call(GlowValue *this,
                               GlowValue *args,
                               GlowValue *args_named,
                               size_t nargs,
                               size_t nargs_named)
{
	GlowGeneratorProxy *gp = glow_objvalue(this);
	GlowValue gen_v = glow_gen_make(gp);
	GlowGeneratorObject *go = glow_objvalue(&gen_v);
	GlowCodeObject *co = gp->co;
	GlowFrame *frame = go->frame;
	GlowValue status = glow_codeobj_load_args(co,
	                                        &gp->defaults,
	                                        args,
	                                        args_named,
	                                        nargs,
	                                        nargs_named,
	                                        frame->locals);

	if (glow_iserror(&status)) {
		gen_free(&gen_v);
		return status;
	}

	return glow_makeobj(go);
}

static GlowValue gen_iter(GlowValue *this)
{
	GlowGeneratorObject *go = glow_objvalue(this);
	glow_retaino(go);
	return *this;
}

static GlowValue gen_iternext(GlowValue *this)
{
	GlowGeneratorObject *go = glow_objvalue(this);
	GLOW_CHECK_THREAD(go);

	if (go->frame == NULL) {
		return glow_get_iter_stop();
	}

	GlowCodeObject *co = go->co;
	GlowFrame *frame = go->frame;
	GlowVM *vm = glow_current_vm_get();

	glow_retaino(co);
	frame->co = co;

	glow_vm_push_frame_direct(vm, frame);
	glow_vm_eval_frame(vm);
	GlowValue res = frame->return_value;
	glow_vm_pop_frame(vm);

	if (glow_iserror(&res) || glow_is_iter_stop(&res)) {
		glow_frame_free(go->frame);
		go->frame = NULL;
	}

	return res;
}

GlowClass glow_gen_proxy_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "GeneratorProxy",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowGeneratorProxy),

	.init = NULL,
	.del = gen_proxy_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = gen_proxy_call,

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

GlowClass glow_gen_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Generator",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowGeneratorObject),

	.init = NULL,
	.del = gen_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = gen_iter,
	.iternext = gen_iternext,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
