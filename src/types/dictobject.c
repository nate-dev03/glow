#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "exc.h"
#include "strbuf.h"
#include "vmops.h"
#include "object.h"
#include "strobject.h"
#include "tupleobject.h"
#include "iter.h"
#include "util.h"
#include "dictobject.h"

#define EMPTY_SIZE  16
#define LOAD_FACTOR 0.75f

typedef struct glow_dict_entry Entry;

#define KEY_EXC(key) GLOW_INDEX_EXC("dict has no key '%s'", (key));

static Entry **make_empty_table(const size_t capacity);
static void dict_resize(GlowDictObject *dict, const size_t new_capacity);
static void dict_free(GlowValue *this);

GlowValue glow_dict_make(GlowValue *entries, const size_t size)
{
	GlowDictObject *dict = glow_obj_alloc(&glow_dict_class);
	GLOW_INIT_SAVED_TID_FIELD(dict);

	const size_t capacity = (size == 0) ? EMPTY_SIZE : glow_smallest_pow_2_at_least(size);

	dict->entries = make_empty_table(capacity);
	dict->count = 0;
	dict->capacity = capacity;
	dict->threshold = (size_t)(capacity * LOAD_FACTOR);
	dict->state_id = 0;

	for (size_t i = 0; i < size; i += 2) {
		GlowValue *key = &entries[i];
		GlowValue *value = &entries[i+1];
		GlowValue v = glow_dict_put(dict, key, value);

		if (glow_iserror(&v)) {
			GlowValue dict_v = glow_makeobj(dict);
			dict_free(&dict_v);

			for (size_t j = i; j < size; j++) {
				glow_release(&entries[j]);
			}

			return v;
		} else {
			glow_release(&v);
		}

		glow_release(key);
		glow_release(value);
	}

	return glow_makeobj(dict);
}

static Entry *make_entry(GlowValue *key, GlowValue *value, const int hash)
{
	Entry *entry = glow_malloc(sizeof(Entry));
	entry->key = *key;
	entry->value = *value;
	entry->hash = hash;
	entry->next = NULL;
	return entry;
}

GlowValue glow_dict_get(GlowDictObject *dict, GlowValue *key, GlowValue *dflt)
{
	const GlowValue hash_v = glow_op_hash(key);

	if (glow_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(key));

	for (Entry *entry = dict->entries[hash & (dict->capacity - 1)];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			GlowValue eq_v = eq(key, &entry->key);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (glow_boolvalue(&eq_v)) {
				glow_retain(&entry->value);
				return entry->value;
			}
		}
	}

	if (dflt != NULL) {
		glow_retain(dflt);
		return *dflt;
	} else {
		GlowValue str_v = glow_op_str(key);

		if (glow_iserror(&str_v)) {
			return str_v;
		}

		GlowStrObject *str = glow_objvalue(&str_v);
		GlowValue exc = KEY_EXC(str->str.value);
		glow_releaseo(str);
		return exc;
	}
}

GlowValue glow_dict_put(GlowDictObject *dict, GlowValue *key, GlowValue *value)
{
	Entry **table = dict->entries;
	const size_t capacity = dict->capacity;
	const GlowValue hash_v = glow_op_hash(key);

	if (glow_iserror(&hash_v)) {
		return hash_v;
	}

	++dict->state_id;
	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));
	const size_t index = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(key));

	for (Entry *entry = table[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			GlowValue eq_v = eq(key, &entry->key);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (glow_boolvalue(&eq_v)) {
				glow_retain(value);
				GlowValue old = entry->value;
				entry->value = *value;
				return old;
			}
		}
	}

	glow_retain(key);
	glow_retain(value);

	Entry *entry = make_entry(key, value, hash);
	entry->next = table[index];
	table[index] = entry;

	if (dict->count++ >= dict->threshold) {
		const size_t new_capacity = (2 * capacity);
		dict_resize(dict, new_capacity);
		dict->threshold = (size_t)(new_capacity * LOAD_FACTOR);
	}

	return glow_makeempty();
}

GlowValue glow_dict_remove_key(GlowDictObject *dict, GlowValue *key)
{
	const GlowValue hash_v = glow_op_hash(key);

	if (glow_iserror(&hash_v)) {
		return hash_v;
	}

	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(key));

	Entry **entries = dict->entries;
	const size_t idx = hash & (dict->capacity - 1);
	Entry *entry = entries[idx];
	Entry *prev = entry;

	while (entry != NULL) {
		Entry *next = entry->next;

		if (hash == entry->hash) {
			GlowValue eq_v = eq(key, &entry->key);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (glow_boolvalue(&eq_v)) {
				GlowValue value = entry->value;

				if (prev == entry) {
					entries[idx] = next;
				} else {
					prev->next = next;
				}

				glow_release(&entry->key);
				free(entry);
				--dict->count;
				++dict->state_id;
				return value;
			}
		}

		prev = entry;
		entry = next;
	}

	return glow_makeempty();
}

