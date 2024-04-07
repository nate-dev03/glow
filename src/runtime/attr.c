#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "err.h"
#include "attr.h"

static void attr_dict_put(GlowAttrDict *d, const char *key, unsigned int attr_index, bool is_method);
static int hash(const char *key);

void glow_attr_dict_init(GlowAttrDict *d, const size_t max_size)
{
	const size_t capacity = (max_size * 8)/5;
	d->table = glow_malloc(capacity * sizeof(GlowAttrDictEntry *));
	for (size_t i = 0; i < capacity; i++) {
		d->table[i] = NULL;
	}
	d->table_capacity = capacity;
	d->table_size = 0;
}

unsigned int glow_attr_dict_get(GlowAttrDict *d, const char *key)
{
	const size_t table_capacity = d->table_capacity;

	if (table_capacity == 0) {
		return 0;
	}

	const int h = hash(key);
	const size_t index = h & (table_capacity - 1);

	for (GlowAttrDictEntry *e = d->table[index]; e != NULL; e = e->next) {
		if (h == e->hash && strcmp(key, e->key) == 0) {
			return e->value;
		}
	}

	return 0;
}

static void attr_dict_put(GlowAttrDict *d, const char *key, unsigned int attr_index, bool is_method)
{
	const size_t table_capacity = d->table_capacity;

	if (table_capacity == 0) {
		GLOW_INTERNAL_ERROR();
	}

	unsigned int value = (attr_index << 2) | GLOW_ATTR_DICT_FLAG_FOUND;
	if (is_method) {
		value |= GLOW_ATTR_DICT_FLAG_METHOD;
	}

	const int h = hash(key);
	const size_t index = h & (table_capacity - 1);

	GlowAttrDictEntry *e = glow_malloc(sizeof(GlowAttrDictEntry));
	e->key = key;
	e->value = value;
	e->hash = h;
	e->next = d->table[index];
	d->table[index] = e;
}

void glow_attr_dict_register_members(GlowAttrDict *d, struct glow_attr_member *members)
{
	if (members == NULL) {
		return;
	}

	unsigned int index = 0;
	while (members[index].name != NULL) {
		attr_dict_put(d, members[index].name, index, false);
		++index;
	}
}

void glow_attr_dict_register_methods(GlowAttrDict *d, struct glow_attr_method *methods)
{
	if (methods == NULL) {
		return;
	}

	unsigned int index = 0;
	while (methods[index].name != NULL) {
		attr_dict_put(d, methods[index].name, index, true);
		++index;
	}
}

/*
 * Hashes null-terminated string
 */
static int hash(const char *key)
{
	char *p = (char *)key;

	unsigned int h = 0;
	while (*p) {
		h = 31*h + *p;
		++p;
	}

	return glow_util_hash_secondary((int)h);
}
