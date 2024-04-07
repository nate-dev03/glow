#ifndef GLOW_CODE_H
#define GLOW_CODE_H

#include <stdint.h>
#include "object.h"
#include "str.h"

/* fundamental unit of compiled code: 8-bit byte */
typedef uint8_t byte;

#define GLOW_INT_SIZE    4
#define GLOW_DOUBLE_SIZE 8

/*
 * Low-level bytecode storage facility
 */
typedef struct {
	/* bytecode */
	byte *bc;

	/* current size in bytes */
	size_t size;

	/* capacity of allocated array */
	size_t capacity;
} GlowCode;

void glow_code_init(GlowCode *code, size_t capacity);

void glow_code_dealloc(GlowCode *code);

void glow_code_ensure_capacity(GlowCode *code, size_t min_capacity);

void glow_code_write_byte(GlowCode *code, byte b);

void glow_code_write_int(GlowCode *code, const int n);

void glow_code_write_uint16(GlowCode *code, const size_t n);

void glow_code_write_uint16_at(GlowCode *code, const size_t n, const size_t pos);

void glow_code_write_double(GlowCode *code, const double d);

void glow_code_write_str(GlowCode *code, const GlowStr *str);

void glow_code_append(GlowCode *code, const GlowCode *append);

byte glow_code_read_byte(GlowCode *code);

int glow_code_read_int(GlowCode *code);

unsigned int glow_code_read_uint16(GlowCode *code);

double glow_code_read_double(GlowCode *code);

const char *glow_code_read_str(GlowCode *code);

void glow_code_skip_ahead(GlowCode *code, const size_t skip);

void glow_code_cpy(GlowCode *dst, GlowCode *src);

#endif /* GLOW_CODE_H */
