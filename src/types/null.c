#include <stdlib.h>
#include <stdbool.h>
#include "util.h"
#include "object.h"
#include "strobject.h"
#include "null.h"

static GlowValue null_str(GlowValue *this)
{
	GLOW_UNUSED(this);
	return glow_strobj_make_direct("null", 4);
}

static GlowValue null_eq(GlowValue *this, GlowValue *other)
{
	GLOW_UNUSED(this);
	return glow_makebool(glow_isnull(other));
}

GlowClass glow_null_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Null",
	.super = &glow_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = null_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = null_str,
	.call = NULL,

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
