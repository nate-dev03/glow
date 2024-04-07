#ifndef GLOW_BUILTINS_H
#define GLOW_BUILTINS_H

#include "object.h"
#include "module.h"

struct glow_builtin {
	const char *name;
	GlowValue value;
};

/* glow_builtins[last].name == NULL */
extern const struct glow_builtin glow_builtins[];

/* glow_builtin_modules[last] == NULL */
extern const GlowModule *glow_builtin_modules[];

#endif /* GLOW_BUILTINS_H */
