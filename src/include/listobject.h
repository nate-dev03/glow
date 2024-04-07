#ifndef GLOW_LISTOBJECT_H
#define GLOW_LISTOBJECT_H

#include <stdlib.h>
#include "object.h"
#include "iter.h"
#include "util.h"

extern struct glow_num_methods glow_list_num_methods;
extern struct glow_seq_methods glow_list_seq_methods;
extern GlowClass glow_list_class;

typedef struct {
	GlowObject base;
	GlowValue *elements;
	size_t count;
	size_t capacity;
	GLOW_SAVED_TID_FIELD
} GlowListObject;

GlowValue glow_list_make(GlowValue *elements, const size_t count);
GlowValue glow_list_get(GlowListObject *list, const size_t idx);
void glow_list_append(GlowListObject *list, GlowValue *v);
void glow_list_clear(GlowListObject *list);
void glow_list_trim(GlowListObject *list);

extern GlowClass glow_list_iter_class;

typedef struct {
	GlowIter base;
	GlowListObject *source;
	size_t index;
} GlowListIter;

#endif /* GLOW_LISTOBJECT_H */
