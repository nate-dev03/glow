#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "iter.h"
#include "util.h"
#include "setobject.h"

#define EMPTY_SIZE  16
#define LOAD_FACTOR 0.75f

typedef struct glow_set_entry Entry;

static Entry **make_empty_table(const size_t capacity);
static void set_resize(GlowSetObject *set, const size_t new_capacity);
static void set_free_entries(GlowSetObject *set);
static void set_free(GlowValue *this);

GlowValue glow_set_make(GlowValue *elements, const size_t size)
{
	GlowSetObject *set = glow_obj_alloc(&glow_set_class);
	GLOW_INIT_SAVED_TID_FIELD(set);

	const size_t capacity = (size == 0) ? EMPTY_SIZE : glow_smallest_pow_2_at_least(size);

	set->entries = make_empty_table(capacity);
	set->count = 0;
	set->capacity = capacity;
	set->threshold = (size_t)(capacity * LOAD_FACTOR);
	set->state_id = 0;

	for (size_t i = 0; i < size; i++) {
		GlowValue *value = &elements[i];
		GlowValue v = glow_set_add(set, value);

		if (glow_iserror(&v)) {
			GlowValue set_v = glow_makeobj(set);
			set_free(&set_v);

			for (size_t j = i; j < size; j++) {
				glow_release(&elements[j]);
			}

			return v;
		}

		glow_release(value);
	}

	return glow_makeobj(set);
}

static Entry *make_entry(GlowValue *element, const int hash)
{
	Entry *entry = glow_malloc(sizeof(Entry));
	entry->element = *element;
	entry->hash = hash;
	entry->next = NULL;
	return entry;
}

GlowValue glow_set_add(GlowSetObject *set, GlowValue *element)
{
	Entry **entries = set->entries;
	const size_t capacity = set->capacity;
	const GlowValue hash_v = glow_op_hash(element);

	if (glow_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));
	const size_t index = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(element));

	for (Entry *entry = entries[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			GlowValue eq_v = eq(element, &entry->element);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (glow_boolvalue(&eq_v)) {
				return glow_makefalse();
			}
		}
	}

	glow_retain(element);

	Entry *entry = make_entry(element, hash);
	entry->next = entries[index];
	entries[index] = entry;

	if (set->count++ >= set->threshold) {
		const size_t new_capacity = (2 * capacity);
		set_resize(set, new_capacity);
		set->threshold = (size_t)(new_capacity * LOAD_FACTOR);
	}

	++set->state_id;
	return glow_maketrue();
}

GlowValue glow_set_remove(GlowSetObject *set, GlowValue *element)
{
	const GlowValue hash_v = glow_op_hash(element);

	if (glow_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(element));

	Entry **entries = set->entries;
	const size_t idx = hash & (set->capacity - 1);
	Entry *entry = entries[idx];
	Entry *prev = entry;

	while (entry != NULL) {
		Entry *next = entry->next;

		if (hash == entry->hash) {
			GlowValue eq_v = eq(element, &entry->element);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (glow_boolvalue(&eq_v)) {
				if (prev == entry) {
					entries[idx] = next;
				} else {
					prev->next = next;
				}

				glow_release(&entry->element);
				free(entry);
				--set->count;
				++set->state_id;
				return glow_maketrue();
			}
		}

		prev = entry;
		entry = next;
	}

	return glow_makefalse();
}

GlowValue glow_set_contains(GlowSetObject *set, GlowValue *element)
{
	Entry **entries = set->entries;
	const size_t capacity = set->capacity;
	GlowValue hash_v = glow_op_hash(element);

	if (glow_iserror(&hash_v)) {
		glow_release(&hash_v);
		return glow_makefalse();
	}

	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));
	const size_t index = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(element));

	for (Entry *entry = entries[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			GlowValue eq_v = eq(element, &entry->element);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (glow_boolvalue(&eq_v)) {
				return glow_maketrue();
			}
		}
	}

	return glow_makefalse();
}

GlowValue glow_set_eq(GlowSetObject *set, GlowSetObject *other)
{
	if (set->count != other->count) {
		return glow_makefalse();
	}

	GlowSetObject *s1, *s2;

	if (set->capacity < other->capacity) {
		s1 = set;
		s2 = other;
	} else {
		s1 = other;
		s2 = set;
	}

	Entry **entries = s1->entries;
	size_t capacity = s1->capacity;
	for (size_t i = 0; i < capacity; i++) {
		for (Entry *entry = entries[i];
		     entry != NULL;
		     entry = entry->next) {

			GlowValue contains = glow_set_contains(s2, &entry->element);

			if (glow_iserror(&contains)) {
				return contains;
			}

			if (!glow_boolvalue(&contains)) {
				return glow_makefalse();
			}
		}
	}

	return glow_maketrue();
}

