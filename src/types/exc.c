#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "object.h"
#include "strobject.h"
#include "util.h"
#include "err.h"
#include "exc.h"

GlowValue glow_exc_make(GlowClass *exc_class, bool active, const char *msg_format, ...)
{
#define EXC_MSG_BUF_SIZE 200

	char msg_static[EXC_MSG_BUF_SIZE];

	GlowException *exc = glow_obj_alloc(exc_class);
	glow_tb_manager_init(&exc->tbm);

	va_list args;
	va_start(args, msg_format);
	int size = vsnprintf(msg_static, EXC_MSG_BUF_SIZE, msg_format, args);
	assert(size >= 0);
	va_end(args);

	if (size >= EXC_MSG_BUF_SIZE)
		size = EXC_MSG_BUF_SIZE;

	char *msg = glow_malloc(size + 1);
	strcpy(msg, msg_static);
	exc->msg = msg;

	return active ? glow_makeexc(exc) : glow_makeobj(exc);

#undef EXC_MSG_BUF_SIZE
}

void glow_exc_traceback_append(GlowException *e,
                              const char *fn,
                              const unsigned int lineno)
{
	glow_tb_manager_add(&e->tbm, fn, lineno);
}

void glow_exc_traceback_print(GlowException *e, FILE *out)
{
	glow_tb_manager_print(&e->tbm, out);
}

void glow_exc_print_msg(GlowException *e, FILE *out)
{
	if (e->msg != NULL) {
		fprintf(out, "%s: %s\n", e->base.class->name, e->msg);
	} else {
		fprintf(out, "%s\n", e->base.class->name);
	}
}

/* Base Exception */

static GlowValue exc_init(GlowValue *this, GlowValue *args, size_t nargs)
{
	if (nargs > 1) {
		return glow_makeerr(glow_err_new(GLOW_ERR_TYPE_TYPE,
		                               "Exception constructor takes at most 1 argument (got %lu)",
		                               nargs));
	}

	GlowException *e = glow_objvalue(this);
	glow_tb_manager_init(&e->tbm);

	if (nargs == 0) {
		e->msg = NULL;
	} else {
		if (!glow_is_a(&args[0], &glow_str_class)) {
			GlowClass *class = glow_getclass(&args[0]);
			return glow_makeerr(glow_err_new(GLOW_ERR_TYPE_TYPE,
			                               "Exception constructor takes a Str argument, not a %s",
			                               class->name));
		}

		GlowStrObject *str = glow_objvalue(&args[0]);
		char *str_copy = glow_malloc(str->str.len + 1);
		strcpy(str_copy, str->str.value);
		e->msg = str_copy;
	}

	return *this;
}

static void exc_free(GlowValue *this)
{
	GlowException *exc = glow_objvalue(this);
	GLOW_FREE(exc->msg);
	glow_tb_manager_dealloc(&exc->tbm);
	glow_obj_class.del(this);
}

GlowClass glow_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Exception",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowException),

	.init = exc_init,
	.del = exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

static GlowValue sub_exc_init(GlowValue *this, GlowValue *args, size_t nargs)
{
	return exc_init(this, args, nargs);
}

static void sub_exc_free(GlowValue *this)
{
	glow_exception_class.del(this);
}

GlowClass glow_index_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "IndexException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowIndexException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_type_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "TypeException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowTypeException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_io_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "IOException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowIOException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_attr_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "AttributeException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowAttributeException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_import_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "ImportException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowImportException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_isc_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "IllegalStateChangeException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowIllegalStateChangeException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_seq_exp_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "SequenceExpandException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowSequenceExpandException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_actor_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "ActorException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowActorException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};

GlowClass glow_conc_access_exception_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "ConcurrentAccessException",
	.super = &glow_exception_class,

	.instance_size = sizeof(GlowConcurrentAccessException),

	.init = sub_exc_init,
	.del = sub_exc_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = NULL,
	.seq_methods  = NULL,

	.members = NULL,
	.methods = NULL,

	.attr_get = NULL,
	.attr_set = NULL
};


/* Common exceptions */

GlowValue glow_type_exc_unsupported_1(const char *op, const GlowClass *c1)
{
	return GLOW_TYPE_EXC("unsupported operand type for %s: '%s'", op, c1->name);
}

GlowValue glow_type_exc_unsupported_2(const char *op, const GlowClass *c1, const GlowClass *c2)
{
	return GLOW_TYPE_EXC("unsupported operand types for %s: '%s' and '%s'",
	                    op,
	                    c1->name,
	                    c2->name);
}

GlowValue glow_type_exc_cannot_index(const GlowClass *c1)
{
	return GLOW_TYPE_EXC("type '%s' does not support indexing", c1->name);
}

