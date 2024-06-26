#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "err.h"
#include "util.h"

/*
 * Hash functions
 */

int glow_util_hash_int(const int i)
{
	return i;
}

int glow_util_hash_long(const long l)
{
	return (int)(l ^ ((unsigned long)l >> 32));
}

int glow_util_hash_double(const double d)
{
	unsigned long long l = 0;
	memcpy(&l, &d, sizeof(double));
	return (int)(l ^ (l >> 32));
}

int glow_util_hash_float(const float f)
{
	unsigned long long l = 0;
	memcpy(&l, &f, sizeof(float));
	return (int)(l ^ (l >> 32));
}

int glow_util_hash_bool(const bool b)
{
	return b ? 1231 : 1237;
}

int glow_util_hash_ptr(const void *p)
{
	uintptr_t ad = (uintptr_t)p;
	return (int)((13*ad) ^ (ad >> 15));
}

int glow_util_hash_cstr(const char *str)
{
	unsigned int h = 0;
	char *p = (char *)str;

	while (*p++) {
		h = 31*h + *p;
	}

	return h;
}

int glow_util_hash_cstr2(const char *str, const size_t len)
{
	unsigned int h = 0;

	for (size_t i = 0; i < len; i++) {
		h = 31*h + str[i];
	}

	return h;
}

/* Adapted from java.util.HashMap#hash */
int glow_util_hash_secondary(int h)
{
	h ^= ((unsigned)h >> 20) ^ ((unsigned)h >> 12);
	return h ^ ((unsigned)h >> 7) ^ ((unsigned)h >> 4);
}

/*
 * Serialization functions
 */

void glow_util_write_int32_to_stream(unsigned char *stream, const int n)
{
	stream[0] = (n >> 0 ) & 0xFF;
	stream[1] = (n >> 8 ) & 0xFF;
	stream[2] = (n >> 16) & 0xFF;
	stream[3] = (n >> 24) & 0xFF;
}

int glow_util_read_int32_from_stream(unsigned char *stream)
{
	const int n = (stream[3] << 24) |
	              (stream[2] << 16) |
	              (stream[1] << 8 ) |
	              (stream[0] << 0 );
	return n;
}

void glow_util_write_uint16_to_stream(unsigned char *stream, const unsigned int n)
{
	assert(n <= 0xFFFF);
	stream[0] = (n >> 0) & 0xFF;
	stream[1] = (n >> 8) & 0xFF;
}

unsigned int glow_util_read_uint16_from_stream(unsigned char *stream)
{
	const unsigned int n = (stream[1] << 8) | (stream[0] << 0);
	return n;
}

/*
 * TODO: check endianness
 */
void glow_util_write_double_to_stream(unsigned char *stream, const double d)
{
	memcpy(stream, &d, sizeof(double));
}

double glow_util_read_double_from_stream(unsigned char *stream)
{
	double d;
	memcpy(&d, stream, sizeof(double));
	return d;
}

/*
 * Memory allocation functions
 */

void *glow_malloc(size_t n)
{
	void *p = malloc(n);

	if (p == NULL && n > 0) {
		GLOW_INTERNAL_ERROR();
	}

	return p;
}

void *glow_calloc(size_t num, size_t size)
{
	void *p = calloc(num, size);

	if (p == NULL && num > 0 && size > 0) {
		GLOW_INTERNAL_ERROR();
	}

	return p;
}

void *glow_realloc(void *p, size_t n)
{
	void *new_p = realloc(p, n);

	if (new_p == NULL && n > 0) {
		GLOW_INTERNAL_ERROR();
	}

	return new_p;
}

/*
 * Miscellaneous functions
 */

const char *glow_util_str_dup(const char *str)
{
	const size_t len = strlen(str);
	char *copy = glow_malloc(len + 1);
	strcpy(copy, str);
	return copy;
}

const char *glow_util_str_format(const char *format, ...)
{
	char buf[1000];

	va_list args;
	va_start(args, format);
	int n = vsnprintf(buf, sizeof(buf), format, args);
	assert(n >= 0);
	va_end(args);

	if ((size_t)n >= sizeof(buf)) {
		n = sizeof(buf) - 1;
	}

	char *str = glow_malloc(n + 1);
	strcpy(str, buf);
	return str;
}

char *glow_util_file_to_str(const char *filename)
{
	FILE *file = fopen(filename, "r");

	if (file == NULL) {
		return NULL;
	}

	fseek(file, 0L, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	char *str = glow_malloc(size + 1);
	fread(str, sizeof(char), size, file);
	fclose(file);
	str[size] = '\0';
	return str;
}

size_t glow_smallest_pow_2_at_least(size_t x)
{
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x+1;
}