size_t glow_set_len(GlowSetObject *set)
{
	return set->count;
}

static Entry **make_empty_table(const size_t capacity)
{
	Entry **table = glow_malloc(capacity * sizeof(Entry *));
	for (size_t i = 0; i < capacity; i++) {
		table[i] = NULL;
	}
	return table;
}

static void set_resize(GlowSetObject *set, const size_t new_capacity)
{
	const size_t old_capacity = set->capacity;
	Entry **old_entries = set->entries;
	Entry **new_entries = make_empty_table(new_capacity);

	for (size_t i = 0; i < old_capacity; i++) {
		Entry *entry = old_entries[i];
		while (entry != NULL) {
			Entry *next = entry->next;
			const size_t idx = entry->hash & (new_capacity - 1);
			entry->next = new_entries[idx];
			new_entries[idx] = entry;
			entry = next;
		}
	}

	free(old_entries);
	set->entries = new_entries;
	set->capacity = new_capacity;
	++set->state_id;
}

static GlowValue set_contains(GlowValue *this, GlowValue *element)
{
	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);
	GlowValue ret = glow_set_contains(set, element);
	GLOW_EXIT(set);
	return ret;
}

static GlowValue set_eq(GlowValue *this, GlowValue *other)
{
	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);

	if (!glow_is_a(other, &glow_set_class)) {
		GLOW_EXIT(set);
		return glow_makefalse();
	}

	GlowSetObject *other_set = glow_objvalue(other);
	GlowValue ret = glow_set_eq(set, other_set);
	GLOW_EXIT(set);
	return ret;
}

static GlowValue set_len(GlowValue *this)
{
	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);
	GlowValue ret = glow_makeint(glow_set_len(set));
	GLOW_EXIT(set);
	return ret;
}

static GlowValue set_str(GlowValue *this)
{
	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);

	const size_t capacity = set->capacity;

	if (set->count == 0) {
		GLOW_EXIT(set);
		return glow_strobj_make_direct("{}", 2);
	}

	GlowStrBuf sb;
	glow_strbuf_init(&sb, 16);
	glow_strbuf_append(&sb, "{", 1);

	Entry **entries = set->entries;

	bool first = true;
	for (size_t i = 0; i < capacity; i++) {
		for (Entry *e = entries[i]; e != NULL; e = e->next) {
			if (!first) {
				glow_strbuf_append(&sb, ", ", 2);
			}
			first = false;

			GlowValue *element = &e->element;

			if (glow_isobject(element) && glow_objvalue(element) == set) {
				glow_strbuf_append(&sb, "{...}", 5);
			} else {
				GlowValue str_v = glow_op_str(element);

				if (glow_iserror(&str_v)) {
					glow_strbuf_dealloc(&sb);
					GLOW_EXIT(set);
					return str_v;
				}

				GlowStrObject *str = glow_objvalue(&str_v);
				glow_strbuf_append(&sb, str->str.value, str->str.len);
				glow_releaseo(str);
			}
		}
	}
	glow_strbuf_append(&sb, "}", 1);

	GlowStr dest;
	glow_strbuf_to_str(&sb, &dest);
	dest.freeable = 1;

	GlowValue ret = glow_strobj_make(dest);
	GLOW_EXIT(set);
	return ret;
}

static GlowValue iter_make(GlowSetObject *set);

static GlowValue set_iter(GlowValue *this)
{
	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);
	GlowValue ret = iter_make(set);
	GLOW_EXIT(set);
	return ret;
}

static GlowValue set_add_method(GlowValue *this,
                               GlowValue *args,
                               GlowValue *args_named,
                               size_t nargs,
                               size_t nargs_named)
{
#define NAME "add"
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);
	GlowValue ret = glow_set_add(set, &args[0]);
	GLOW_EXIT(set);
	return ret;
#undef NAME
}

static GlowValue set_remove_method(GlowValue *this,
                                  GlowValue *args,
                                  GlowValue *args_named,
                                  size_t nargs,
                                  size_t nargs_named)
{
#define NAME "remove"
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	GlowSetObject *set = glow_objvalue(this);
	GLOW_ENTER(set);
	GlowValue ret = glow_set_remove(set, &args[0]);
	GLOW_EXIT(set);
	return ret;
#undef NAME
}

