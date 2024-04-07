#ifndef GLOW_VM_H
#define GLOW_VM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include "object.h"
#include "code.h"
#include "codeobject.h"
#include "str.h"
#include "strdict.h"
#include "module.h"
#include "main.h"

struct glow_exc_stack_element {
	size_t start;  /* start position of try-block */
	size_t end;    /* end position of try-block */
	size_t handler_pos;  /* where to jump in case of exception */

	/*
	 * Some opcodes could be using space on the stack
	 * long-term, so if we catch an exception, we don't
	 * want to clear the whole stack. This pointer
	 * defines where we should stop clearing.
	 */
	GlowValue *purge_wall;
};

struct glow_mailbox;

typedef struct glow_frame {
	GlowCodeObject *co;

	GlowValue *locals;
	size_t n_locals;

	GlowStr *frees;  /* free variables */
	GlowValue *val_stack;
	GlowValue *val_stack_base;
	GlowValue return_value;

	struct glow_exc_stack_element *exc_stack_base;
	struct glow_exc_stack_element *exc_stack;

	size_t pos;  /* position in bytecode */
	struct glow_frame *prev;

	struct glow_mailbox *mailbox;  /* support for actors */
	atomic_flag owned;

	unsigned active            : 1;
	unsigned persistent        : 1;
	unsigned top_level         : 1;
	unsigned force_free_locals : 1;
} GlowFrame;

typedef struct glow_vm {
	byte *head;
	GlowFrame *module;
	GlowFrame *callstack;
	struct glow_value_array globals;
	struct glow_str_array global_names;
	GlowStrDict exports;

	/*
	 * VM instances form a tree whose structure is
	 * determined by imports. This is a convenient way
	 * to retain the global variables of CodeObjects
	 * that are imported.
	 */
	struct glow_vm *children;
	struct glow_vm *sibling;
} GlowVM;

GlowVM *glow_vm_new(void);
int glow_vm_exec_code(GlowVM *vm, GlowCode *code);
void glow_vm_push_frame(GlowVM *vm, GlowCodeObject *co);
void glow_vm_push_frame_direct(GlowVM *vm, GlowFrame *frame);
void glow_vm_eval_frame(GlowVM *vm);
void glow_vm_pop_frame(GlowVM *vm);
void glow_vm_free(GlowVM *vm);

GlowFrame *glow_frame_make(GlowCodeObject *co);
void glow_frame_save_state(GlowFrame *frame,
                          const size_t pos,
                          GlowValue ret_val,
                          GlowValue *val_stack,
                          struct glow_exc_stack_element *exc_stack);
void glow_frame_reset(GlowFrame *frame);
void glow_frame_free(GlowFrame *frame);

GlowVM *glow_current_vm_get(void);
void glow_current_vm_set(GlowVM *vm);

void glow_vm_register_module(const GlowModule *module);

#endif /* GLOW_VM_H */
