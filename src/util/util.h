#ifndef GLOW_UTIL_H
#define GLOW_UTIL_H

#include <stddef.h>
#include <stdbool.h>

#define GLOW_ANSI_CLR_RED     "\x1b[31m"
#define GLOW_ANSI_CLR_GREEN   "\x1b[32m"
#define GLOW_ANSI_CLR_YELLOW  "\x1b[33m"
#define GLOW_ANSI_CLR_BLUE    "\x1b[34m"
#define GLOW_ANSI_CLR_MAGENTA "\x1b[35m"
#define GLOW_ANSI_CLR_CYAN    "\x1b[36m"
#define GLOW_ANSI_CLR_INFO    "\x1b[1;36m"
#define GLOW_ANSI_CLR_WARNING "\x1b[1;33m"
#define GLOW_ANSI_CLR_ERROR   "\x1b[1;31m"
#define GLOW_ANSI_CLR_RESET   "\x1b[0m"

#define GLOW_INFO_HEADER    GLOW_ANSI_CLR_INFO    "info:    " GLOW_ANSI_CLR_RESET
#define GLOW_WARNING_HEADER GLOW_ANSI_CLR_WARNING "warning: " GLOW_ANSI_CLR_RESET
#define GLOW_ERROR_HEADER   GLOW_ANSI_CLR_ERROR   "error:   " GLOW_ANSI_CLR_RESET

#define glow_getmember(instance, offset, type) (*(type *)((char *)instance + offset))

#define GLOW_UNUSED(x) (void)(x)
#define GLOW_SAFE(x) if (x) GLOW_INTERNAL_ERROR()

int glow_util_hash_int(const int i);
int glow_util_hash_long(const long l);
int glow_util_hash_double(const double d);
int glow_util_hash_float(const float f);
int glow_util_hash_bool(const bool b);
int glow_util_hash_ptr(const void *p);
int glow_util_hash_cstr(const char *str);
int glow_util_hash_cstr2(const char *str, const size_t len);
int glow_util_hash_secondary(int hash);

void glow_util_write_int32_to_stream(unsigned char *stream, const int n);
int glow_util_read_int32_from_stream(unsigned char *stream);
void glow_util_write_uint16_to_stream(unsigned char *stream, const unsigned int n);
unsigned int glow_util_read_uint16_from_stream(unsigned char *stream);
void glow_util_write_double_to_stream(unsigned char *stream, const double d);
double glow_util_read_double_from_stream(unsigned char *stream);

void *glow_malloc(size_t n);
void *glow_calloc(size_t num, size_t size);
void *glow_realloc(void *p, size_t n);
#define GLOW_FREE(ptr) free((void *)(ptr))

const char *glow_util_str_dup(const char *str);
const char *glow_util_str_format(const char *format, ...);

char *glow_util_file_to_str(const char *filename);

size_t glow_smallest_pow_2_at_least(size_t x);

#endif /* GLOW_UTIL_H */
