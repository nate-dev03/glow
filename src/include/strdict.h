#ifndef GLOW_STRDICT_H
#define GLOW_STRDICT_H

#include <stdlib.h>
#include "str.h"
#include "object.h"

typedef struct str_dict_entry {
	GlowStr key;
	int hash;
	GlowValue value;
	struct str_dict_entry *next;
} GlowStrDictEntry;

typedef struct {
	GlowStrDictEntry **table;
	size_t count;
	size_t capacity;
	size_t threshold;
} GlowStrDict;

void glow_strdict_init(GlowStrDict *dict);
GlowValue glow_strdict_get(GlowStrDict *dict, GlowStr *key);
GlowValue glow_strdict_get_cstr(GlowStrDict *dict, const char *key);
void glow_strdict_put(GlowStrDict *dict, const char *key, GlowValue *value, bool key_freeable);
void glow_strdict_put_copy(GlowStrDict *dict, const char *key, size_t len, GlowValue *value);
void glow_strdict_dealloc(GlowStrDict *dict);

#endif /* GLOW_STRDICT_H */
