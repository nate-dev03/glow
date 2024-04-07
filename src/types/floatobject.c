#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "floatobject.h"

#define TYPE_ERR_STR(op) "invalid operator types for operator " #op "."

#define FLOAT_BINOP_FUNC_BODY(op) \
	if (glow_isint(other)) { \
		return glow_makefloat(glow_floatvalue(this) op glow_intvalue(other)); \
	} else if (glow_isfloat(other)) { \
		return glow_makefloat(glow_floatvalue(this) op glow_floatvalue(other)); \
	} else { \
		return glow_makeut(); \
	}

#define FLOAT_IBINOP_FUNC_BODY(op) \
	if (glow_isint(other)) { \
		glow_floatvalue(this) = glow_floatvalue(this) op glow_intvalue(other); \
		return *this; \
	} else if (glow_isfloat(other)) { \
		glow_floatvalue(this) = glow_floatvalue(this) op glow_floatvalue(other); \
		return *this; \
	} else { \
		return glow_makeut(); \
	}

static GlowValue float_eq(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other)) {
		return glow_makebool(glow_floatvalue(this) == glow_intvalue(other));
	} else if (glow_isfloat(other)) {
		return glow_makebool(glow_floatvalue(this) == glow_floatvalue(other));
	} else {
		return glow_makefalse();
	}
}

static GlowValue float_hash(GlowValue *this)
{
	return glow_makeint(glow_util_hash_double(glow_floatvalue(this)));
}

static GlowValue float_cmp(GlowValue *this, GlowValue *other)
{
	const double x = glow_floatvalue(this);
	if (glow_isint(other)) {
		const long y = glow_intvalue(other);
		return glow_makeint((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else if (glow_isfloat(other)) {
		const double y = glow_floatvalue(other);
		return glow_makeint((x < y) ? -1 : ((x == y) ? 0 : 1));
	} else {
		return glow_makeut();
	}
}

static GlowValue float_plus(GlowValue *this)
{
	return *this;
}

static GlowValue float_minus(GlowValue *this)
{
	return glow_makefloat(-glow_floatvalue(this));
}

static GlowValue float_abs(GlowValue *this)
{
	return glow_makefloat(fabs(glow_floatvalue(this)));
}

static GlowValue float_add(GlowValue *this, GlowValue *other)
{
	FLOAT_BINOP_FUNC_BODY(+)
}

static GlowValue float_sub(GlowValue *this, GlowValue *other)
{
	FLOAT_BINOP_FUNC_BODY(-)
}

static GlowValue float_mul(GlowValue *this, GlowValue *other)
{
	FLOAT_BINOP_FUNC_BODY(*)
}

static GlowValue float_div(GlowValue *this, GlowValue *other)
{
	FLOAT_BINOP_FUNC_BODY(/)
}

static GlowValue float_pow(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other)) {
		return glow_makefloat(pow(glow_floatvalue(this), glow_intvalue(other)));
	} else if (glow_isfloat(other)) {
		return glow_makefloat(pow(glow_floatvalue(this), glow_floatvalue(other)));
	} else {
		return glow_makeut();
	}
}

static GlowValue float_iadd(GlowValue *this, GlowValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(+)
}

static GlowValue float_isub(GlowValue *this, GlowValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(-)
}

static GlowValue float_imul(GlowValue *this, GlowValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(*)
}

static GlowValue float_idiv(GlowValue *this, GlowValue *other)
{
	FLOAT_IBINOP_FUNC_BODY(/)
}

static GlowValue float_ipow(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other)) {
		glow_floatvalue(this) = pow(glow_floatvalue(this), glow_intvalue(other));
		return *this;
	} else if (glow_isfloat(other)) {
		glow_floatvalue(this) = pow(glow_floatvalue(this), glow_floatvalue(other));
		return *this;
	} else {
		return glow_makeut();
	}
}

static bool float_nonzero(GlowValue *this)
{
	return glow_floatvalue(this) != 0;
}

static GlowValue float_to_int(GlowValue *this)
{
	return glow_makeint(glow_floatvalue(this));
}

static GlowValue float_to_float(GlowValue *this)
{
	return *this;
}

static GlowValue float_str(GlowValue *this)
{
	char buf[32];
	const double d = glow_floatvalue(this);
	int len = snprintf(buf, sizeof(buf), "%f", d);
	assert(0 < len && (size_t)len < sizeof(buf));

	return glow_strobj_make_direct(buf, len);
}

struct glow_num_methods glow_float_num_methods = {
	float_plus,    /* plus */
	float_minus,    /* minus */
	float_abs,    /* abs */

	float_add,    /* add */
	float_sub,    /* sub */
	float_mul,    /* mul */
	float_div,    /* div */
	NULL,    /* mod */
	float_pow,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	float_iadd,    /* iadd */
	float_isub,    /* isub */
	float_imul,    /* imul */
	float_idiv,    /* idiv */
	NULL,    /* imod */
	float_ipow,    /* ipow */

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

	float_nonzero,    /* nonzero */

	float_to_int,    /* to_int */
	float_to_float,    /* to_float */
};

GlowClass glow_float_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Float",
	.super = &glow_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = float_eq,
	.hash = float_hash,
	.cmp = float_cmp,
	.str = float_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &glow_float_num_methods,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
