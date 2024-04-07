#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "boolobject.h"

static GlowValue bool_eq(GlowValue *this, GlowValue *other)
{
	if (!glow_isbool(other)) {
		return glow_makefalse();
	}

	return glow_makebool(glow_boolvalue(this) == glow_boolvalue(other));
}

static GlowValue bool_hash(GlowValue *this)
{
	return glow_makeint(glow_util_hash_bool(glow_boolvalue(this)));
}

static GlowValue bool_cmp(GlowValue *this, GlowValue *other)
{
	if (!glow_isbool(other)) {
		return glow_makeut();
	}

	const bool b1 = glow_boolvalue(this);
	const bool b2 = glow_boolvalue(other);
	return glow_makeint((b1 == b2) ? 0 : (b1 ? 1 : -1));
}

static bool bool_nonzero(GlowValue *this)
{
	return glow_boolvalue(this);
}

static GlowValue bool_to_int(GlowValue *this)
{
	return glow_makeint(glow_boolvalue(this) ? 1 : 0);
}

static GlowValue bool_to_float(GlowValue *this)
{
	return glow_makefloat(glow_boolvalue(this) ? 1.0 : 0.0);
}

static GlowValue bool_str(GlowValue *this)
{
	return glow_boolvalue(this) ? glow_strobj_make_direct("true", 4) :
	                             glow_strobj_make_direct("false", 5);
}

struct glow_num_methods glow_bool_num_methods = {
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

	bool_nonzero,    /* nonzero */

	bool_to_int,    /* to_int */
	bool_to_float,    /* to_float */
};

GlowClass glow_bool_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Bool",
	.super = &glow_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = bool_eq,
	.hash = bool_hash,
	.cmp = bool_cmp,
	.str = bool_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &glow_bool_num_methods,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