static GlowValue set_init(GlowValue *this, GlowValue *args, size_t nargs)
{
	GLOW_ARG_COUNT_CHECK_AT_MOST("Set", nargs, 1);
	glow_obj_class.init(this, NULL, 0);

	GlowSetObject *set = glow_objvalue(this);
	GLOW_INIT_SAVED_TID_FIELD(set);
	set->entries = make_empty_table(EMPTY_SIZE);
	set->count = 0;
	set->capacity = EMPTY_SIZE;
	set->threshold = (size_t)(EMPTY_SIZE * LOAD_FACTOR);
	set->state_id = 0;

	if (nargs > 0) {
		GlowValue iter = glow_op_iter(&args[0]);

		if (glow_iserror(&iter)) {
			set_free_entries(set);
			return iter;
		}

		while (true) {
			GlowValue next = glow_op_iternext(&iter);

			if (glow_is_iter_stop(&next)) {
				break;
			} else if (glow_iserror(&next)) {
				set_free_entries(set);
				glow_release(&iter);
				return next;
			}

			GlowValue v = glow_set_add(set, &next);

			if (glow_iserror(&v)) {
				set_free_entries(set);
				glow_release(&next);
				glow_release(&iter);
				return v;
			}
		}

		glow_release(&iter);
	}

	return *this;
}

static void set_free_entries(GlowSetObject *set)
{
	Entry **entries = set->entries;
	const size_t capacity = set->capacity;

	for (size_t i = 0; i < capacity; i++) {
		Entry *entry = entries[i];
		while (entry != NULL) {
			Entry *temp = entry;
			entry = entry->next;
			glow_release(&temp->element);
			free(temp);
		}
	}

	free(entries);
}

static void set_free(GlowValue *this)
{
	GlowSetObject *set = glow_objvalue(this);
	set_free_entries(set);
	glow_obj_class.del(this);
}

struct glow_num_methods glow_set_num_methods = {
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

struct glow_seq_methods glow_set_seq_methods = {
	set_len,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	set_contains,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

struct glow_attr_method set_methods[] = {
	{"add", set_add_method},
	{"remove", set_remove_method},
	{NULL, NULL}
};

GlowClass glow_set_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Set",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowSetObject),

	.init = set_init,
	.del = set_free,

	.eq = set_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = set_str,
	.call = NULL,

	.print = NULL,

	.iter = set_iter,
	.iternext = NULL,

	.num_methods = &glow_set_num_methods,
	.seq_methods = &glow_set_seq_methods,

	.members = NULL,
	.methods = set_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* set iterator */

static GlowValue iter_make(GlowSetObject *set)
{
	GlowSetIter *iter = glow_obj_alloc(&glow_set_iter_class);
	glow_retaino(set);
	iter->source = set;
	iter->saved_state_id = set->state_id;
	iter->current_entry = NULL;
	iter->current_index = 0;
	return glow_makeobj(iter);
}

static GlowValue iter_next(GlowValue *this)
{
	GlowSetIter *iter = glow_objvalue(this);
	GLOW_ENTER(iter->source);

	if (iter->saved_state_id != iter->source->state_id) {
		GLOW_EXIT(iter->source);
		return GLOW_ISC_EXC("set changed state during iteration");
	}

	Entry **entries = iter->source->entries;
	Entry *entry = iter->current_entry;
	size_t idx = iter->current_index;
	const size_t capacity = iter->source->capacity;

	if (idx >= capacity) {
		GLOW_EXIT(iter->source);
		return glow_get_iter_stop();
	}

	if (entry == NULL) {
		for (size_t i = idx; i < capacity; i++) {
			if (entries[i] != NULL) {
				entry = entries[i];
				idx = i+1;
				break;
			}
		}

		if (entry == NULL) {
			GLOW_EXIT(iter->source);
			return glow_get_iter_stop();
		}
	}

	GlowValue next = entry->element;
	glow_retain(&next);

	iter->current_index = idx;
	iter->current_entry = entry->next;

	GLOW_EXIT(iter->source);
	return next;
}

static void iter_free(GlowValue *this)
{
	GlowSetIter *iter = glow_objvalue(this);
	glow_releaseo(iter->source);
	glow_iter_class.del(this);
}

struct glow_seq_methods set_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_set_iter_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "SetIter",
	.super = &glow_iter_class,

	.instance_size = sizeof(GlowSetIter),

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
	.seq_methods = &set_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
