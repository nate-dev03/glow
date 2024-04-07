#ifndef GLOW_MODULE_H
#define GLOW_MODULE_H

#include "object.h"
#include "strdict.h"

extern GlowClass glow_module_class;

typedef struct {
	GlowObject base;
	const char *name;
	GlowStrDict contents;
} GlowModule;

GlowValue glow_module_make(const char *name, GlowStrDict *contents);

extern GlowClass glow_builtin_module_class;

/* builtins.h */
struct glow_builtin;

typedef struct glow_built_in_module {
	GlowModule base;
	const struct glow_builtin *members;
	bool initialized;
} GlowBuiltInModule;

#define GLOW_BUILTIN_MODULE_INIT_STATIC(name_, members_) { \
  .base = { .base = GLOW_OBJ_INIT_STATIC(&glow_builtin_module_class), .name = (name_) }, \
  .members = (members_), \
  .initialized = false \
}

#endif /* GLOW_MODULE_H */
