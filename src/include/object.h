#ifndef GLOW_OBJECT_H
#define GLOW_OBJECT_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include "str.h"
#include "attr.h"
#include "metaclass.h"
#include "main.h"

typedef struct glow_value  GlowValue;
typedef struct glow_class  GlowClass;
typedef struct glow_object GlowObject;

typedef GlowValue (*GlowUnOp)(GlowValue *this);
typedef GlowValue (*GlowBinOp)(GlowValue *this, GlowValue *other);
typedef bool (*BoolUnOp)(GlowValue *this);
typedef struct glow_str_object *(*GlowStrUnOp)(GlowValue *this);

typedef GlowValue (*GlowInitFunc)(GlowValue *this, GlowValue *args, size_t nargs);
typedef void (*GlowDelFunc)(GlowValue *this);
typedef GlowValue (*GlowCallFunc)(GlowValue *this,
                                GlowValue *args,
                                GlowValue *args_named,
                                size_t nargs,
                                size_t nargs_named);
typedef int (*GlowPrintFunc)(GlowValue *this, FILE *out);
typedef GlowValue (*GlowSeqSetFunc)(GlowValue *this, GlowValue *idx, GlowValue *v);

typedef GlowValue (*GlowAttrGetFunc)(GlowValue *this, const char *attr);
typedef GlowValue (*GlowAttrSetFunc)(GlowValue *this, const char *attr, GlowValue *v);

struct glow_object {
	struct glow_class *class;
	atomic_uint refcnt;
	unsigned int monitor;
};

struct glow_num_methods;
struct glow_seq_methods;

struct glow_class {
	GlowObject base;
	const char *name;

	struct glow_class *super;

	const size_t instance_size;

	GlowInitFunc init;
	GlowDelFunc del;  /* every class should implement this */

	GlowBinOp eq;
	GlowUnOp hash;
	GlowBinOp cmp;
	GlowUnOp str;
	GlowCallFunc call;

	GlowPrintFunc print;

	GlowUnOp iter;
	GlowUnOp iternext;

	struct glow_num_methods *num_methods;
	struct glow_seq_methods *seq_methods;

	struct glow_attr_member *members;
	struct glow_attr_method *methods;
	GlowAttrDict attr_dict;

	GlowAttrGetFunc attr_get;
	GlowAttrSetFunc attr_set;
};

struct glow_num_methods {
	GlowUnOp plus;
	GlowUnOp minus;
	GlowUnOp abs;

	GlowBinOp add;
	GlowBinOp sub;
	GlowBinOp mul;
	GlowBinOp div;
	GlowBinOp mod;
	GlowBinOp pow;

	GlowUnOp bitnot;
	GlowBinOp bitand;
	GlowBinOp bitor;
	GlowBinOp xor;
	GlowBinOp shiftl;
	GlowBinOp shiftr;

	GlowBinOp iadd;
	GlowBinOp isub;
	GlowBinOp imul;
	GlowBinOp idiv;
	GlowBinOp imod;
	GlowBinOp ipow;

	GlowBinOp ibitand;
	GlowBinOp ibitor;
	GlowBinOp ixor;
	GlowBinOp ishiftl;
	GlowBinOp ishiftr;

	GlowBinOp radd;
	GlowBinOp rsub;
	GlowBinOp rmul;
	GlowBinOp rdiv;
	GlowBinOp rmod;
	GlowBinOp rpow;

	GlowBinOp rbitand;
	GlowBinOp rbitor;
	GlowBinOp rxor;
	GlowBinOp rshiftl;
	GlowBinOp rshiftr;

	BoolUnOp nonzero;

	GlowUnOp to_int;
	GlowUnOp to_float;
};

struct glow_seq_methods {
	GlowUnOp len;
	GlowBinOp get;
	GlowSeqSetFunc set;
	GlowBinOp contains;
	GlowBinOp apply;
	GlowBinOp iapply;
};

extern struct glow_num_methods obj_num_methods;
extern struct glow_seq_methods obj_seq_methods;
extern GlowClass glow_obj_class;

