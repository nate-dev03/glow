#ifndef GLOW_LOADER_H
#define GLOW_LOADER_H

#include "code.h"

#define GLOW_EXT  ".glow"
#define GLOWC_EXT ".glowc"

enum {
	GLOW_LOAD_ERR_NONE,
	GLOW_LOAD_ERR_NOT_FOUND,
	GLOW_LOAD_ERR_INVALID_SIGNATURE
};

int glow_load_from_file(const char *name, const bool name_has_ext, GlowCode *dest);

#endif /* GLOW_LOADER_H */
