#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "listobject.h"

static GlowValue iter_make(GlowListObject *list);

#define INDEX_CHECK(index, count) \
	if ((index) < 0 || ((size_t)(index)) >= (count)) { \
		return GLOW_INDEX_EXC("list index out of range (index = %li, len = %lu)", (index), (count)); \
	}

static void list_ensure_capacity(GlowListObject *list, const size_t min_capacity);

/* Does not retain elements; direct transfer from value stack. */
GlowValue glow_list_make(GlowValue *elements, const size_t count)
{
	GlowListObject *list = glow_obj_alloc(&glow_list_class);
	GLOW_INIT_SAVED_TID_FIELD(list);

	const size_t size = count * sizeof(GlowValue);
	list->elements = glow_malloc(size);
	memcpy(list->elements, elements, size);

	list->count = count;
	list->capacity = count;

	return glow_makeobj(list);
}

static GlowValue list_str(GlowValue *this)
{
	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);

	const size_t count = list->count;

	if (count == 0) {
		GLOW_EXIT(list);
		return glow_strobj_make_direct("[]", 2);
	}

	GlowStrBuf sb;
	glow_strbuf_init(&sb, 16);
	glow_strbuf_append(&sb, "[", 1);

	GlowValue *elements = list->elements;

	for (size_t i = 0; i < count; i++) {
		GlowValue *v = &elements[i];
		if (glow_isobject(v) && glow_objvalue(v) == list) {
			glow_strbuf_append(&sb, "[...]", 5);
		} else {
			GlowValue str_v = glow_op_str(v);

			if (glow_iserror(&str_v)) {
				glow_strbuf_dealloc(&sb);
				GLOW_EXIT(list);
				return str_v;
			}

			GlowStrObject *str = glow_objvalue(&str_v);
			glow_strbuf_append(&sb, str->str.value, str->str.len);
			glow_releaseo(str);
		}

		if (i < count - 1) {
			glow_strbuf_append(&sb, ", ", 2);
		} else {
			glow_strbuf_append(&sb, "]", 1);
			break;
		}
	}

	GlowStr dest;
	glow_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	GLOW_EXIT(list);
	return glow_strobj_make(dest);
}

static void list_free(GlowValue *this)
{
	GlowListObject *list = glow_objvalue(this);
	GlowValue *elements = list->elements;
	const size_t count = list->count;

	for (size_t i = 0; i < count; i++) {
		glow_release(&elements[i]);
	}

	free(elements);
	glow_obj_class.del(this);
}

static GlowValue list_len(GlowValue *this)
{
	GlowListObject *list = glow_objvalue(this);
	return glow_makeint(list->count);
}

GlowValue glow_list_get(GlowListObject *list, const size_t idx)
{
	GlowValue v = list->elements[idx];
	glow_retain(&v);
	return v;
}

static GlowValue list_get(GlowValue *this, GlowValue *idx)
{
	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);

	if (!glow_isint(idx)) {
		GLOW_EXIT(list);
		GlowClass *class = glow_getclass(idx);
		return GLOW_TYPE_EXC("list indices must be integers, not %s instances", class->name);
	}

	const size_t count = list->count;
	const long idx_raw = glow_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	GlowValue ret = glow_list_get(list, idx_raw);
	GLOW_EXIT(list);
	return ret;
}

static GlowValue list_set(GlowValue *this, GlowValue *idx, GlowValue *v)
{
	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);

	if (!glow_isint(idx)) {
		GLOW_EXIT(list);
		GlowClass *class = glow_getclass(idx);
		return GLOW_TYPE_EXC("list indices must be integers, not %s instances", class->name);
	}

	const size_t count = list->count;
	const long idx_raw = glow_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	GlowValue old = list->elements[idx_raw];
	glow_retain(v);
	list->elements[idx_raw] = *v;
	GLOW_EXIT(list);
	return old;
}

static GlowValue list_apply(GlowValue *this, GlowValue *fn)
{
	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);

	GlowCallFunc call = glow_resolve_call(glow_getclass(fn));  // this should've been checked already

	GlowValue list2_value = glow_list_make(list->elements, list->count);
	GlowListObject *list2 = glow_objvalue(&list2_value);
	GlowValue *elements = list2->elements;
	const size_t count = list2->count;

	for (size_t i = 0; i < count; i++) {
		GlowValue r = call(fn, &elements[i], NULL, 1, 0);

		if (glow_iserror(&r)) {
			list2->count = i;
			list_free(&glow_makeobj(list2));
			GLOW_EXIT(list);
			return r;
		}

		elements[i] = r;
	}

	GLOW_EXIT(list);
	return glow_makeobj(list2);
}

void glow_list_append(GlowListObject *list, GlowValue *v)
{
	const size_t count = list->count;
	list_ensure_capacity(list, count + 1);
	glow_retain(v);
	list->elements[list->count++] = *v;
}

static GlowValue list_append(GlowValue *this,
                            GlowValue *args,
                            GlowValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "append"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);
	glow_list_append(list, &args[0]);
	GLOW_EXIT(list);
	return glow_makenull();

#undef NAME
}