#define GLOW_OBJ_INIT_STATIC(class_) { .class = (class_), .refcnt = -1 }
#define GLOW_CLASS_BASE_INIT()       GLOW_OBJ_INIT_STATIC(&glow_meta_class)

struct glow_error;

struct glow_value {
	enum {
		/* nonexistent value */
		GLOW_VAL_TYPE_EMPTY = 0,

		/* standard type classes */
		GLOW_VAL_TYPE_NULL,
		GLOW_VAL_TYPE_BOOL,
		GLOW_VAL_TYPE_INT,
		GLOW_VAL_TYPE_FLOAT,
		GLOW_VAL_TYPE_OBJECT,
		GLOW_VAL_TYPE_EXC,

		/* flags */
		GLOW_VAL_TYPE_ERROR,
		GLOW_VAL_TYPE_UNSUPPORTED_TYPES,
		GLOW_VAL_TYPE_DIV_BY_ZERO
	} type;

	union {
		bool b;
		long i;
		double f;
		void *o;
		struct glow_error *e;
	} data;
};

#define glow_isempty(val)    ((val)->type == GLOW_VAL_TYPE_EMPTY)
#define glow_isbool(val)     ((val)->type == GLOW_VAL_TYPE_BOOL)
#define glow_isnull(val)     ((val)->type == GLOW_VAL_TYPE_NULL)
#define glow_isint(val)      ((val)->type == GLOW_VAL_TYPE_INT)
#define glow_isfloat(val)    ((val)->type == GLOW_VAL_TYPE_FLOAT)
#define glow_isnumber(val)   (glow_isint(val) || glow_isfloat(val))
#define glow_isobject(val)   ((val)->type == GLOW_VAL_TYPE_OBJECT)
#define glow_isexc(val)      ((val)->type == GLOW_VAL_TYPE_EXC)

#define glow_iserror(val)    ((val)->type == GLOW_VAL_TYPE_ERROR || (val)->type == GLOW_VAL_TYPE_EXC)
#define glow_isut(val)       ((val)->type == GLOW_VAL_TYPE_UNSUPPORTED_TYPES)
#define glow_isdbz(val)      ((val)->type == GLOW_VAL_TYPE_DIV_BY_ZERO)

#define glow_boolvalue(val)  ((val)->data.b)
#define glow_intvalue(val)   ((val)->data.i)
#define glow_floatvalue(val) ((val)->data.f)
#define glow_objvalue(val)   ((val)->data.o)
#define glow_errvalue(val)   ((val)->data.e)

#define glow_intvalue_force(val)   (glow_isint(val) ? glow_intvalue(val) : (long)glow_floatvalue(val))
#define glow_floatvalue_force(val) (glow_isint(val) ? (double)glow_intvalue(val) : glow_floatvalue(val))

#define glow_makeempty()     ((GlowValue){.type = GLOW_VAL_TYPE_EMPTY, .data = {.i = 0}})
#define glow_makenull()      ((GlowValue){.type = GLOW_VAL_TYPE_NULL, .data = {.i = 0}})
#define glow_makebool(val)   ((GlowValue){.type = GLOW_VAL_TYPE_BOOL, .data = {.b = (val)}})
#define glow_maketrue()      ((GlowValue){.type = GLOW_VAL_TYPE_BOOL, .data = {.b = 1}})
#define glow_makefalse()     ((GlowValue){.type = GLOW_VAL_TYPE_BOOL, .data = {.b = 0}})
#define glow_makeint(val)    ((GlowValue){.type = GLOW_VAL_TYPE_INT, .data = {.i = (val)}})
#define glow_makefloat(val)  ((GlowValue){.type = GLOW_VAL_TYPE_FLOAT, .data = {.f = (val)}})
#define glow_makeobj(val)    ((GlowValue){.type = GLOW_VAL_TYPE_OBJECT, .data = {.o = (val)}})
#define glow_makeexc(val)    ((GlowValue){.type = GLOW_VAL_TYPE_EXC, .data = {.o = (val)}})

