#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "exc.h"
#include "builtins.h"
#include "str.h"
#include "strdict.h"
#include "util.h"
#include "module.h"

GlowValue glow_module_make(const char *name, GlowStrDict *contents)
{
	GlowModule *mod = glow_obj_alloc(&glow_module_class);
	mod->name = name;
	mod->contents = *contents;
	return glow_makeobj(mod);
}

static void module_free(GlowValue *this)
{
	GlowModule *mod = glow_objvalue(this);
	glow_strdict_dealloc(&mod->contents);
	glow_obj_class.del(this);
}

static GlowValue module_attr_get(GlowValue *this, const char *attr)
{
	GlowModule *mod = glow_objvalue(this);
	GlowStr key = GLOW_STR_INIT(attr, strlen(attr), 0);
	GlowValue v = glow_strdict_get(&mod->contents, &key);

	if (glow_isempty(&v)) {
		return glow_attr_exc_not_found(&glow_module_class, attr);
	}

	glow_retain(&v);
	return v;
}

static GlowValue module_attr_set(GlowValue *this, const char *attr, GlowValue *v)
{
	GLOW_UNUSED(attr);
	GLOW_UNUSED(v);
	GlowModule *mod = glow_objvalue(this);
	return GLOW_ATTR_EXC("cannot re-assign attributes of module '%s'", mod->name);
}

struct glow_num_methods module_num_methods = {
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

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct glow_seq_methods module_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_module_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "Module",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowModule),

	.init = NULL,
	.del = module_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &module_num_methods,
	.seq_methods = &module_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = module_attr_get,
	.attr_set = module_attr_set
};

static void builtin_module_init(GlowBuiltInModule *mod)
{
	GlowStrDict *dict = &mod->base.contents;
	const struct glow_builtin *members = mod->members;
	for (size_t i = 0; members[i].name != NULL; i++) {
		glow_strdict_put(dict, members[i].name, (GlowValue *)&members[i].value, false);
	}
}

static GlowValue builtin_module_attr_get(GlowValue *this, const char *attr)
{
	GlowBuiltInModule *mod = glow_objvalue(this);

	if (!mod->initialized) {
		glow_strdict_init(&mod->base.contents);
		builtin_module_init(mod);
		mod->initialized = true;
	}

	return glow_module_class.attr_get(this, attr);
}

static void builtin_module_free(GlowValue *this)
{
	glow_module_class.del(this);
}

struct glow_num_methods builtin_module_num_methods = {
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

	NULL,    /* nonzero */

	NULL,    /* to_int */
	NULL,    /* to_float */
};

struct glow_seq_methods builtin_module_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

GlowClass glow_builtin_module_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "BuiltInModule",
	.super = &glow_module_class,

	.instance_size = sizeof(GlowBuiltInModule),

	.init = NULL,
	.del = builtin_module_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = NULL,
	.iternext = NULL,

	.num_methods = &builtin_module_num_methods,
	.seq_methods = &builtin_module_seq_methods,

	.members = NULL,
	.methods = NULL,

	.attr_get = builtin_module_attr_get,
	.attr_set = NULL   /* inherit from Module */
};
