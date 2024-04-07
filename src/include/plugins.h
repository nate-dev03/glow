#include "main.h"
#if GLOW_IS_POSIX
#ifndef GLOW_PLUGINS_H
#define GLOW_PLUGINS_H

#include "object.h"

#define GLOW_PLUGIN_PATH_ENV "GLOW_PLUGINS_PATH"

typedef struct {
	GlowValue module;
} GlowPlugin;

typedef void (*GlowPluginInitFunc)(GlowPlugin *plugin);

void glow_set_plugin_path(const char *path);
int glow_reload_plugins(void);

#endif /* GLOW_PLUGINS_H */
#endif /* GLOW_IS_POSIX */
