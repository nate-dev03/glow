#ifndef GLOW_EXC_H
#define GLOW_EXC_H

#include <stdio.h>
#include <stdbool.h>

#include "err.h"
#include "object.h"

typedef struct {
	GlowObject base;
	const char *msg;
	struct glow_traceback_manager tbm;
} GlowException;

typedef struct {
	GlowException base;
} GlowIndexException;

typedef struct {
	GlowException base;
} GlowTypeException;

typedef struct {
	GlowException base;
} GlowIOException;

typedef struct {
	GlowException base;
} GlowAttributeException;

typedef struct {
	GlowException base;
} GlowImportException;

typedef struct {
	GlowException base;
} GlowIllegalStateChangeException;

typedef struct {
	GlowException base;
} GlowSequenceExpandException;

typedef struct {
	GlowException base;
} GlowActorException;

typedef struct {
	GlowException base;
} GlowConcurrentAccessException;

extern GlowClass glow_exception_class;
extern GlowClass glow_index_exception_class;
extern GlowClass glow_type_exception_class;
extern GlowClass glow_io_exception_class;
extern GlowClass glow_attr_exception_class;
extern GlowClass glow_import_exception_class;
extern GlowClass glow_isc_exception_class;
extern GlowClass glow_seq_exp_exception_class;
extern GlowClass glow_actor_exception_class;
extern GlowClass glow_conc_access_exception_class;

GlowValue glow_exc_make(GlowClass *exc_class, bool active, const char *msg_format, ...);
void glow_exc_traceback_append(GlowException *e,
                              const char *fn,
                              const unsigned int lineno);
void glow_exc_traceback_print(GlowException *e, FILE *out);
void glow_exc_print_msg(GlowException *e, FILE *out);

#define GLOW_EXC(...)           glow_exc_make(&glow_exception_class, true, __VA_ARGS__)
#define GLOW_INDEX_EXC(...)     glow_exc_make(&glow_index_exception_class, true, __VA_ARGS__)
#define GLOW_TYPE_EXC(...)      glow_exc_make(&glow_type_exception_class, true, __VA_ARGS__)
#define GLOW_IO_EXC(...)        glow_exc_make(&glow_io_exception_class, true, __VA_ARGS__)
#define GLOW_ATTR_EXC(...)      glow_exc_make(&glow_attr_exception_class, true, __VA_ARGS__)
#define GLOW_IMPORT_EXC(...)    glow_exc_make(&glow_import_exception_class, true, __VA_ARGS__)
#define GLOW_ISC_EXC(...)       glow_exc_make(&glow_isc_exception_class, true, __VA_ARGS__)
#define GLOW_SEQ_EXP_EXC(...)   glow_exc_make(&glow_seq_exp_exception_class, true, __VA_ARGS__)
#define GLOW_ACTOR_EXC(...)     glow_exc_make(&glow_actor_exception_class, true, __VA_ARGS__)
#define GLOW_CONC_ACCS_EXC(...) glow_exc_make(&glow_conc_access_exception_class, true, __VA_ARGS__)

GlowValue glow_type_exc_unsupported_1(const char *op, const GlowClass *c1);
GlowValue glow_type_exc_unsupported_2(const char *op, const GlowClass *c1, const GlowClass *c2);
GlowValue glow_type_exc_cannot_index(const GlowClass *c1);
GlowValue glow_type_exc_cannot_apply(const GlowClass *c1);
GlowValue glow_type_exc_cannot_instantiate(const GlowClass *c1);
GlowValue glow_type_exc_not_callable(const GlowClass *c1);
GlowValue glow_type_exc_not_iterable(const GlowClass *c1);
GlowValue glow_type_exc_not_iterator(const GlowClass *c1);
GlowValue glow_type_exc_hint_mismatch(const GlowClass *got, const GlowClass *expected);
GlowValue glow_call_exc_num_args(const char *fn, unsigned int got, unsigned int expected);
GlowValue glow_call_exc_num_args_at_most(const char *fn, unsigned int got, unsigned int expected);
GlowValue glow_call_exc_num_args_between(const char *fn, unsigned int got, unsigned int min, unsigned int max);
GlowValue glow_call_exc_named_args(const char *fn);
GlowValue glow_call_exc_dup_arg(const char *fn, const char *name);
GlowValue glow_call_exc_unknown_arg(const char *fn, const char *name);
GlowValue glow_call_exc_missing_arg(const char *fn, const char *name);
GlowValue glow_call_exc_native_named_args(void);
GlowValue glow_call_exc_constructor_named_args(void);
GlowValue glow_io_exc_cannot_open_file(const char *filename, const char *mode);
GlowValue glow_io_exc_cannot_read_file(const char *filename);
GlowValue glow_io_exc_cannot_write_file(const char *filename);
GlowValue glow_io_exc_file_closed(const char *filename);
GlowValue glow_attr_exc_not_found(const GlowClass *type, const char *attr);
GlowValue glow_attr_exc_readonly(const GlowClass *type, const char *attr);
GlowValue glow_attr_exc_mismatch(const GlowClass *type, const char *attr, const GlowClass *assign_type);
GlowValue glow_import_exc_not_found(const char *name);
GlowValue glow_seq_exp_exc_inconsistent(const unsigned int got, const unsigned int expected);

/* Miscellaneous utilities */
#define GLOW_ARG_COUNT_CHECK(name, count, expected) \
	if ((count) != (expected)) return glow_call_exc_num_args((name), (count), (expected));

#define GLOW_ARG_COUNT_CHECK_AT_MOST(name, count, expected) \
	if ((count) > (expected)) return glow_call_exc_num_args_at_most((name), (count), (expected));

#define GLOW_ARG_COUNT_CHECK_BETWEEN(name, count, min, max) \
	if ((count) < (min) || (count) > (max)) return glow_call_exc_num_args_between((name), (count), (min), (max));

#define GLOW_NO_NAMED_ARGS_CHECK(name, count) \
	if ((count) > 0) return glow_call_exc_named_args((name));

#endif /* GLOW_EXC_H */
