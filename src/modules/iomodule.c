#include <stdlib.h>
#include "object.h"
#include "strobject.h"
#include "fileobject.h"
#include "nativefunc.h"
#include "exc.h"
#include "module.h"
#include "builtins.h"
#include "strdict.h"
#include "iomodule.h"

static GlowValue open_file(GlowValue *args, size_t nargs)
{
#define NAME "open"

	GLOW_ARG_COUNT_CHECK_BETWEEN(NAME, nargs, 1, 2);

	if (nargs == 2) {
		if (!glow_is_a(&args[0], &glow_str_class) || !glow_is_a(&args[0], &glow_str_class)) {
			return glow_type_exc_unsupported_2(NAME, glow_getclass(&args[0]), glow_getclass(&args[1]));
		}

		GlowStrObject *filename = glow_objvalue(&args[0]);
		GlowStrObject *mode = glow_objvalue(&args[1]);

		return glow_file_make(filename->str.value, mode->str.value);
	} else {
		if (!glow_is_a(&args[0], &glow_str_class)) {
			return glow_type_exc_unsupported_1(NAME, glow_getclass(&args[0]));
		}

		GlowStrObject *filename = glow_objvalue(&args[0]);

		return glow_file_make(filename->str.value, "r");
	}

#undef NAME
}

static GlowNativeFuncObject open_file_nfo = GLOW_NFUNC_INIT(open_file);

const struct glow_builtin io_builtins[] = {
		{"open",  GLOW_MAKE_OBJ(&open_file_nfo)},
		{NULL,  GLOW_MAKE_EMPTY()},
};

GlowBuiltInModule glow_io_module = GLOW_BUILTIN_MODULE_INIT_STATIC("io", &io_builtins[0]);
