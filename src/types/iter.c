#include <stdlib.h>
#include <stdbool.h>
#include "object.h"
#include "exc.h"
#include "iter.h"

/* Base Iter */
static void iter_free(GlowValue *this)
{
	glow_obj_class.del(this);
}

static GlowValue iter_iter(GlowValue *this)
{
	glow_retain(this);
	return *this;
}

static GlowValue iter_apply(GlowValue *this, GlowValue *fn)
{
	GlowIter *iter = glow_objvalue(this);
	return glow_applied_iter_make(iter, fn);
}

struct glow_seq_methods iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	iter_apply,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_iter_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Iter",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowIter),

	.init = NULL,
	.del = iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = iter_iter,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = &iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* IterStop */
GlowValue glow_get_iter_stop(void)
{
	static GlowIterStop iter_stop = { .base = GLOW_OBJ_INIT_STATIC(&glow_iter_stop_class) };
	return glow_makeobj(&iter_stop);
}

static void iter_stop_free(GlowValue *this)
{
	glow_obj_class.del(this);
}

GlowClass glow_iter_stop_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "IterStop",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowIterStop),

	.init = NULL,
	.del = iter_stop_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* AppliedIter -- result of for e.g. `function @ iter` */
GlowValue glow_applied_iter_make(GlowIter *source, GlowValue *fn)
{
	GlowAppliedIter *appiter = glow_obj_alloc(&glow_applied_iter_class);
	glow_retaino(source);
	glow_retain(fn);
	appiter->source = source;
	appiter->fn = *fn;
	return glow_makeobj(appiter);
}

static void applied_iter_free(GlowValue *this)
{
	GlowAppliedIter *appiter = glow_objvalue(this);
	glow_releaseo(appiter->source);
	glow_release(&appiter->fn);
	glow_iter_class.del(this);
}

static GlowValue applied_iter_iternext(GlowValue *this)
{
	GlowAppliedIter *appiter = glow_objvalue(this);
	GlowIter *source = appiter->source;
	GlowClass *source_class = source->base.class;
	GlowUnOp iternext = glow_resolve_iternext(source_class);

	if (!iternext) {
		return glow_type_exc_not_iterator(source_class);
	}

	GlowValue *fn = &appiter->fn;
	GlowClass *fn_class = glow_getclass(fn);
	GlowCallFunc call = glow_resolve_call(fn_class);

	if (!call) {
		return glow_type_exc_not_callable(fn_class);
	}

	GlowValue next = iternext(&glow_makeobj(source));

	if (glow_is_iter_stop(&next) || glow_iserror(&next)) {
		return next;
	}

	GlowValue ret = call(fn, &next, NULL, 1, 0);
	glow_release(&next);
	return ret;
}

struct glow_seq_methods applied_iter_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_applied_iter_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "AppliedIter",
	.super = &glow_iter_class,

	.instance_size = sizeof(GlowAppliedIter),

	.init = NULL,
	.del = applied_iter_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = applied_iter_iternext,

	.num_methods = NULL,
	.seq_methods = &applied_iter_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

/* Range iterators */
GlowValue glow_range_make(GlowValue *from, GlowValue *to)
{
	if (!(glow_isint(from) && glow_isint(to))) {
		return glow_type_exc_unsupported_2("..", glow_getclass(from), glow_getclass(to));
	}

	GlowRange *range = glow_obj_alloc(&glow_range_class);
	GLOW_INIT_SAVED_TID_FIELD(range);
	range->from = range->i = glow_intvalue(from);
	range->to = glow_intvalue(to);
	return glow_makeobj(range);
}

static void range_free(GlowValue *this)
{
	glow_iter_class.del(this);
}

static GlowValue range_iternext(GlowValue *this)
{
	GlowRange *range = glow_objvalue(this);

	GLOW_ENTER(range);
	const long from = range->from;
	const long to = range->to;
	const long i = range->i;

	if (to >= from) {
		if (i < to) {
			++(range->i);
			GLOW_EXIT(range);
			return glow_makeint(i);
		} else {
			GLOW_EXIT(range);
			return glow_get_iter_stop();
		}
	} else {
		if (i >= to) {
			--(range->i);
			GLOW_EXIT(range);
			return glow_makeint(i);
		} else {
			GLOW_EXIT(range);
			return glow_get_iter_stop();
		}
	}
}

static GlowValue range_contains(GlowValue *this, GlowValue *n)
{
	if (!glow_isint(n)) {
		return glow_makefalse();
	}

	GlowRange *range = glow_objvalue(this);
	const long target = glow_intvalue(n);
	const long from = range->from;
	const long to = range->to;

	if (to >= from) {
		return glow_makebool((from <= target && target < to));
	} else {
		return glow_makebool((to <= target && target <= from));
	}
}

struct glow_seq_methods range_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	range_contains,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_range_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Range",
	.super = &glow_iter_class,

	.instance_size = sizeof(GlowRange),

	.init = NULL,
	.del = range_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = range_iternext,

	.num_methods = NULL,
	.seq_methods = &range_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};
