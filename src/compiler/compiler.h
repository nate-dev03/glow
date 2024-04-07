#ifndef GLOW_COMPILER_H
#define GLOW_COMPILER_H

#include <stdio.h>
#include "code.h"
#include "symtab.h"
#include "consttab.h"
#include "opcodes.h"

extern const byte glow_magic[];
extern const size_t glow_magic_size;

/*
 * The following structure is used for
 * continue/break bookkeeping.
 */
struct glow_loop_block_info {
	size_t start_index;     // start of loop body

	size_t *break_indices;  // indexes of break statements
	size_t break_indices_size;
	size_t break_indices_capacity;

	struct glow_loop_block_info *prev;
};

typedef struct {
	const char *filename;
	GlowCode code;
	struct glow_loop_block_info *lbi;
	GlowSymTable *st;
	GlowConstTable *ct;

	unsigned int try_catch_depth;
	unsigned int try_catch_depth_max;

	GlowCode lno_table;
	unsigned int first_lineno;
	unsigned int first_ins_on_line_idx;
	unsigned int last_ins_idx;
	unsigned int last_lineno;

	unsigned in_generator : 1;
} GlowCompiler;

void glow_compile(const char *name, GlowProgram *prog, FILE *out);

int glow_opcode_arg_size(GlowOpcode opcode);

#endif /* GLOW_COMPILER_H */
