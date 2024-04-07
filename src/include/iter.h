#ifndef GLOW_ITER_H
#define GLOW_ITER_H

#include <pthread.h>
#include "object.h"

/* Base iterator */
extern GlowClass glow_iter_class;

typedef struct {
	GlowObject base;
} GlowIter;

/* Singleton used to mark end of iteration */
extern GlowClass glow_iter_stop_class;

typedef struct {
	GlowObject base;
} GlowIterStop;

GlowValue glow_get_iter_stop(void);
#define glow_is_iter_stop(v) (glow_getclass((v)) == &glow_iter_stop_class)

/* Result of applying function to iterator */
extern GlowClass glow_applied_iter_class;

typedef struct {
	GlowIter base;
	GlowIter *source;
	GlowValue fn;
} GlowAppliedIter;

GlowValue glow_applied_iter_make(GlowIter *source, GlowValue *fn);

/* Range iterator */
extern GlowClass glow_range_class;

typedef struct {
	GlowIter base;
	GLOW_SAVED_TID_FIELD
	long from;
	long to;
	long i;
} GlowRange;

GlowValue glow_range_make(GlowValue *from, GlowValue *to);

#endif /* GLOW_ITER_H */
