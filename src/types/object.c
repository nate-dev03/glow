#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "util.h"
#include "null.h"
#include "boolobject.h"
#include "intobject.h"
#include "floatobject.h"
#include "strobject.h"
#include "exc.h"
#include "object.h"

static GlowValue obj_init(GlowValue *this, GlowValue *args, size_t nargs)
{
	GLOW_UNUSED(args);

	if (nargs > 0) {
		return GLOW_TYPE_EXC("Object constructor takes no arguments (got %lu)", nargs);
	}

	return *this;
}

static GlowValue obj_eq(GlowValue *this, GlowValue *other)
{
	if (!glow_isobject(other)) {
		return glow_makefalse();
	}
	return glow_makebool(glow_objvalue(this) == glow_objvalue(other));
}

static GlowValue obj_str(GlowValue *this)
{
#define STR_MAX_LEN 50
	char buf[STR_MAX_LEN];
	size_t len = snprintf(buf, STR_MAX_LEN, "<%s at %p>", glow_getclass(this)->name, glow_objvalue(this));
	assert(len > 0);

	if (len > STR_MAX_LEN) {
		len = STR_MAX_LEN;
	}

	return glow_strobj_make_direct(buf, len);
#undef STR_MAX_LEN
}

static bool obj_nonzero(GlowValue *this)
{
	GLOW_UNUSED(this);
	return true;
}

static void obj_free(GlowValue *this)
{
	free(glow_objvalue(this));
}

struct glow_num_methods obj_num_methods = {
	NULL,    /* plus */
	NULL,    /* minus */
	NULL,    /* abs */

	NULL,    /* add */
	NULL,    /* sub */
	NULL,    /* mul */
	NULL,    /* div */
	NULL,    /* mod */
	NULL,    /* pow */

	NULL,    /* bitnot */
	NULL,    /* bitand */
	NULL,    /* bitor */
	NULL,    /* xor */
	NULL,    /* shiftl */
	NULL,    /* shiftr */

	NULL,    /* iadd */
	NULL,    /* isub */
	NULL,    /* imul */
	NULL,    /* idiv */
	NULL,    /* imod */
	NULL,    /* ipow */

	NULL,    /* ibitand */
	NULL,    /* ibitor */
	NULL,    /* ixor */
	NULL,    /* ishiftl */
	NULL,    /* ishiftr */

	NULL,    /* radd */
	NULL,    /* rsub */
	NULL,    /* rmul */
	NULL,    /* rdiv */
	NULL,    /* rmod */
	NULL,    /* rpow */

	NULL,    /* rbitand */
	NULL,    /* rbitor */
	NULL,    /* rxor */
	NULL,    /* rshiftl */
	NULL,    /* rshiftr */

	obj_nonzero,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct glow_seq_methods obj_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_obj_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Object",
	.super = NULL,

	.instance_size = sizeof(GlowObject),

	.init = obj_init,
	.del = obj_free,

	.eq = obj_eq,
	.hash = NULL,
	.cmp = NULL,
	.str = obj_str,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &obj_num_methods,
	.seq_methods = &obj_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass *glow_getclass(GlowValue *v)
{
	if (v == NULL) {
		return NULL;
	}

	switch (v->type) {
	case GLOW_VAL_TYPE_NULL:
		return &glow_null_class;
	case GLOW_VAL_TYPE_BOOL:
		return &glow_bool_class;
	case GLOW_VAL_TYPE_INT:
		return &glow_int_class;
	case GLOW_VAL_TYPE_FLOAT:
		return &glow_float_class;
	case GLOW_VAL_TYPE_OBJECT:
	case GLOW_VAL_TYPE_EXC: {
		const GlowObject *o = glow_objvalue(v);
		return o->class;
	case GLOW_VAL_TYPE_EMPTY:
	case GLOW_VAL_TYPE_ERROR:
	case GLOW_VAL_TYPE_UNSUPPORTED_TYPES:
	case GLOW_VAL_TYPE_DIV_BY_ZERO:
		GLOW_INTERNAL_ERROR();
		return NULL;
	}
	}

	GLOW_INTERNAL_ERROR();
	return NULL;
}

/*
 * Generic "is a" -- checks if the given object is
 * an instance of the given class or any of its super-
 * classes. This is subject to change if support for
 * multiple-inheritance is ever added.
 */
bool glow_is_a(GlowValue *v, GlowClass *class)
{
	return glow_is_subclass(glow_getclass(v), class);
}

bool glow_is_subclass(GlowClass *child, GlowClass *parent)
{
	while (child != NULL) {
		if (child == parent) {
			return true;
		} else if (child == &glow_meta_class) {
			return false;
		}
		child = child->super;
	}
	return false;
}

#define MAKE_METHOD_RESOLVER_DIRECT(name, type) \
type glow_resolve_##name(GlowClass *class) { \
	GlowClass *target = class; \
	type op; \
	while (target != NULL && (op = target->name) == NULL) { \
		if (target == target->super) { \
			return glow_obj_class.name; \
		} \
		target = target->super; \
	} \
\
	if (target == NULL) { \
		return glow_obj_class.name; \
	} \
\
	class->name = op; \
	return op; \
}

