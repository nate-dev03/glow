#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "loader.h"
#include "err.h"
#include "util.h"
#include "main.h"

enum cmd_flags {
	FLAG_UNKNOWN     = 1 << 0,
	FLAG_NOT_OPT     = 1 << 1,
	FLAG_HELP        = 1 << 2,
	FLAG_VERSION     = 1 << 3,
	FLAG_COMPILE     = 1 << 4,
	FLAG_DISASSEMBLE = 1 << 5
};

static const struct {
	const char short_opt;
	const char *long_opt;
	enum cmd_flags mask;
	const char *description;
} options[] = {
	{'h', "help",        FLAG_HELP,        "print this message and exit"},
	{'V', "version",     FLAG_VERSION,     "print version number and exit"},
	{'c', "compile",     FLAG_COMPILE,     "compile (glow ==> glowc)"},
	{'d', "disassemble", FLAG_DISASSEMBLE, "dump disassembled bytecode"},
	{'\0', NULL, 0, NULL}
};

static void print_usage_and_exit(const char *argv0, const int status)
{
	fprintf(stderr, "usage: %s [options] <file>\n", argv0);
	for (int i = 0; options[i].mask != 0; i++) {
		fprintf(stderr, "-%c : %s\n", options[i].short_opt, options[i].description);
	}
	exit(status);
}

static void print_version_and_exit(void)
{
	fprintf(stderr, "Glow " GLOW_VERSION "\n");
	exit(EXIT_SUCCESS);
}

static void print_not_implemented_and_exit(const enum cmd_flags mask)
{
	for (int i = 0; options[i].mask != 0; i++) {
		if (options[i].mask == mask) {
			fprintf(stderr,
			        GLOW_ERROR_HEADER "-%c/--%s not yet implemented\n",
			        options[i].short_opt,
			        options[i].long_opt);
			exit(EXIT_FAILURE);
		}
	}
	GLOW_INTERNAL_ERROR();
}

static int process_arg(const char *arg)
{
#define UNKNOWN_OPT_FMT_SHORT "Unknown option: -%c\n"
#define UNKNOWN_OPT_FMT_LONG  "Unknown option: --%s\n"

	if (arg[0] == '-') {
		if (arg[1] == '-') {
			for (int i = 0; options[i].mask != 0; i++) {
				if (strcmp(&arg[2], options[i].long_opt) == 0) {
					return options[i].mask;
				}
			}

			fprintf(stderr, UNKNOWN_OPT_FMT_LONG, &arg[2]);
			return FLAG_UNKNOWN;
		} else {
			enum cmd_flags opts = 0;

			for (int i = 1; arg[i] != '\0'; i++) {
				bool found = false;
				for (int j = 0; options[j].mask != 0; j++) {
					if (options[j].short_opt == arg[i]) {
						opts |= options[j].mask;
						found = true;
					}
				}

				if (!found) {
					fprintf(stderr, UNKNOWN_OPT_FMT_SHORT, arg[i]);
					return FLAG_UNKNOWN;
				}
			}

			return opts;
		}
	} else {
		return FLAG_NOT_OPT;
	}

#undef UNKNOWN_OPT_FMT_SHORT
#undef UNKNOWN_OPT_FMT_LONG
}

int main(int argc, char *argv[])
{
	enum cmd_flags opts = 0;
	char *filename = NULL;
	for (int i = 1; i < argc; i++) {
		const int flags = process_arg(argv[i]);
		if (flags == FLAG_NOT_OPT) {
			filename = argv[i];
			break;
		} else if (flags == FLAG_UNKNOWN) {
			print_usage_and_exit(argv[0], EXIT_FAILURE);
		} else {
			opts |= flags;
		}
	}

	if (opts & FLAG_HELP) {
		print_usage_and_exit(argv[0], EXIT_SUCCESS);
	}

	if (opts & FLAG_VERSION) {
		print_version_and_exit();
	}

	if (opts & FLAG_DISASSEMBLE) {
		print_not_implemented_and_exit(FLAG_DISASSEMBLE);
	}

	if (filename == NULL) {
		fprintf(stderr, GLOW_ERROR_HEADER "no input files\n");
		exit(EXIT_FAILURE);
	}

	const char *ext = strrchr(filename, '.');

	if (ext == NULL || !(strcmp(ext, ".glow") == 0 || strcmp(ext, ".glowc") == 0)) {
		fprintf(stderr, GLOW_ERROR_HEADER "unknown file type\n");
		fprintf(stderr, GLOW_INFO_HEADER "input file should be either Glow source (.glow) or compiled bytecode (.glowc)\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(ext, ".glow") == 0) {
		char *src = glow_util_file_to_str(filename);

		if (src == NULL) {
			fprintf(stderr, GLOW_ERROR_HEADER "can't open file '%s'\n", filename);
			exit(EXIT_FAILURE);
		}

		GlowParser *p = glow_parser_new(src, filename);

		if (GLOW_PARSER_ERROR(p)) {
			fprintf(stderr, GLOW_ERROR_HEADER "%s\n", p->error_msg);
			GLOW_FREE(src);
			glow_parser_free(p);
			exit(EXIT_FAILURE);
		}

		GlowProgram *prog = glow_parse(p);

		if (GLOW_PARSER_ERROR(p)) {
			fprintf(stderr, GLOW_ERROR_HEADER "%s\n", p->error_msg);
			GLOW_FREE(src);
			glow_parser_free(p);
			exit(EXIT_FAILURE);
		}

		glow_parser_free(p);
		GLOW_FREE(src);

		char *out_filename_buf = glow_malloc(strlen(filename) + 2);
		strcpy(out_filename_buf, filename);
		strcat(out_filename_buf, "c");
		FILE *out_file = fopen(out_filename_buf, "w");

		if (out_file == NULL) {
			fprintf(stderr, GLOW_ERROR_HEADER "can't open file '%sc' for writing\n", out_filename_buf);
			free(out_filename_buf);
			exit(EXIT_FAILURE);
		}

		glow_compile(filename, prog, out_file);
		fclose(out_file);
		glow_ast_list_free(prog);

		if (!(opts & FLAG_COMPILE)) {
			GlowCode code;
			assert(glow_load_from_file(out_filename_buf, true, &code) == 0);
			GlowVM *vm = glow_vm_new();
			glow_current_vm_set(vm);
			glow_vm_exec_code(vm, &code);
			glow_vm_free(vm);
		}

		free(out_filename_buf);
		exit(EXIT_SUCCESS);
	} else if (strcmp(ext, ".glowc") == 0) {
		if (opts & FLAG_COMPILE) {
			fprintf(stderr, GLOW_INFO_HEADER "nothing to do\n");
			exit(EXIT_SUCCESS);
		}

		GlowCode code;

		switch (glow_load_from_file(filename, true, &code)) {
		case GLOW_LOAD_ERR_NONE:
			break;
		case GLOW_LOAD_ERR_NOT_FOUND:
			fprintf(stderr, GLOW_ERROR_HEADER "can't open file '%s'\n", filename);
			exit(EXIT_FAILURE);
			break;
		case GLOW_LOAD_ERR_INVALID_SIGNATURE:
			fprintf(stderr, GLOW_ERROR_HEADER "glowc file '%s' had an invalid signature\n", filename);
			exit(EXIT_FAILURE);
			break;
		default:
			GLOW_INTERNAL_ERROR();
		}

		GlowVM *vm = glow_vm_new();
		glow_current_vm_set(vm);
		glow_vm_exec_code(vm, &code);
		glow_vm_free(vm);
		exit(EXIT_SUCCESS);
	} else {
		GLOW_INTERNAL_ERROR();
	}
}