GlowValue glow_dict_contains_key(GlowDictObject *dict, GlowValue *key)
{
	Entry **entries = dict->entries;
	const size_t capacity = dict->capacity;
	GlowValue hash_v = glow_op_hash(key);

	if (glow_iserror(&hash_v)) {
		glow_release(&hash_v);
		return glow_makefalse();
	}

	const int hash = glow_util_hash_secondary(glow_intvalue(&hash_v));
	const size_t idx = hash & (capacity - 1);

	/* every value should have a valid `eq` */
	const GlowBinOp eq = glow_resolve_eq(glow_getclass(key));

	for (Entry *entry = entries[idx];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash) {
			GlowValue eq_v = eq(key, &entry->key);

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

GlowValue glow_dict_eq(GlowDictObject *dict, GlowDictObject *other)
{
	if (dict->count != other->count) {
		return glow_makefalse();
	}

	GlowDictObject *d1, *d2;

	if (dict->capacity < other->capacity) {
		d1 = dict;
		d2 = other;
	} else {
		d1 = other;
		d2 = dict;
	}

	Entry **entries = d1->entries;
	size_t capacity = d1->capacity;
	static GlowValue empty = GLOW_MAKE_EMPTY();

	for (size_t i = 0; i < capacity; i++) {
		for (Entry *entry = entries[i];
		     entry != NULL;
		     entry = entry->next) {

			GlowValue v1 = entry->value;
			const GlowBinOp eq = glow_resolve_eq(glow_getclass(&v1));
			GlowValue v2 = glow_dict_get(d2, &entry->key, &empty);

			if (glow_isempty(&v2)) {
				goto neq;
			}

			GlowValue eq_v = eq(&v1, &v2);

			if (glow_iserror(&eq_v)) {
				return eq_v;
			}

			if (!glow_boolvalue(&eq_v)) {
				goto neq;
			}
		}
	}

	return glow_maketrue();

	neq:
	return glow_makefalse();
}

size_t glow_dict_len(GlowDictObject *dict)
{
	return dict->count;
}

static Entry **make_empty_table(const size_t capacity)
{
	Entry **table = glow_malloc(capacity * sizeof(Entry *));
	for (size_t i = 0; i < capacity; i++) {
		table[i] = NULL;
	}
	return table;
}

static void dict_resize(GlowDictObject *dict, const size_t new_capacity)
{
	const size_t old_capacity = dict->capacity;
	Entry **old_entries = dict->entries;
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
	dict->entries = new_entries;
	dict->capacity = new_capacity;
	++dict->state_id;
}

static GlowValue dict_get(GlowValue *this, GlowValue *key)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue ret = glow_dict_get(dict, key, NULL);
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue dict_set(GlowValue *this, GlowValue *key, GlowValue *value)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue ret = glow_dict_put(dict, key, value);
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue dict_contains(GlowValue *this, GlowValue *key)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue ret = glow_dict_contains_key(dict, key);
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue dict_eq(GlowValue *this, GlowValue *other)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);

	if (!glow_is_a(other, &glow_dict_class)) {
		GLOW_EXIT(dict);
		return glow_makefalse();
	}

	GlowDictObject *other_dict = glow_objvalue(other);
	GlowValue ret = glow_dict_eq(dict, other_dict);
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue dict_len(GlowValue *this)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue ret = glow_makeint(glow_dict_len(dict));
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue dict_str(GlowValue *this)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);

	const size_t capacity = dict->capacity;

	if (dict->count == 0) {
		GLOW_EXIT(dict);
		return glow_strobj_make_direct("{}", 2);
	}

	GlowStrBuf sb;
	glow_strbuf_init(&sb, 16);
	glow_strbuf_append(&sb, "{", 1);

	Entry **entries = dict->entries;

	bool first = true;
	for (size_t i = 0; i < capacity; i++) {
		for (Entry *e = entries[i]; e != NULL; e = e->next) {
			if (!first) {
				glow_strbuf_append(&sb, ", ", 2);
			}
			first = false;

			GlowValue *key = &e->key;
			GlowValue *value = &e->value;

			if (glow_isobject(key) && glow_objvalue(key) == dict) {
				glow_strbuf_append(&sb, "{...}", 5);
			} else {
				GlowValue str_v = glow_op_str(key);

				if (glow_iserror(&str_v)) {
					glow_strbuf_dealloc(&sb);
					GLOW_EXIT(dict);
					return str_v;
				}

				GlowStrObject *str = glow_objvalue(&str_v);
				glow_strbuf_append(&sb, str->str.value, str->str.len);
				glow_releaseo(str);
			}

			glow_strbuf_append(&sb, ": ", 2);

			if (glow_isobject(value) && glow_objvalue(value) == dict) {
				glow_strbuf_append(&sb, "{...}", 5);
			} else {
				GlowValue str_v = glow_op_str(value);

				if (glow_iserror(&str_v)) {
					glow_strbuf_dealloc(&sb);
					GLOW_EXIT(dict);
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
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue iter_make(GlowDictObject *dict);

static GlowValue dict_iter(GlowValue *this)
{
	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue ret = iter_make(dict);
	GLOW_EXIT(dict);
	return ret;
}

static GlowValue dict_get_method(GlowValue *this,
                                GlowValue *args,
                                GlowValue *args_named,
                                size_t nargs,
                                size_t nargs_named)
{
#define NAME "get"
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK_BETWEEN(NAME, nargs, 1, 2);

	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue ret = glow_dict_get(dict, &args[0], (nargs == 2) ? &args[1] : NULL);
	GLOW_EXIT(dict);
	return ret;
#undef NAME
}

static GlowValue dict_put_method(GlowValue *this,
                                GlowValue *args,
                                GlowValue *args_named,
                                size_t nargs,
                                size_t nargs_named)
{
#define NAME "put"
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 2);

	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue old = glow_dict_put(dict, &args[0], &args[1]);
	GlowValue ret = glow_isempty(&old) ? glow_makenull() : old;
	GLOW_EXIT(dict);
	return ret;
#undef NAME
}

static GlowValue dict_remove_method(GlowValue *this,
                                   GlowValue *args,
                                   GlowValue *args_named,
                                   size_t nargs,
                                   size_t nargs_named)
{
#define NAME "remove"
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	GlowDictObject *dict = glow_objvalue(this);
	GLOW_ENTER(dict);
	GlowValue v = glow_dict_remove_key(dict, &args[0]);
	GlowValue ret = glow_isempty(&v) ? glow_makenull() : v;
	GLOW_EXIT(dict);
	return ret;
#undef NAME
}

static void dict_free(GlowValue *this)
{
	GlowDictObject *dict = glow_objvalue(this);
	Entry **entries = dict->entries;
	const size_t capacity = dict->capacity;

	for (size_t i = 0; i < capacity; i++) {
		Entry *entry = entries[i];
		while (entry != NULL) {
			Entry *temp = entry;
			entry = entry->next;
			glow_release(&temp->key);
			glow_release(&temp->value);
			free(temp);
		}
	}

	free(entries);
	glow_obj_class.del(this);
}

struct glow_num_methods glow_dict_num_methods = {
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

struct glow_seq_methods glow_dict_seq_methods = {
	dict_len,    /* len */
	dict_get,    /* get */
	dict_set,    /* set */
	dict_contains,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

struct glow_attr_method dict_methods[] = {
	{"get", dict_get_method},
	{"put", dict_put_method},
	{"remove", dict_remove_method},
	{NULL, NULL}
};

GlowClass glow_dict_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Dict",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowDictObject),

	.init = NULL,
	.del = dict_free,

	.eq = dict_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = dict_str,
	.call = NULL,

	.print = NULL,

	.iter = dict_iter,
	.iternext = NULL,

	.num_methods = &glow_dict_num_methods,
	.seq_methods = &glow_dict_seq_methods,

	.members = NULL,
	.methods = dict_methods,

	.attr_get = NULL,
	.attr_set = NULL
};


/* dict iterator */

static GlowValue iter_make(GlowDictObject *dict)
{
	GlowDictIter *iter = glow_obj_alloc(&glow_dict_iter_class);
	glow_retaino(dict);
	iter->source = dict;
	iter->saved_state_id = dict->state_id;
	iter->current_entry = NULL;
	iter->current_index = 0;
	return glow_makeobj(iter);
}

static GlowValue iter_next(GlowValue *this)
{
	GlowDictIter *iter = glow_objvalue(this);
	GLOW_ENTER(iter->source);

	if (iter->saved_state_id != iter->source->state_id) {
		GLOW_EXIT(iter->source);
		return GLOW_ISC_EXC("dict changed state during iteration");
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

	GlowValue pair[] = {entry->key, entry->value};
	glow_retain(&pair[0]);
	glow_retain(&pair[1]);

	iter->current_index = idx;
	iter->current_entry = entry->next;

	GLOW_EXIT(iter->source);
	return glow_tuple_make(pair, 2);
}

static void iter_free(GlowValue *this)
{
	GlowDictIter *iter = glow_objvalue(this);
	glow_releaseo(iter->source);
	glow_iter_class.del(this);
}

struct glow_seq_methods dict_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_dict_iter_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "DictIter",
	.super = &glow_iter_class,

	.instance_size = sizeof(GlowDictIter),

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
	.seq_methods = &dict_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