#define glow_makeerr(val)    ((GlowValue){.type = GLOW_VAL_TYPE_ERROR, .data = {.e = (val)}})
#define glow_makeut()        ((GlowValue){.type = GLOW_VAL_TYPE_UNSUPPORTED_TYPES})
#define glow_makedbz()       ((GlowValue){.type = GLOW_VAL_TYPE_DIV_BY_ZERO})

#define GLOW_MAKE_EMPTY()     { .type = GLOW_VAL_TYPE_EMPTY, .data = { .i = 0 } }
#define GLOW_MAKE_NULL()      { .type = GLOW_VAL_TYPE_NULL, .data = { .i = 0 } }
#define GLOW_MAKE_BOOL(val)   { .type = GLOW_VAL_TYPE_BOOL, .data = { .b = (val) } }
#define GLOW_MAKE_TRUE()      { .type = GLOW_VAL_TYPE_BOOL, .data = { .b = 1 } }
#define GLOW_MAKE_FALSE()     { .type = GLOW_VAL_TYPE_BOOL, .data = { .b = 0 } }
#define GLOW_MAKE_INT(val)    { .type = GLOW_VAL_TYPE_INT, .data = { .i = (val) } }
#define GLOW_MAKE_FLOAT(val)  { .type = GLOW_VAL_TYPE_FLOAT, .data = { .f = (val) } }
#define GLOW_MAKE_OBJ(val)    { .type = GLOW_VAL_TYPE_OBJECT, .data = { .o = (val) } }
#define GLOW_MAKE_EXC(val)    { .type = GLOW_VAL_TYPE_EXC, .data = { .o = (val) } }

#define GLOW_MAKE_ERR(val)    { .type = GLOW_VAL_TYPE_ERROR, .data = { .e = (val) } }
#define GLOW_MAKE_UT()        { .type = GLOW_VAL_TYPE_UNSUPPORTED_TYPES }
#define GLOW_MAKE_DBZ()       { .type = GLOW_VAL_TYPE_DIV_BY_ZERO }

GlowClass *glow_getclass(GlowValue *v);

bool glow_is_a(GlowValue *v, GlowClass *class);
bool glow_is_subclass(GlowClass *child, GlowClass *parent);

GlowInitFunc glow_resolve_init(GlowClass *class);
GlowDelFunc glow_resolve_del(GlowClass *class);
GlowBinOp glow_resolve_eq(GlowClass *class);
GlowUnOp glow_resolve_hash(GlowClass *class);
GlowBinOp glow_resolve_cmp(GlowClass *class);
GlowUnOp glow_resolve_str(GlowClass *class);
GlowCallFunc glow_resolve_call(GlowClass *class);
GlowPrintFunc glow_resolve_print(GlowClass *class);
GlowUnOp glow_resolve_iter(GlowClass *class);
GlowUnOp glow_resolve_iternext(GlowClass *class);
GlowAttrGetFunc glow_resolve_attr_get(GlowClass *class);
GlowAttrSetFunc glow_resolve_attr_set(GlowClass *class);

