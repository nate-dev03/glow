#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "intobject.h"

#define TYPE_ERR_STR(op) "invalid operator types for operator " #op "."

#define INT_BINOP_FUNC_BODY(op) \
	if (glow_isint(other)) { \
		return glow_makeint(glow_intvalue(this) op glow_intvalue(other)); \
	} else if (glow_isfloat(other)) { \
		return glow_makefloat(glow_intvalue(this) op glow_floatvalue(other)); \
	} else { \
		return glow_makeut(); \
	}

#define INT_BINOP_FUNC_BODY_NOFLOAT(op) \
	if (glow_isint(other)) { \
		return glow_makeint(glow_intvalue(this) op glow_intvalue(other)); \
	} else { \
		return glow_makeut(); \
	}

#define INT_IBINOP_FUNC_BODY(op) \
	if (glow_isint(other)) { \
		glow_intvalue(this) = glow_intvalue(this) op glow_intvalue(other); \
		return *this; \
	} else if (glow_isfloat(other)) { \
		this->type = GLOW_VAL_TYPE_FLOAT; \
		glow_floatvalue(this) = glow_intvalue(this) op glow_floatvalue(other); \
		return *this; \
	} else { \
		return glow_makeut(); \
	}

#define INT_IBINOP_FUNC_BODY_NOFLOAT(op) \
	if (glow_isint(other)) { \
		glow_intvalue(this) = glow_intvalue(this) op glow_intvalue(other); \
		return *this; \
	} else { \
		return glow_makeut(); \
	}

static GlowValue int_eq(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other)) {
		return glow_makebool(glow_intvalue(this) == glow_intvalue(other));
	} else if (glow_isfloat(other)) {
		return glow_makebool(glow_intvalue(this) == glow_floatvalue(other));
	} else {
		return glow_makefalse();
	}
}

static GlowValue int_hash(GlowValue *this)
{
	return glow_makeint(glow_util_hash_long(glow_intvalue(this)));
}

static GlowValue int_cmp(GlowValue *this, GlowValue *other)
{
	const long x = glow_intvalue(this);
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

static GlowValue int_plus(GlowValue *this)
{
	return *this;
}

static GlowValue int_minus(GlowValue *this)
{
	return glow_makeint(-glow_intvalue(this));
}

static GlowValue int_abs(GlowValue *this)
{
	return glow_makeint(labs(glow_intvalue(this)));
}

static GlowValue int_add(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY(+)
}

static GlowValue int_sub(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY(-)
}

static GlowValue int_mul(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY(*)
}

static GlowValue int_div(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other) && !glow_intvalue(other)) {
		return glow_makedbz();
	}
	INT_BINOP_FUNC_BODY(/)
}

static GlowValue int_mod(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other) && !glow_intvalue(other)) {
		return glow_makedbz();
	}
	INT_BINOP_FUNC_BODY_NOFLOAT(%)
}

static GlowValue int_pow(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other)) {
		return glow_makeint(pow(glow_intvalue(this), glow_intvalue(other)));
	} else if (glow_isfloat(other)) {
		return glow_makefloat(pow(glow_intvalue(this), glow_floatvalue(other)));
	} else {
		return glow_makeut();
	}
}

static GlowValue int_bitnot(GlowValue *this)
{
	return glow_makeint(~glow_intvalue(this));
}

static GlowValue int_bitand(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(&)
}

static GlowValue int_bitor(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(|)
}

static GlowValue int_xor(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(^)
}

static GlowValue int_shiftl(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(<<)
}

static GlowValue int_shiftr(GlowValue *this, GlowValue *other)
{
	INT_BINOP_FUNC_BODY_NOFLOAT(>>)
}

static GlowValue int_iadd(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY(+)
}

static GlowValue int_isub(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY(-)
}

static GlowValue int_imul(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY(*)
}

static GlowValue int_idiv(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other) && !glow_intvalue(other)) {
		return glow_makedbz();
	}
	INT_IBINOP_FUNC_BODY(/)
}

static GlowValue int_imod(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other) && !glow_intvalue(other)) {
		return glow_makedbz();
	}
	INT_IBINOP_FUNC_BODY_NOFLOAT(%)
}

static GlowValue int_ipow(GlowValue *this, GlowValue *other)
{
	if (glow_isint(other)) {
		glow_intvalue(this) = pow(glow_intvalue(this), glow_intvalue(other));
		return *this;
	} else if (glow_isfloat(other)) {
		this->type = GLOW_VAL_TYPE_FLOAT;
		glow_floatvalue(this) = pow(glow_intvalue(this), glow_floatvalue(other));
		return *this;
	} else {
		return glow_makeut();
	}
}

static GlowValue int_ibitand(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(&)
}

static GlowValue int_ibitor(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(|)
}

static GlowValue int_ixor(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(^)
}

static GlowValue int_ishiftl(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(<<)
}

static GlowValue int_ishiftr(GlowValue *this, GlowValue *other)
{
	INT_IBINOP_FUNC_BODY_NOFLOAT(>>)
}

static bool int_nonzero(GlowValue *this)
{
	return glow_intvalue(this) != 0;
}

static GlowValue int_to_int(GlowValue *this)
{
	return *this;
}

static GlowValue int_to_float(GlowValue *this)
{
	return glow_makefloat(glow_intvalue(this));
}

static GlowValue int_str(GlowValue *this)
{
	char buf[32];
	const long n = glow_intvalue(this);
	int len = snprintf(buf, sizeof(buf), "%ld", n);
	assert(0 < len && (size_t)len < sizeof(buf));

	return  glow_strobj_make_direct(buf, len);
}

struct glow_num_methods glow_int_num_methods = {
	int_plus,    /* plus */
	int_minus,    /* minus */
	int_abs,    /* abs */

	int_add,    /* add */
	int_sub,    /* sub */
	int_mul,    /* mul */
	int_div,    /* div */
	int_mod,    /* mod */
	int_pow,    /* pow */

	int_bitnot,    /* bitnot */
	int_bitand,    /* bitand */
	int_bitor,    /* bitor */
	int_xor,    /* xor */
	int_shiftl,    /* shiftl */
	int_shiftr,    /* shiftr */

	int_iadd,    /* iadd */
	int_isub,    /* isub */
	int_imul,    /* imul */
	int_idiv,    /* idiv */
	int_imod,    /* imod */
	int_ipow,    /* ipow */

	int_ibitand,    /* ibitand */
	int_ibitor,    /* ibitor */
	int_ixor,    /* ixor */
	int_ishiftl,    /* ishiftl */
	int_ishiftr,    /* ishiftr */

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

	int_nonzero,    /* nonzero */

	int_to_int,    /* to_int */
	int_to_float,    /* to_float */
};

GlowClass glow_int_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Int",
	.super = &glow_obj_class,

	.instance_size = 0,

	.init = NULL,
	.del = NULL,

	.eq = int_eq,
	.hash = int_hash,
	.cmp = int_cmp,
	.str = int_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &glow_int_num_methods,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
