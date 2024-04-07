#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "str.h"
#include "util.h"
#include "strbuf.h"

#define STRBUF_DEFAULT_INIT_CAPACITY 16

static void strbuf_grow(GlowStrBuf *sb, const size_t min_cap);

void glow_strbuf_init(GlowStrBuf *sb, const size_t cap)
{
	assert(cap > 0);
	sb->buf = glow_malloc(cap);
	sb->len = 0;
	sb->cap = cap;
	sb->buf[0] = '\0';
}

void glow_strbuf_init_default(GlowStrBuf *sb)
{
	glow_strbuf_init(sb, STRBUF_DEFAULT_INIT_CAPACITY);
}

void glow_strbuf_append(GlowStrBuf *sb, const char *str, const size_t len)
{
	size_t new_len = sb->len + len;
	if (new_len + 1 > sb->cap) {
		strbuf_grow(sb, new_len + 1);
	}
	memcpy(sb->buf + sb->len, str, len);
	sb->len = new_len;
	sb->buf[new_len] = '\0';
}

void glow_strbuf_to_str(GlowStrBuf *sb, GlowStr *dest)
{
	*dest = GLOW_STR_INIT(sb->buf, sb->len, 0);
}

void glow_strbuf_trim(GlowStrBuf *sb)
{
	const size_t new_cap = sb->len + 1;
	sb->buf = glow_realloc(sb->buf, new_cap);
	sb->cap = new_cap;
}

void glow_strbuf_dealloc(GlowStrBuf *sb)
{
	free(sb->buf);
}

static void strbuf_grow(GlowStrBuf *sb, const size_t min_cap)
{
	size_t new_cap = (sb->cap + 1) * 2;
	if (new_cap < min_cap) {
		new_cap = min_cap;
	}

	sb->buf = glow_realloc(sb->buf, new_cap);
	sb->cap = new_cap;
}
