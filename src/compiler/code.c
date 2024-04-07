#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compiler.h"
#include "err.h"
#include "util.h"
#include "code.h"

void glow_code_init(GlowCode *code, size_t capacity)
{
	code->bc = glow_malloc(capacity);
	code->size = 0;
	code->capacity = capacity;
}

void glow_code_dealloc(GlowCode *code)
{
	free(code->bc);
}

void glow_code_ensure_capacity(GlowCode *code, size_t min_capacity)
{
	size_t capacity = code->capacity;
	if (capacity < min_capacity) {
		while (capacity < min_capacity) {
			capacity <<= 1;
		}

		code->capacity = capacity;
		code->bc = glow_realloc(code->bc, capacity);
	}
}

void glow_code_write_byte(GlowCode *code, const byte b)
{
	glow_code_ensure_capacity(code, code->size + 1);
	code->bc[code->size++] = b;
}

/*
 * This is typically used for writing "sizes" to the bytecode
 * (for example, the size of the symbol table), which is why
 * `n` is of type `size_t`. An error will be emitted if `n`
 * is too large, however.
 */
void glow_code_write_uint16(GlowCode *code, const size_t n)
{
	glow_code_write_uint16_at(code, n, code->size);
}

void glow_code_write_uint16_at(GlowCode *code, const size_t n, const size_t pos)
{
	if (n > 0xFFFF) {
		GLOW_INTERNAL_ERROR();
	}

	const size_t code_size = code->size;

	if (pos > code_size) {
		GLOW_INTERNAL_ERROR();
	}

	if (code_size < 2 || pos > code_size - 2) {
		glow_code_ensure_capacity(code, pos + 2);
		code->size += (pos + 2 - code_size);
	}

	glow_util_write_uint16_to_stream(code->bc + pos, n);
}

void glow_code_write_int(GlowCode *code, const int n)
{
	glow_code_ensure_capacity(code, code->size + GLOW_INT_SIZE);
	glow_util_write_int32_to_stream(code->bc + code->size, n);
	code->size += GLOW_INT_SIZE;
}

void glow_code_write_double(GlowCode *code, const double d)
{
	glow_code_ensure_capacity(code, code->size + GLOW_DOUBLE_SIZE);
	glow_util_write_double_to_stream(code->bc + code->size, d);
	code->size += GLOW_DOUBLE_SIZE;
}

void glow_code_write_str(GlowCode *code, const GlowStr *str)
{
	const size_t len = str->len;
	glow_code_ensure_capacity(code, code->size + len + 1);

	for (size_t i = 0; i < len; i++) {
		code->bc[code->size++] = str->value[i];
	}
	code->bc[code->size++] = '\0';
}

void glow_code_append(GlowCode *code, const GlowCode *append)
{
	const size_t size = append->size;
	glow_code_ensure_capacity(code, code->size + size);

	memcpy(code->bc + code->size, append->bc, size);
	code->size += size;
}

byte glow_code_read_byte(GlowCode *code)
{
	--code->size;
	return *code->bc++;
}

int glow_code_read_int(GlowCode *code)
{
	code->size -= GLOW_INT_SIZE;
	const int ret = glow_util_read_int32_from_stream(code->bc);
	code->bc += GLOW_INT_SIZE;
	return ret;
}

unsigned int glow_code_read_uint16(GlowCode *code)
{
	code->size -= 2;
	const unsigned int ret = glow_util_read_uint16_from_stream(code->bc);
	code->bc += 2;
	return ret;
}

double glow_code_read_double(GlowCode *code)
{
	code->size -= GLOW_DOUBLE_SIZE;
	const double ret = glow_util_read_double_from_stream(code->bc);
	code->bc += GLOW_DOUBLE_SIZE;
	return ret;
}

const char *glow_code_read_str(GlowCode *code)
{
	const char *start = (const char *)code->bc;

	while (*(code->bc++) != '\0');

	return start;
}

void glow_code_skip_ahead(GlowCode *code, const size_t skip)
{
	assert(skip <= code->size);
	code->bc += skip;
	code->size -= skip;
}

void glow_code_cpy(GlowCode *dst, GlowCode *src)
{
	dst->bc = glow_malloc(src->capacity);
	memcpy(dst->bc, src->bc, src->size);
	dst->size = src->size;
	dst->capacity = src->capacity;
}
