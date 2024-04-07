#ifndef GLOW_STROBJECT_H
#define GLOW_STROBJECT_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "object.h"

extern struct glow_num_methods glow_str_num_methods;
extern struct glow_seq_methods glow_str_seq_methods;
extern GlowClass glow_str_class;

typedef struct glow_str_object {
	GlowObject base;
	GlowStr str;
	bool freeable;  /* whether the underlying buffer should be freed */
} GlowStrObject;

GlowValue glow_strobj_make(GlowStr value);
GlowValue glow_strobj_make_direct(const char *value, const size_t len);

#endif /* GLOW_STROBJECT_H */
