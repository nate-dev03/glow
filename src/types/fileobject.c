#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "object.h"
#include "strobject.h"
#include "iter.h"
#include "exc.h"
#include "str.h"
#include "strbuf.h"
#include "util.h"
#include "fileobject.h"

#define isopen(file)   ((file->flags) & GLOW_FILE_FLAG_OPEN)
#define close(file)    ((file->flags) &= ~GLOW_FILE_FLAG_OPEN)
#define canread(file)  ((file->flags) & (GLOW_FILE_FLAG_READ | GLOW_FILE_FLAG_UPDATE))
#define canwrite(file) ((file->flags) & (GLOW_FILE_FLAG_WRITE | GLOW_FILE_FLAG_UPDATE))

static int parse_mode(const char *mode)
{
	int flags = 0;

	if (mode[0] == '\0') {
		return -1;
	}

	switch (mode[0]) {
	case 'r': flags |= GLOW_FILE_FLAG_READ; break;
	case 'w': flags |= GLOW_FILE_FLAG_WRITE; break;
	case 'a': flags |= GLOW_FILE_FLAG_APPEND; break;
	default: return -1;
	}

	if (mode[1] == '\0') {
		return flags;
	} else if (mode[2] != '\0' || mode[1] != '+') {
		return -1;
	}

	flags |= GLOW_FILE_FLAG_UPDATE;
	return flags;
}

GlowValue glow_file_make(const char *filename, const char *mode)
{
	const int flags = parse_mode(mode);

	if (flags == -1) {
		return GLOW_TYPE_EXC("invalid file mode '%s'", mode);
	}

	FILE *file = fopen(filename, mode);

	if (file == NULL) {
		return glow_io_exc_cannot_open_file(filename, mode);
	}

	GlowFileObject *fileobj = glow_obj_alloc(&glow_file_class);
	fileobj->file = file;
	fileobj->name = glow_util_str_dup(filename);
	fileobj->flags = flags | GLOW_FILE_FLAG_OPEN;
	return glow_makeobj(fileobj);
}

static void glow_file_free(GlowValue *this)
{
	GlowFileObject *file = glow_objvalue(this);

	if (isopen(file)) {
		fclose(file->file);
	}

	GLOW_FREE(file->name);
	glow_obj_class.del(this);
}

static bool is_newline_or_eof(const int c)
{
	return c == '\n' || c == EOF;
}

GlowValue glow_file_readline(GlowFileObject *fileobj)
{
	if (!isopen(fileobj)) {
		return glow_io_exc_file_closed(fileobj->name);
	}

	if (!canread(fileobj)) {
		return glow_io_exc_cannot_read_file(fileobj->name);
	}

	FILE *file = fileobj->file;

	if (feof(file)) {
		return glow_makenull();
	}

	char buf[255];
	char *next = &buf[0];
	int c;
	GlowStrBuf aux;
	aux.buf = NULL;
	buf[sizeof(buf)-1] = '\0';

	while (!is_newline_or_eof((c = fgetc(file)))) {
		if (next == &buf[sizeof(buf) - 1]) {
			if (aux.buf == NULL) {
				glow_strbuf_init(&aux, 2*sizeof(buf));
				glow_strbuf_append(&aux, buf, sizeof(buf) - 1);
			}

			const char str = c;
			glow_strbuf_append(&aux, &str, 1);
		} else {
			*next++ = c;
		}
	}

	if (ferror(file)) {
		fclose(file);
		close(fileobj);
		glow_strbuf_dealloc(&aux);
		return glow_io_exc_cannot_read_file(fileobj->name);
	}

	if (aux.buf == NULL) {
		*next = '\0';
		return glow_strobj_make_direct(buf, next - buf);
	} else {
		GlowStr str;
		glow_strbuf_trim(&aux);
		glow_strbuf_to_str(&aux, &str);
		str.freeable = 1;
		return glow_strobj_make(str);
	}
}

