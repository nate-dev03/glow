#ifndef GLOW_FILEOBJECT_H
#define GLOW_FILEOBJECT_H

#include <stdio.h>
#include <stdbool.h>
#include "object.h"

extern struct glow_num_methods glow_file_num_methods;
extern struct glow_seq_methods glow_file_seq_methods;
extern GlowClass glow_file_class;

#define GLOW_FILE_FLAG_OPEN   (1 << 0)
#define GLOW_FILE_FLAG_READ   (1 << 1)
#define GLOW_FILE_FLAG_WRITE  (1 << 2)
#define GLOW_FILE_FLAG_APPEND (1 << 3)
#define GLOW_FILE_FLAG_UPDATE (1 << 4)

typedef struct {
	GlowObject base;
	FILE *file;
	const char *name;
	int flags;
} GlowFileObject;

GlowValue glow_file_make(const char *filename, const char *mode);
GlowValue glow_file_write(GlowFileObject *fileobj, const char *str, const size_t len);
GlowValue glow_file_readline(GlowFileObject *fileobj);
void glow_file_rewind(GlowFileObject *fileobj);
bool glow_file_close(GlowFileObject *fileobj);

#endif /* GLOW_FILEOBJECT_H */