#define MAKE_METHOD_RESOLVER(name, category, type) \
type glow_resolve_##name(GlowClass *class) { \
	if (class->category == NULL) { \
		return glow_obj_class.category->name; \
	} \
\
	GlowClass *target = class; \
	type op; \
	while (target != NULL && (op = target->category->name) == NULL) { \
		if (target == target->super) { \
			return glow_obj_class.category->name; \
		} \
		target = target->super; \
	} \
\
	if (target == NULL) { \
		return glow_obj_class.category->name; \
	} \
\
	class->category->name = op; \
	return op; \
}

/*
 * Initializers should not be inherited.
 */
GlowInitFunc glow_resolve_init(GlowClass *class)
{
	return class->init;
}

/*
 * Every class should implement `del`.
 */
GlowDelFunc glow_resolve_del(GlowClass *class)
{
	return class->del;
}

MAKE_METHOD_RESOLVER_DIRECT(eq, GlowBinOp)
MAKE_METHOD_RESOLVER_DIRECT(hash, GlowUnOp)
MAKE_METHOD_RESOLVER_DIRECT(cmp, GlowBinOp)
MAKE_METHOD_RESOLVER_DIRECT(str, GlowUnOp)
MAKE_METHOD_RESOLVER_DIRECT(call, GlowCallFunc)
MAKE_METHOD_RESOLVER_DIRECT(print, GlowPrintFunc)
MAKE_METHOD_RESOLVER_DIRECT(iter, GlowUnOp)
MAKE_METHOD_RESOLVER_DIRECT(iternext, GlowUnOp)
MAKE_METHOD_RESOLVER_DIRECT(attr_get, GlowAttrGetFunc)
MAKE_METHOD_RESOLVER_DIRECT(attr_set, GlowAttrSetFunc)

