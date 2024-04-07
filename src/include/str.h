#ifndef GLOW_STR_H
#define GLOW_STR_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	const char *value;  // read-only; this should NEVER be mutated
	size_t len;

	int hash;
	unsigned hashed : 1;
	unsigned freeable : 1;
} GlowStr;

#define GLOW_STR_INIT(v, l, f) ((GlowStr){.value = (v), .len = (l), .hash = 0, .hashed = 0, .freeable = (f)})

GlowStr *glow_str_new(const char *value, const size_t len);
GlowStr *glow_str_new_copy(const char *value, const size_t len);

bool glow_str_eq(GlowStr *s1, GlowStr *s2);
int glow_str_cmp(GlowStr *s1, GlowStr *s2);
int glow_str_hash(GlowStr *str);

GlowStr *glow_str_cat(GlowStr *s1, GlowStr *s2);

void glow_str_dealloc(GlowStr *str);
void glow_str_free(GlowStr *str);

struct glow_str_array {
	/* bare-bones string array */
	struct {
		const char *str;
		size_t length;
	} *array;

	size_t length;
};

void glow_util_str_array_dup(struct glow_str_array *src, struct glow_str_array *dst);

#endif /* GLOW_STR_H */
