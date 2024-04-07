#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "str.h"

GlowStr *glow_str_new(const char *value, const size_t len)
{
	GlowStr *str = glow_malloc(sizeof(GlowStr));
	str->value = value;
	str->len = len;
	str->hash = 0;
	str->hashed = 0;
	str->freeable = 0;
	return str;
}

GlowStr *glow_str_new_copy(const char *value, const size_t len)
{
	GlowStr *str = glow_malloc(sizeof(GlowStr));
	char *copy = glow_malloc(len + 1);
	memcpy(copy, value, len);
	copy[len] = '\0';
	str->value = copy;

	str->len = len;
	str->hash = 0;
	str->hashed = 0;
	str->freeable = 0;

	return str;
}

bool glow_str_eq(GlowStr *s1, GlowStr *s2)
{
	if (s1->len != s2->len) {
		return false;
	}

	return memcmp(s1->value, s2->value, s1->len) == 0;
}

int glow_str_cmp(GlowStr *s1, GlowStr *s2)
{
	return strcmp(s1->value, s2->value);
}

int glow_str_hash(GlowStr *str)
{
	if (!str->hashed) {
		str->hash = glow_util_hash_cstr2(str->value, str->len);
		str->hashed = 1;
	}

	return str->hash;
}

GlowStr *glow_str_cat(GlowStr *s1, GlowStr *s2)
{
	const size_t len1 = s1->len, len2 = s2->len;
	const size_t len_cat = len1 + len2;

	char *cat = glow_malloc(len_cat + 1);

	for (size_t i = 0; i < len1; i++) {
		cat[i] = s1->value[i];
	}

	for (size_t i = 0; i < len2; i++) {
		cat[i + len1] = s2->value[i];
	}

	cat[len_cat] = '\0';

	return glow_str_new(cat, len_cat);
}

void glow_str_dealloc(GlowStr *str)
{
	GLOW_FREE(str->value);
}

void glow_str_free(GlowStr *str)
{
	glow_str_dealloc(str);
	free(str);
}

void glow_util_str_array_dup(struct glow_str_array *src, struct glow_str_array *dst)
{
	const size_t length = src->length;
	const size_t size = length * sizeof(*(src->array));
	dst->array = glow_malloc(size);
	dst->length = length;
	memcpy(dst->array, src->array, size);
}