MAKE_METHOD_RESOLVER(plus, num_methods, GlowUnOp)
MAKE_METHOD_RESOLVER(minus, num_methods, GlowUnOp)
MAKE_METHOD_RESOLVER(abs, num_methods, GlowUnOp)
MAKE_METHOD_RESOLVER(add, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(sub, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(mul, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(div, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(mod, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(pow, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(bitnot, num_methods, GlowUnOp)
MAKE_METHOD_RESOLVER(bitand, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(bitor, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(xor, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(shiftl, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(shiftr, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(iadd, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(isub, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(imul, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(idiv, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(imod, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(ipow, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(ibitand, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(ibitor, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(ixor, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(ishiftl, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(ishiftr, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(radd, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rsub, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rmul, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rdiv, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rmod, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rpow, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rbitand, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rbitor, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rxor, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rshiftl, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(rshiftr, num_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(nonzero, num_methods, BoolUnOp)
MAKE_METHOD_RESOLVER(to_int, num_methods, GlowUnOp)
MAKE_METHOD_RESOLVER(to_float, num_methods, GlowUnOp)

MAKE_METHOD_RESOLVER(len, seq_methods, GlowUnOp)
MAKE_METHOD_RESOLVER(get, seq_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(set, seq_methods, GlowSeqSetFunc)
MAKE_METHOD_RESOLVER(contains, seq_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(apply, seq_methods, GlowBinOp)
MAKE_METHOD_RESOLVER(iapply, seq_methods, GlowBinOp)

#undef MAKE_METHOD_RESOLVER_DIRECT
#undef MAKE_METHOD_RESOLVER

void *glow_obj_alloc(GlowClass *class)
{
	return glow_obj_alloc_var(class, 0);
}

void *glow_obj_alloc_var(GlowClass *class, size_t extra)
{
	GlowObject *o = glow_malloc(class->instance_size + extra);
	o->class = class;
	o->refcnt = 1;
	o->monitor = 0;
	return o;
}

GlowValue glow_class_instantiate(GlowClass *class, GlowValue *args, size_t nargs)
{
	if (class == &glow_null_class) {
		return glow_makenull();
	} else if (class == &glow_int_class) {
		return glow_makefalse();
	} else if (class == &glow_float_class) {
		return glow_makefloat(0);
	} else {
		GlowInitFunc init = glow_resolve_init(class);

		if (!init) {
			return glow_type_exc_cannot_instantiate(class);
		}

		GlowValue instance = glow_makeobj(glow_obj_alloc(class));
		init(&instance, args, nargs);
		return instance;
	}
}

void glow_retaino(void *p)
{
	GlowObject *o = p;
	if (o->refcnt != (unsigned)(-1)) {
		GLOW_UNUSED(++o->refcnt);
	}
}

void glow_releaseo(void *p)
{
	GlowObject *o = p;
	if (o->refcnt != (unsigned)(-1) && --o->refcnt == 0) {
		glow_destroyo(o);
	}
}

void glow_destroyo(void *p)
{
	GlowObject *o = p;
	o->class->del(&glow_makeobj(o));
}

void glow_retain(GlowValue *v)
{
	if (v == NULL || !(glow_isobject(v) || glow_isexc(v))) {
		return;
	}
	glow_retaino(glow_objvalue(v));
}

void glow_release(GlowValue *v)
{
	if (v == NULL || !(glow_isobject(v) || glow_isexc(v))) {
		return;
	}
	glow_releaseo(glow_objvalue(v));
}

void glow_destroy(GlowValue *v)
{
	if (v == NULL || v->type != GLOW_VAL_TYPE_OBJECT) {
		return;
	}
	GlowObject *o = glow_objvalue(v);
	o->class->del(v);
}

void glow_class_init(GlowClass *class)
{
	/* initialize attributes */
	size_t max_size = 0;

	if (class->members != NULL) {
		for (size_t i = 0; class->members[i].name != NULL; i++) {
			++max_size;
		}
	}

	if (class->methods != NULL) {
		for (struct glow_attr_method *m = class->methods; m->name != NULL; m++) {
			++max_size;
		}
	}

	glow_attr_dict_init(&class->attr_dict, max_size);
	glow_attr_dict_register_members(&class->attr_dict, class->members);
	glow_attr_dict_register_methods(&class->attr_dict, class->methods);
}

static pthread_mutex_t monitor_management_mutex = PTHREAD_MUTEX_INITIALIZER;

static void monotir_init(GlowMonitor *monitor)
{
	GLOW_SAFE(pthread_mutex_init(&monitor->mutex, NULL));
	GLOW_SAFE(pthread_cond_init(&monitor->cond, NULL));
}

static void monitor_destroy(GlowMonitor *monitor)
{
	GLOW_SAFE(pthread_mutex_destroy(&monitor->mutex));
	GLOW_SAFE(pthread_cond_destroy(&monitor->cond));
}

static GlowMonitor *volatile monitors;
#define MONITORS_INIT_CAPACITY (1 << 4)
#define MONITORS_MAX_CAPACITY  (1 << 16)
static size_t monotirs_capacity = 0;
static unsigned int monitors_next = 0;
static bool monitors_full = false;

static void free_monitors(void)
{
	for (size_t i = 1; i < monitors_next; i++) {
		monitor_destroy(&monitors[i]);
	}

	free(monitors);
}

bool glow_object_set_monitor(GlowObject *o)
{
	if (o->refcnt > 1 || o->monitor != 0) {
		return false;
	}

	GLOW_SAFE(pthread_mutex_lock(&monitor_management_mutex));
	if (monotirs_capacity == 0) {
		monitors = glow_malloc(MONITORS_INIT_CAPACITY * sizeof(GlowMonitor));
		monotirs_capacity = MONITORS_INIT_CAPACITY;
		monitors_next = 1;
		monitors_full = false;
		atexit(free_monitors);
	}

	if (monitors_next == monotirs_capacity) {
		if (monotirs_capacity < MONITORS_MAX_CAPACITY) {
			monotirs_capacity *= 2;
			monitors = glow_realloc(monitors, monotirs_capacity * sizeof(GlowMonitor));
		} else {
			monitors_next = 1;
			monitors_full = true;
		}
	}

	if (!monitors_full) {
		monotir_init(&monitors[monitors_next]);
	}

	o->monitor = monitors_next++;
	GLOW_SAFE(pthread_mutex_unlock(&monitor_management_mutex));
	return true;
}

bool glow_object_enter(GlowObject *o)
{
	if (o->monitor == 0) {
		return false;
	}

	GlowMonitor *monitor = &monitors[o->monitor];
	GLOW_SAFE(pthread_mutex_lock(&monitor->mutex));
	return true;
}

bool glow_object_exit(GlowObject *o)
{
	if (o->monitor == 0) {
		return false;
	}

	GlowMonitor *monitor = &monitors[o->monitor];
	GLOW_SAFE(pthread_mutex_unlock(&monitor->mutex));
	return true;
}