GlowUnOp glow_resolve_plus(GlowClass *class);
GlowUnOp glow_resolve_minus(GlowClass *class);
GlowUnOp glow_resolve_abs(GlowClass *class);
GlowBinOp glow_resolve_add(GlowClass *class);
GlowBinOp glow_resolve_sub(GlowClass *class);
GlowBinOp glow_resolve_mul(GlowClass *class);
GlowBinOp glow_resolve_div(GlowClass *class);
GlowBinOp glow_resolve_mod(GlowClass *class);
GlowBinOp glow_resolve_pow(GlowClass *class);
GlowUnOp glow_resolve_bitnot(GlowClass *class);
GlowBinOp glow_resolve_bitand(GlowClass *class);
GlowBinOp glow_resolve_bitor(GlowClass *class);
GlowBinOp glow_resolve_xor(GlowClass *class);
GlowBinOp glow_resolve_shiftl(GlowClass *class);
GlowBinOp glow_resolve_shiftr(GlowClass *class);
GlowBinOp glow_resolve_iadd(GlowClass *class);
GlowBinOp glow_resolve_isub(GlowClass *class);
GlowBinOp glow_resolve_imul(GlowClass *class);
GlowBinOp glow_resolve_idiv(GlowClass *class);
GlowBinOp glow_resolve_imod(GlowClass *class);
GlowBinOp glow_resolve_ipow(GlowClass *class);
GlowBinOp glow_resolve_ibitand(GlowClass *class);
GlowBinOp glow_resolve_ibitor(GlowClass *class);
GlowBinOp glow_resolve_ixor(GlowClass *class);
GlowBinOp glow_resolve_ishiftl(GlowClass *class);
GlowBinOp glow_resolve_ishiftr(GlowClass *class);
GlowBinOp glow_resolve_radd(GlowClass *class);
GlowBinOp glow_resolve_rsub(GlowClass *class);
GlowBinOp glow_resolve_rmul(GlowClass *class);
GlowBinOp glow_resolve_rdiv(GlowClass *class);
GlowBinOp glow_resolve_rmod(GlowClass *class);
GlowBinOp glow_resolve_rpow(GlowClass *class);
GlowBinOp glow_resolve_rbitand(GlowClass *class);
GlowBinOp glow_resolve_rbitor(GlowClass *class);
GlowBinOp glow_resolve_rxor(GlowClass *class);
GlowBinOp glow_resolve_rshiftl(GlowClass *class);
GlowBinOp glow_resolve_rshiftr(GlowClass *class);
BoolUnOp glow_resolve_nonzero(GlowClass *class);
GlowUnOp glow_resolve_to_int(GlowClass *class);
GlowUnOp glow_resolve_to_float(GlowClass *class);

GlowUnOp glow_resolve_len(GlowClass *class);
GlowBinOp glow_resolve_get(GlowClass *class);
GlowSeqSetFunc glow_resolve_set(GlowClass *class);
GlowBinOp glow_resolve_contains(GlowClass *class);
GlowBinOp glow_resolve_apply(GlowClass *class);
GlowBinOp glow_resolve_iapply(GlowClass *class);

void *glow_obj_alloc(GlowClass *class);
void *glow_obj_alloc_var(GlowClass *class, size_t extra);

GlowValue glow_class_instantiate(GlowClass *class, GlowValue *args, size_t nargs);

void glow_retaino(void *o);
void glow_releaseo(void *o);
void glow_destroyo(void *o);

void glow_retain(GlowValue *v);
void glow_release(GlowValue *v);
void glow_destroy(GlowValue *v);

struct glow_value_array {
	GlowValue *array;
	size_t length;
};

void glow_class_init(GlowClass *class);

#define GLOW_SAVED_TID_FIELD_NAME _saved_id
#define GLOW_SAVED_TID_FIELD pthread_t GLOW_SAVED_TID_FIELD_NAME;

#define GLOW_CHECK_THREAD(o) \
	if ((o)->GLOW_SAVED_TID_FIELD_NAME != pthread_self()) \
		return GLOW_CONC_ACCS_EXC("invalid concurrent access of non-thread-safe method or function");

#define GLOW_INIT_SAVED_TID_FIELD(o) (o)->GLOW_SAVED_TID_FIELD_NAME = pthread_self()
#undef GLOW_SAVED_TID_FIELD_NAME

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} GlowMonitor;

bool glow_object_set_monitor(GlowObject *o);
bool glow_object_enter(GlowObject *o);
bool glow_object_exit(GlowObject *o);

#define GLOW_ENTER(o) if (!glow_object_enter((GlowObject *)(o))) GLOW_CHECK_THREAD(o)
#define GLOW_EXIT(o)  (glow_object_exit((GlowObject *)(o)))

#endif /* GLOW_OBJECT_H */
