#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "tupleobject.h"

#define INDEX_CHECK(index, count) \
	if ((index) < 0 || ((size_t)(index)) >= (count)) { \
		return GLOW_INDEX_EXC("tuple index out of range (index = %li, len = %lu)", (index), (count)); \
	}

/* Does not retain elements; direct transfer from value stack. */
GlowValue glow_tuple_make(GlowValue *elements, const size_t count)
{
	const size_t extra_size = count * sizeof(GlowValue);
	GlowTupleObject *tup = glow_obj_alloc_var(&glow_tuple_class, extra_size);
	memcpy(tup->elements, elements, extra_size);
	tup->count = count;
	return glow_makeobj(tup);
}

static GlowValue tuple_str(GlowValue *this)
{
	GlowTupleObject *tup = glow_objvalue(this);
	const size_t count = tup->count;

	if (count == 0) {
		return glow_strobj_make_direct("()", 2);
	}

	GlowStrBuf sb;
	glow_strbuf_init(&sb, 16);
	glow_strbuf_append(&sb, "(", 1);

	GlowValue *elements = tup->elements;

	for (size_t i = 0; i < count; i++) {
		GlowValue *v = &elements[i];
		if (glow_isobject(v) && glow_objvalue(v) == tup) {  // this should really never happen
			glow_strbuf_append(&sb, "(...)", 5);
		} else {
			GlowValue str_v = glow_op_str(v);

			if (glow_iserror(&str_v)) {
				glow_strbuf_dealloc(&sb);
				return str_v;
			}

			GlowStrObject *str = glow_objvalue(&str_v);
			glow_strbuf_append(&sb, str->str.value, str->str.len);
			glow_releaseo(str);
		}

		if (i < count - 1) {
			glow_strbuf_append(&sb, ", ", 2);
		} else {
			glow_strbuf_append(&sb, ")", 1);
			break;
		}
	}

	GlowStr dest;
	glow_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	return glow_strobj_make(dest);
}

static void tuple_free(GlowValue *this)
{
	GlowTupleObject *tup = glow_objvalue(this);
	GlowValue *elements = tup->elements;
	const size_t count = tup->count;

	for (size_t i = 0; i < count; i++) {
		glow_release(&elements[i]);
	}

	glow_obj_class.del(this);
}

static GlowValue tuple_len(GlowValue *this)
{
	GlowTupleObject *tup = glow_objvalue(this);
	return glow_makeint(tup->count);
}

static GlowValue tuple_get(GlowValue *this, GlowValue *idx)
{
	if (!glow_isint(idx)) {
		return GLOW_TYPE_EXC("list indices must be integers, not %s instances", glow_getclass(idx)->name);
	}

	GlowTupleObject *tup = glow_objvalue(this);
	const size_t count = tup->count;
	const long idx_raw = glow_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	glow_retain(&tup->elements[idx_raw]);
	return tup->elements[idx_raw];
}

struct glow_num_methods glow_tuple_num_methods = {
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

struct glow_seq_methods glow_tuple_seq_methods = {
	tuple_len,    /* len */
	tuple_get,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_tuple_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Tuple",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowTupleObject),  /* variable-length */

	.init = NULL,
	.del = tuple_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = tuple_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &glow_tuple_num_methods,
	.seq_methods = &glow_tuple_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