static GlowValue list_pop(GlowValue *this,
                         GlowValue *args,
                         GlowValue *args_named,
                         size_t nargs,
                         size_t nargs_named)
{
#define NAME "pop"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK_AT_MOST(NAME, nargs, 1);

	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);

	GlowValue *elements = list->elements;
	const size_t count = list->count;

	if (nargs == 0) {
		GLOW_EXIT(list);
		if (count > 0) {
			return elements[--list->count];
		} else {
			return GLOW_INDEX_EXC("cannot invoke " NAME "() on an empty list");
		}
	} else {
		GlowValue *idx = &args[0];
		if (glow_isint(idx)) {
			const long idx_raw = glow_intvalue(idx);

			INDEX_CHECK(idx_raw, count);

			GlowValue ret = elements[idx_raw];
			memmove(&elements[idx_raw],
			        &elements[idx_raw + 1],
			        ((count - 1) - idx_raw) * sizeof(GlowValue));
			--list->count;
			GLOW_EXIT(list);
			return ret;
		} else {
			GLOW_EXIT(list);
			GlowClass *class = glow_getclass(idx);
			return GLOW_TYPE_EXC(NAME "() requires an integer argument (got a %s)", class->name);
		}
	}

#undef NAME
}

static GlowValue list_insert(GlowValue *this,
                            GlowValue *args,
                            GlowValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "insert"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 2);

	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);

	const size_t count = list->count;

	GlowValue *idx = &args[0];
	GlowValue *e = &args[1];

	if (!glow_isint(idx)) {
		GLOW_EXIT(list);
		GlowClass *class = glow_getclass(idx);
		return GLOW_TYPE_EXC(NAME "() requires an integer as its first argument (got a %s)", class->name);
	}

	const long idx_raw = glow_intvalue(idx);

	INDEX_CHECK(idx_raw, count);

	list_ensure_capacity(list, count + 1);
	GlowValue *elements = list->elements;

	memmove(&elements[idx_raw + 1],
	        &elements[idx_raw],
	        (count - idx_raw) * sizeof(GlowValue));

	glow_retain(e);
	elements[idx_raw] = *e;
	++list->count;
	GLOW_EXIT(list);
	return glow_makenull();

#undef NAME
}

static GlowValue list_iter(GlowValue *this)
{
	GlowListObject *list = glow_objvalue(this);
	GLOW_ENTER(list);
	GlowValue iter = iter_make(list);
	GLOW_EXIT(list);
	return iter;
}

static void list_ensure_capacity(GlowListObject *list, const size_t min_capacity)
{
	const size_t capacity = list->capacity;

	if (capacity < min_capacity) {
		size_t new_capacity = (capacity * 3)/2 + 1;

		if (new_capacity < min_capacity) {
			new_capacity = min_capacity;
		}

		list->elements = glow_realloc(list->elements, new_capacity * sizeof(GlowValue));
		list->capacity = new_capacity;
	}
}

void glow_list_clear(GlowListObject *list)
{
	GlowValue *elements = list->elements;
	const size_t count = list->count;

	for (size_t i = 0; i < count; i++) {
		glow_release(&elements[i]);
	}

	list->count = 0;
}

void glow_list_trim(GlowListObject *list)
{
	const size_t new_capacity = list->count;
	list->capacity = new_capacity;
	list->elements = glow_realloc(list->elements, new_capacity * sizeof(GlowValue));
}

struct glow_num_methods glow_list_num_methods = {
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

struct glow_seq_methods glow_list_seq_methods = {
	list_len,    /* len */
	list_get,    /* get */
	list_set,    /* set */
	NULL,    /* contains */
	list_apply,    /* apply */
	NULL,    /* iapply */
};

struct glow_attr_method list_methods[] = {
	{"append", list_append},
	{"pop", list_pop},
	{"insert", list_insert},
	{NULL, NULL}
};

GlowClass glow_list_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "List",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowListObject),

	.init = NULL,
	.del = list_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = list_str,
	.call = NULL,

	.print = NULL,

	.iter = list_iter,
	.iternext = NULL,

	.num_methods = &glow_list_num_methods,
	.seq_methods = &glow_list_seq_methods,

	.members = NULL,
	.methods = list_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* list iterator */

static GlowValue iter_make(GlowListObject *list)
{
	GlowListIter *iter = glow_obj_alloc(&glow_list_iter_class);
	glow_retaino(list);
	iter->source = list;
	iter->index = 0;
	return glow_makeobj(iter);
}

static GlowValue iter_next(GlowValue *this)
{
	GlowListIter *iter = glow_objvalue(this);
	GLOW_ENTER(iter->source);

	if (iter->index >= iter->source->count) {
		GLOW_EXIT(iter->source);
		return glow_get_iter_stop();
	} else {
		GlowValue v = iter->source->elements[iter->index++];
		GLOW_EXIT(iter->source);
		glow_retain(&v);
		return v;
	}
}

static void iter_free(GlowValue *this)
{
	GlowListIter *iter = glow_objvalue(this);
	glow_releaseo(iter->source);
	glow_iter_class.del(this);
}

struct glow_seq_methods list_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_list_iter_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "ListIter",
	.super = &glow_iter_class,

	.instance_size = sizeof(GlowListIter),

	.init = NULL,
	.del = iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = iter_next,

	.num_methods = NULL,
	.seq_methods = &list_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
