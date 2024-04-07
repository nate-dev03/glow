#ifndef GLOW_DICT_H
#define GLOW_DICT_H

#include <stdlib.h>
#include "object.h"

extern struct glow_num_methods glow_dict_num_methods;
extern struct glow_seq_methods glow_dict_seq_methods;
extern GlowClass glow_dict_class;

struct glow_dict_entry {
	GlowValue key;
	GlowValue value;
	int hash;
	struct glow_dict_entry *next;
};

typedef struct {
	GlowObject base;
	struct glow_dict_entry **entries;
	size_t count;
	size_t capacity;
	size_t threshold;
	unsigned state_id;
	GLOW_SAVED_TID_FIELD
} GlowDictObject;

/*
 * Elements should be given in `entries` in the format
 *   [key1, value1, key2, value2, ..., keyN, valueN]
 * where the `size` parameter is the length of this list.
 */
GlowValue glow_dict_make(GlowValue *entries, const size_t size);

GlowValue glow_dict_get(GlowDictObject *dict, GlowValue *key, GlowValue *dflt);
GlowValue glow_dict_put(GlowDictObject *dict, GlowValue *key, GlowValue *value);
GlowValue glow_dict_remove_key(GlowDictObject *dict, GlowValue *key);
GlowValue glow_dict_contains_key(GlowDictObject *dict, GlowValue *key);
GlowValue glow_dict_eq(GlowDictObject *dict, GlowDictObject *other);
size_t glow_dict_len(GlowDictObject *dict);

extern GlowClass glow_dict_iter_class;

typedef struct {
	GlowIter base;
	GlowDictObject *source;
	unsigned saved_state_id;
	size_t current_index;
	struct glow_dict_entry *current_entry;
} GlowDictIter;

#endif /* GLOW_DICT_H */
