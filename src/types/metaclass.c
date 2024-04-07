#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "util.h"
#include "exc.h"
#include "object.h"
#include "strobject.h"
#include "metaclass.h"

static void meta_class_del(GlowValue *this)
{
	GLOW_UNUSED(this);
}

static GlowValue meta_class_str(GlowValue *this)
{
#define STR_MAX_LEN 50
	char buf[STR_MAX_LEN];
	GlowClass *class = glow_objvalue(this);
	size_t len = snprintf(buf, STR_MAX_LEN, "<class %s>", class->name);
	assert(len > 0);

	if (len > STR_MAX_LEN) {
		len = STR_MAX_LEN;
	}

	return glow_strobj_make_direct(buf, len);
#undef STR_MAX_LEN
}

static GlowValue meta_class_call(GlowValue *this,
                                GlowValue *args,
                                GlowValue *args_named,
                                size_t nargs,
                                size_t nargs_named)
{
	GLOW_UNUSED(args_named);

	if (nargs_named > 0) {
		return glow_call_exc_constructor_named_args();
	}

	GlowClass *class = glow_objvalue(this);
	GlowInitFunc init = glow_resolve_init(class);

	if (!init) {
		return glow_type_exc_cannot_instantiate(class);
	}

	GlowValue instance = glow_makeobj(glow_obj_alloc(class));
	GlowValue init_result = init(&instance, args, nargs);

	if (glow_iserror(&init_result)) {
		free(glow_objvalue(&instance));  /* straight-up free; no need to
		                                   go through `release` since we can
		                                   be sure nobody has a reference to
		                                   the newly created instance        */
		return init_result;
	} else {
		return instance;
	}
}

GlowClass glow_meta_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "MetaClass",
	.super = &glow_meta_class,

	.instance_size = sizeof(GlowClass),

	.init = NULL,
	.del = meta_class_del,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = meta_class_str,
	.call = meta_class_call,

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
