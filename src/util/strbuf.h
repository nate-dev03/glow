#ifndef GLOW_STRBUF_H
#define GLOW_STRBUF_H

#include <stdlib.h>
#include "str.h"

typedef struct {
	char *buf;
	size_t len;
	size_t cap;
} GlowStrBuf;

void glow_strbuf_init(GlowStrBuf *sb, const size_t cap);
void glow_strbuf_init_default(GlowStrBuf *sb);
void glow_strbuf_append(GlowStrBuf *sb, const char *str, const size_t len);
void glow_strbuf_to_str(GlowStrBuf *sb, GlowStr *dest);
void glow_strbuf_trim(GlowStrBuf *sb);
void glow_strbuf_dealloc(GlowStrBuf *sb);

#endif /* GLOW_STRBUF_H */