GlowValue glow_type_exc_cannot_apply(const GlowClass *c1)
{
	return GLOW_TYPE_EXC("type '%s' does not support function application", c1->name);
}

GlowValue glow_type_exc_cannot_instantiate(const GlowClass *c1)
{
	return GLOW_TYPE_EXC("class '%s' cannot be instantiated", c1->name);
}

GlowValue glow_type_exc_not_callable(const GlowClass *c1)
{
	return GLOW_TYPE_EXC("object of type '%s' is not callable", c1->name);
}

GlowValue glow_type_exc_not_iterable(const GlowClass *c1)
{
	return GLOW_TYPE_EXC("object of type '%s' is not iterable", c1->name);
}

GlowValue glow_type_exc_not_iterator(const GlowClass *c1)
{
	return GLOW_TYPE_EXC("object of type '%s' is not an iterator", c1->name);
}

GlowValue glow_type_exc_hint_mismatch(const GlowClass *got, const GlowClass *expected)
{
	return GLOW_TYPE_EXC("hint mismatch: %s is not a %s", got->name, expected->name);
}


GlowValue glow_call_exc_num_args(const char *fn, unsigned int got, unsigned int expected)
{
	return GLOW_TYPE_EXC("function %s(): expected %u arguments, got %u",
	                    fn,
	                    expected,
	                    got);
}

GlowValue glow_call_exc_num_args_at_most(const char *fn, unsigned int got, unsigned int expected)
{
	return GLOW_TYPE_EXC("function %s(): expected at most %u arguments, got %u",
	                    fn,
	                    expected,
	                    got);
}

GlowValue glow_call_exc_num_args_between(const char *fn, unsigned int got, unsigned int min, unsigned int max)
{
	return GLOW_TYPE_EXC("function %s(): expected %u-%u arguments, got %u",
	                    fn,
	                    min, max,
	                    got);
}

GlowValue glow_call_exc_named_args(const char *fn)
{
	return GLOW_TYPE_EXC("function %s(): got unexpected named arguments", fn);
}

GlowValue glow_call_exc_dup_arg(const char *fn, const char *name)
{
	return GLOW_TYPE_EXC("function %s(): duplicate argument for parameter '%s'",
	                fn,
	                name);
}

GlowValue glow_call_exc_unknown_arg(const char *fn, const char *name)
{
	return GLOW_TYPE_EXC("function %s(): unknown parameter name '%s'",
	                fn,
	                name);
}

GlowValue glow_call_exc_missing_arg(const char *fn, const char *name)
{
	return GLOW_TYPE_EXC("function %s(): missing argument for parameter '%s'",
	                fn,
	                name);
}

GlowValue glow_call_exc_native_named_args(void)
{
	return GLOW_TYPE_EXC("native functions do not take named arguments");
}

GlowValue glow_call_exc_constructor_named_args(void)
{
	return GLOW_TYPE_EXC("constructors do not take named arguments");
}

GlowValue glow_io_exc_cannot_open_file(const char *filename, const char *mode)
{
	return GLOW_IO_EXC("cannot open file '%s' in mode '%s'", filename, mode);
}

GlowValue glow_io_exc_cannot_read_file(const char *filename)
{
	return GLOW_IO_EXC("cannot read from file '%s'", filename);
}

GlowValue glow_io_exc_cannot_write_file(const char *filename)
{
	return GLOW_IO_EXC("cannot write to file '%s'", filename);
}

GlowValue glow_io_exc_file_closed(const char *filename)
{
	return GLOW_IO_EXC("file '%s' has been closed", filename);
}

GlowValue glow_attr_exc_not_found(const GlowClass *type, const char *attr)
{
	return GLOW_ATTR_EXC("object of type '%s' has no attribute '%s'",
	                type->name,
	                attr);
}

GlowValue glow_attr_exc_readonly(const GlowClass *type, const char *attr)
{
	return GLOW_ATTR_EXC("attribute '%s' of type '%s' object is read-only",
	                attr,
	                type->name);
}

GlowValue glow_attr_exc_mismatch(const GlowClass *type, const char *attr, const GlowClass *assign_type)
{
	return GLOW_ATTR_EXC("cannot assign '%s' to attribute '%s' of '%s' object",
	                assign_type->name,
	                attr,
	                type->name);
}

GlowValue glow_import_exc_not_found(const char *name)
{
	return GLOW_IMPORT_EXC("cannot find module '%s'", name);
}

GlowValue glow_seq_exp_exc_inconsistent(const unsigned int got, const unsigned int expected)
{
	if (got > expected) {
		return GLOW_SEQ_EXP_EXC("too many values to expand (got %u, expected %u)", got, expected);
	} else {
		return GLOW_SEQ_EXP_EXC("too few values to expand (got %u, expected %u)", got, expected);
	}
}
