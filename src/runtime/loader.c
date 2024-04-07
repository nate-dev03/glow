#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "code.h"
#include "compiler.h"
#include "err.h"
#include "util.h"
#include "loader.h"

int glow_load_from_file(const char *name, const bool name_has_ext, GlowCode *dest)
{
	FILE *compiled;

	if (name_has_ext) {
		compiled = fopen(name, "rb");
	} else {
		char *filename_buf = glow_malloc(strlen(name) + strlen(GLOWC_EXT) + 1);
		strcpy(filename_buf, name);
		strcat(filename_buf, GLOWC_EXT);
		compiled = fopen(filename_buf, "rb");
		free(filename_buf);
	}

	if (compiled == NULL) {
		return GLOW_LOAD_ERR_NOT_FOUND;
	}

	fseek(compiled, 0L, SEEK_END);
	const size_t code_size = ftell(compiled) - glow_magic_size;
	fseek(compiled, 0L, SEEK_SET);

	/* verify file signature */
	for (size_t i = 0; i < glow_magic_size; i++) {
		const byte c = fgetc(compiled);
		if (c != glow_magic[i]) {
			return GLOW_LOAD_ERR_INVALID_SIGNATURE;
		}
	}

	glow_code_init(dest, code_size);
	fread(dest->bc, 1, code_size, compiled);
	dest->size = code_size;
	fclose(compiled);
	return GLOW_LOAD_ERR_NONE;
}
