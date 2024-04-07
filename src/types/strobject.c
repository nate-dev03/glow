#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "attr.h"
#include "exc.h"
#include "str.h"
#include "object.h"
#include "util.h"
#include "strobject.h"

GlowValue glow_strobj_make(GlowStr value)
{
	GlowStrObject *s = glow_obj_alloc(&glow_str_class);
	s->freeable = value.freeable;
	value.freeable = 0;
	s->str = value;
	return glow_makeobj(s);
}

GlowValue glow_strobj_make_direct(const char *value, const size_t len)
{
	GlowStrObject *s = glow_obj_alloc(&glow_str_class);
	char *copy = glow_malloc(len + 1);
	memcpy(copy, value, len);
	copy[len] = '\0';
	s->str = GLOW_STR_INIT(copy, len, 0);
	s->freeable = 1;
	return glow_makeobj(s);
}

static GlowValue strobj_eq(GlowValue *this, GlowValue *other)
{
	if (!glow_is_a(other, &glow_str_class)) {
		return glow_makefalse();
	}

	GlowStrObject *s1 = glow_objvalue(this);
	GlowStrObject *s2 = glow_objvalue(other);
	return glow_makebool(glow_str_eq(&s1->str, &s2->str));
}

static GlowValue strobj_cmp(GlowValue *this, GlowValue *other)
{
	if (!glow_is_a(other, &glow_str_class)) {
		return glow_makeut();
	}

	GlowStrObject *s1 = glow_objvalue(this);
	GlowStrObject *s2 = glow_objvalue(other);
	return glow_makeint(glow_str_cmp(&s1->str, &s2->str));
}

static GlowValue strobj_hash(GlowValue *this)
{
	GlowStrObject *s = glow_objvalue(this);
	return glow_makeint(glow_str_hash(&s->str));
}

static bool strobj_nonzero(GlowValue *this)
{
	GlowStrObject *s = glow_objvalue(this);
	return (s->str.len != 0);
}

static void strobj_free(GlowValue *this)
{
	GlowStrObject *s = glow_objvalue(this);
	if (s->freeable) {
		GLOW_FREE(s->str.value);
	}

	s->base.class->super->del(this);
}

static GlowValue strobj_str(GlowValue *this)
{
	GlowStrObject *s = glow_objvalue(this);
	glow_retaino(s);
	return *this;
}

static GlowValue strobj_cat(GlowValue *this, GlowValue *other)
{
	if (!glow_is_a(other, &glow_str_class)) {
		return glow_makeut();
	}

	GlowStr *s1 = &((GlowStrObject *) glow_objvalue(this))->str;
	GlowStr *s2 = &((GlowStrObject *) glow_objvalue(other))->str;

	const size_t len1 = s1->len;
	const size_t len2 = s2->len;
	const size_t len_cat = len1 + len2;

	char *cat = glow_malloc(len_cat + 1);

	for (size_t i = 0; i < len1; i++) {
		cat[i] = s1->value[i];
	}

	for (size_t i = 0; i < len2; i++) {
		cat[i + len1] = s2->value[i];
	}

	cat[len_cat] = '\0';

	return glow_strobj_make(GLOW_STR_INIT(cat, len_cat, 1));
}

static GlowValue strobj_len(GlowValue *this)
{
	GlowStrObject *s = glow_objvalue(this);
	return glow_makeint(s->str.len);
}

struct glow_num_methods glow_str_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	strobj_cat,    /* add */
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

	strobj_nonzero,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct glow_seq_methods glow_str_seq_methods = {
	strobj_len,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_str_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Str",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowStrObject),

	.init = NULL,
	.del = strobj_free,

	.eq = strobj_eq,
	.hash = strobj_hash,
	.cmp = strobj_cmp,
	.str = strobj_str,
	.call = NULL,

	.num_methods = &glow_str_num_methods,
	.seq_methods = &glow_str_seq_methods,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
