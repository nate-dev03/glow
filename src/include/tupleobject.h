#ifndef GLOW_TUPLEOBJECT_H
#define GLOW_TUPLEOBJECT_H

#include <stdlib.h>
#include "object.h"

extern struct glow_num_methods glow_tuple_num_methods;
extern struct glow_seq_methods glow_tuple_seq_methods;
extern GlowClass glow_tuple_class;

typedef struct {
	GlowObject base;
	size_t count;
	GlowValue elements[];
} GlowTupleObject;

GlowValue glow_tuple_make(GlowValue *elements, const size_t count);

#endif /* GLOW_TUPLEOBJECT_H */
