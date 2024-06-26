#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "util.h"
#include "strdict.h"

#define STRDICT_INIT_TABLE_SIZE  32
#define STRDICT_LOAD_FACTOR      0.75f

#define HASH(key)  (glow_util_hash_secondary(glow_str_hash((key))))

typedef GlowStrDictEntry Entry;

static Entry **make_empty_table(const size_t capacity);
static void strdict_resize(GlowStrDict *dict, const size_t new_capacity);

void glow_strdict_init(GlowStrDict *dict)
{
	dict->table = make_empty_table(STRDICT_INIT_TABLE_SIZE);
	dict->count = 0;
	dict->capacity = STRDICT_INIT_TABLE_SIZE;
	dict->threshold = (size_t)(STRDICT_INIT_TABLE_SIZE * STRDICT_LOAD_FACTOR);
}

static Entry *make_entry(GlowStr *key, const int hash, GlowValue *value)
{
	Entry *entry = glow_malloc(sizeof(Entry));
	entry->key = *key;
	entry->hash = hash;
	entry->value = *value;
	entry->next = NULL;
	return entry;
}

GlowValue glow_strdict_get(GlowStrDict *dict, GlowStr *key)
{
	const int hash = HASH(key);
	for (Entry *entry = dict->table[hash & (dict->capacity - 1)];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && glow_str_eq(key, &entry->key)) {
			return entry->value;
		}
	}

	return glow_makeempty();
}

GlowValue glow_strdict_get_cstr(GlowStrDict *dict, const char *key)
{
	GlowStr key_str = GLOW_STR_INIT(key, strlen(key), 0);
	return glow_strdict_get(dict, &key_str);
}

void glow_strdict_put(GlowStrDict *dict, const char *key, GlowValue *value, bool key_freeable)
{
	GlowStr key_str = GLOW_STR_INIT(key, strlen(key), (key_freeable ? 1 : 0));
	Entry **table = dict->table;
	const size_t capacity = dict->capacity;
	const int hash = HASH(&key_str);
	const size_t index = hash & (capacity - 1);

	for (Entry *entry = table[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && glow_str_eq(&key_str, &entry->key)) {
			glow_release(&entry->value);

			if (entry->key.freeable) {
				glow_str_dealloc(&entry->key);
			}

			entry->key = key_str;
			entry->value = *value;
			return;
		}
	}

	Entry *entry = make_entry(&key_str, hash, value);
	entry->next = table[index];
	table[index] = entry;

	if (dict->count++ >= dict->threshold) {
		const size_t new_capacity = 2 * capacity;
		strdict_resize(dict, new_capacity);
		dict->threshold = (size_t)(new_capacity * STRDICT_LOAD_FACTOR);
	}
}

void glow_strdict_put_copy(GlowStrDict *dict, const char *key, size_t len, GlowValue *value)
{
	if (len == 0) {
		len = strlen(key);
	}
	char *key_copy = glow_malloc(len + 1);
	strcpy(key_copy, key);
	glow_strdict_put(dict, key_copy, value, true);
}

void glow_strdict_dealloc(GlowStrDict *dict)
{
	Entry **table = dict->table;
	const size_t capacity = dict->capacity;

	for (size_t i = 0; i < capacity; i++) {
		Entry *entry = table[i];
		while (entry != NULL) {
			Entry *temp = entry;
			entry = entry->next;
			glow_release(&temp->value);
			if (temp->key.freeable) {
				glow_str_dealloc(&temp->key);
			}
			free(temp);
		}
	}

	free(table);
}

static Entry **make_empty_table(const size_t capacity)
{
	Entry **table = glow_malloc(capacity * sizeof(Entry *));
	for (size_t i = 0; i < capacity; i++) {
		table[i] = NULL;
	}
	return table;
}

static void strdict_resize(GlowStrDict *dict, const size_t new_capacity)
{
	const size_t old_capacity = dict->capacity;
	Entry **old_table = dict->table;
	Entry **new_table = make_empty_table(new_capacity);

	for (size_t i = 0; i < old_capacity; i++) {
		Entry *entry = old_table[i];
		while (entry != NULL) {
			Entry *next = entry->next;
			const size_t index = entry->hash & (new_capacity - 1);
			entry->next = new_table[index];
			new_table[index] = entry;
			entry = next;
		}
	}

	free(old_table);
	dict->table = new_table;
	dict->capacity = new_capacity;
}
