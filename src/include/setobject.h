#ifndef GLOW_SET_H
#define GLOW_SET_H

#include <stdlib.h>
#include "object.h"

extern struct glow_num_methods glow_set_num_methods;
extern struct glow_seq_methods glow_set_seq_methods;
extern GlowClass glow_set_class;

struct glow_set_entry {
	GlowValue element;
	int hash;
	struct glow_set_entry *next;
};

typedef struct {
	GlowObject base;
	struct glow_set_entry **entries;
	size_t count;
	size_t capacity;
	size_t threshold;
	unsigned state_id;
	GLOW_SAVED_TID_FIELD
} GlowSetObject;

GlowValue glow_set_make(GlowValue *elements, const size_t size);
GlowValue glow_set_add(GlowSetObject *set, GlowValue *element);
GlowValue glow_set_remove(GlowSetObject *set, GlowValue *element);
GlowValue glow_set_contains(GlowSetObject *set, GlowValue *element);
GlowValue glow_set_eq(GlowSetObject *set, GlowSetObject *other);
size_t glow_set_len(GlowSetObject *set);

extern GlowClass glow_set_iter_class;

typedef struct {
	GlowIter base;
	GlowSetObject *source;
	unsigned saved_state_id;
	size_t current_index;
	struct glow_set_entry *current_entry;
} GlowSetIter;

#endif /* GLOW_SET_H */