GlowValue glow_file_write(GlowFileObject *fileobj, const char *str, const size_t len)
{
	if (!isopen(fileobj)) {
		return glow_io_exc_file_closed(fileobj->name);
	}

	if (!canwrite(fileobj)) {
		return glow_io_exc_cannot_write_file(fileobj->name);
	}

	FILE *file = fileobj->file;
	fwrite(str, 1, len, file);

	if (ferror(file)) {
		fclose(file);
		close(fileobj);
		return glow_io_exc_cannot_write_file(fileobj->name);
	}

	return glow_makenull();
}

void glow_file_rewind(GlowFileObject *fileobj)
{
	rewind(fileobj->file);
}

bool glow_file_close(GlowFileObject *fileobj)
{
	if (isopen(fileobj)) {
		fclose(fileobj->file);
		close(fileobj);
		return true;
	} else {
		return false;
	}
}

static GlowValue file_readline(GlowValue *this,
                              GlowValue *args,
                              GlowValue *args_named,
                              size_t nargs,
                              size_t nargs_named)
{
#define NAME "readline"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowFileObject *fileobj = glow_objvalue(this);
	return glow_file_readline(fileobj);

#undef NAME
}

static GlowValue file_write(GlowValue *this,
                           GlowValue *args,
                           GlowValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "write"

	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 1);

	if (!glow_is_a(&args[0], &glow_str_class)) {
		GlowClass *class = glow_getclass(&args[0]);
		return GLOW_TYPE_EXC("can only write strings to a file, not %s instances", class->name);
	}

	GlowFileObject *fileobj = glow_objvalue(this);
	GlowStrObject *str = glow_objvalue(&args[0]);

	return glow_file_write(fileobj, str->str.value, str->str.len);

#undef NAME
}

static GlowValue file_rewind(GlowValue *this,
                            GlowValue *args,
                            GlowValue *args_named,
                            size_t nargs,
                            size_t nargs_named)
{
#define NAME "rewind"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowFileObject *fileobj = glow_objvalue(this);
	glow_file_rewind(fileobj);
	return glow_makenull();

#undef NAME
}

static GlowValue file_close(GlowValue *this,
                           GlowValue *args,
                           GlowValue *args_named,
                           size_t nargs,
                           size_t nargs_named)
{
#define NAME "close"

	GLOW_UNUSED(args);
	GLOW_UNUSED(args_named);
	GLOW_NO_NAMED_ARGS_CHECK(NAME, nargs_named);
	GLOW_ARG_COUNT_CHECK(NAME, nargs, 0);

	GlowFileObject *fileobj = glow_objvalue(this);
	return glow_makebool(glow_file_close(fileobj));

#undef NAME
}

static GlowValue file_iter(GlowValue *this)
{
	GlowFileObject *fileobj = glow_objvalue(this);
	glow_retaino(fileobj);
	return *this;
}

static GlowValue file_iternext(GlowValue *this)
{
	GlowFileObject *fileobj = glow_objvalue(this);
	GlowValue next = glow_file_readline(fileobj);
	return glow_isnull(&next) ? glow_get_iter_stop() : next;
}

struct glow_num_methods glow_file_num_methods = {
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

struct glow_seq_methods glow_file_seq_methods = {
	NULL,    /* len */
	NULL,    /* get */
	NULL,    /* set */
	NULL,    /* contains */
	NULL,    /* apply */
	NULL,    /* iapply */
};

struct glow_attr_method file_methods[] = {
	{"readline", file_readline},
	{"write", file_write},
	{"rewind", file_rewind},
	{"close", file_close},
	{NULL, NULL}
};

GlowClass glow_file_class = {
	.base = GLOW_CLASS_BASE_INIT(),
	.name = "File",
	.super = &glow_obj_class,

	.instance_size = sizeof(GlowFileObject),

	.init = NULL,
	.del = glow_file_free,

	.eq = NULL,
	.hash = NULL,
	.cmp = NULL,
	.str = NULL,
	.call = NULL,

	.print = NULL,

	.iter = file_iter,
	.iternext = file_iternext,

	.num_methods = &glow_file_num_methods,
	.seq_methods = &glow_file_seq_methods,

	.members = NULL,
	.methods = file_methods,

	.attr_get = NULL,
	.attr_set = NULL
};
