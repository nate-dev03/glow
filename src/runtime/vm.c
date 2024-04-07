#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "compiler.h"
#include "opcodes.h"
#include "str.h"
#include "object.h"
#include "boolobject.h"
#include "intobject.h"
#include "floatobject.h"
#include "strobject.h"
#include "listobject.h"
#include "tupleobject.h"
#include "setobject.h"
#include "dictobject.h"
#include "fileobject.h"
#include "codeobject.h"
#include "funcobject.h"
#include "generator.h"
#include "actor.h"
#include "method.h"
#include "nativefunc.h"
#include "module.h"
#include "metaclass.h"
#include "null.h"
#include "attr.h"
#include "exc.h"
#include "err.h"
#include "code.h"
#include "compiler.h"
#include "builtins.h"
#include "loader.h"
#include "plugins.h"
#include "util.h"
#include "main.h"
#include "vmops.h"
#include "vm.h"

static pthread_key_t vm_key;

GlowVM *glow_current_vm_get(void)
{
	return pthread_getspecific(vm_key);
}

void glow_current_vm_set(GlowVM *vm)
{
	pthread_setspecific(vm_key, vm);
}

static GlowClass *classes[] = {
	&glow_obj_class,
	&glow_null_class,
	&glow_bool_class,
	&glow_int_class,
	&glow_float_class,
	&glow_str_class,
	&glow_list_class,
	&glow_tuple_class,
	&glow_set_class,
	&glow_dict_class,
	&glow_file_class,
	&glow_co_class,
	&glow_fn_class,
	&glow_actor_class,
	&glow_future_class,
	&glow_message_class,
	&glow_method_class,
	&glow_native_func_class,
	&glow_module_class,
	&glow_meta_class,

	/* exception classes */
	&glow_exception_class,
	&glow_index_exception_class,
	&glow_type_exception_class,
	&glow_io_exception_class,
	&glow_attr_exception_class,
	&glow_import_exception_class,
	&glow_isc_exception_class,
	&glow_seq_exp_exception_class,
	&glow_actor_exception_class,
	&glow_conc_access_exception_class,
	NULL
};

static GlowStrDict builtins_dict;
static GlowStrDict builtin_modules_dict;
static GlowStrDict import_cache;

static void builtins_dict_dealloc(void)
{
	glow_strdict_dealloc(&builtins_dict);
}

static void builtin_modules_dict_dealloc(void)
{
	glow_strdict_dealloc(&builtin_modules_dict);
}

static void builtin_modules_dealloc(void)
{
	for (size_t i = 0; glow_builtin_modules[i] != NULL; i++) {
		glow_strdict_dealloc((GlowStrDict *)&glow_builtin_modules[i]->contents);
	}
}

static void import_cache_dealloc(void)
{
	glow_strdict_dealloc(&import_cache);
}

static unsigned int get_lineno(GlowFrame *frame);

static void vm_push_module_frame(GlowVM *vm, GlowCode *code);
static void vm_load_builtins(void);
static void vm_load_builtin_modules(void);
static GlowValue vm_import(GlowVM *vm, const char *name);

GlowVM *glow_vm_new(void)
{
	static bool init = false;

	if (!init) {
		for (GlowClass **class = &classes[0]; *class != NULL; class++) {
			glow_class_init(*class);
		}

		glow_strdict_init(&builtins_dict);
		glow_strdict_init(&builtin_modules_dict);
		glow_strdict_init(&import_cache);

		vm_load_builtins();
		vm_load_builtin_modules();

		atexit(builtins_dict_dealloc);
		atexit(builtin_modules_dict_dealloc);
		atexit(builtin_modules_dealloc);
		atexit(import_cache_dealloc);

#if GLOW_IS_POSIX
		const char *path = getenv(GLOW_PLUGIN_PATH_ENV);
		if (path != NULL) {
			glow_set_plugin_path(path);
			if (glow_reload_plugins() != 0) {
				fprintf(stderr, GLOW_WARNING_HEADER "could not load plug-ins at %s\n", path);
			}
		}
#endif

		pthread_key_create(&vm_key, NULL);
		init = true;
	}

	GlowVM *vm = glow_malloc(sizeof(GlowVM));
	vm->head = NULL;
	vm->module = NULL;
	vm->callstack = NULL;
	vm->globals = (struct glow_value_array){.array = NULL, .length = 0};
	vm->global_names = (struct glow_str_array){.array = NULL, .length = 0};
	vm->children = NULL;
	vm->sibling = NULL;
	glow_strdict_init(&vm->exports);
	return vm;
}

static void vm_free_helper(GlowVM *vm)
{
	free(vm->head);
	const size_t n_globals = vm->globals.length;
	GlowValue *globals = vm->globals.array;

	for (size_t i = 0; i < n_globals; i++) {
		glow_release(&globals[i]);
	}

	free(globals);
	free(vm->global_names.array);

	for (GlowVM *child = vm->children; child != NULL;) {
		GlowVM *temp = child;
		child = child->sibling;
		vm_free_helper(temp);
	}

	free(vm);
}

void glow_vm_free(GlowVM *vm)
{
	if (vm == NULL) {
		return;
	}

	/*
	 * We only deallocate the export dictionary of the
	 * top-level VM, since those of child VMs (i.e. the
	 * result of imports) will be deallocated when their
	 * associated Module instances are deallocated.
	 */
	glow_strdict_dealloc(&vm->exports);
	vm_free_helper(vm);
}

static void vm_link(GlowVM *parent, GlowVM *child)
{
	child->sibling = parent->children;
	parent->children = child;
}

int glow_vm_exec_code(GlowVM *vm, GlowCode *code)
{
	vm->head = code->bc;

	vm_push_module_frame(vm, code);
	glow_vm_eval_frame(vm);
	glow_actor_join_all();

	GlowValue *ret = &vm->callstack->return_value;

	int status = 0;
	if (glow_isexc(ret)) {
		status = 1;
		GlowException *e = (GlowException *)glow_objvalue(ret);
		glow_exc_traceback_print(e, stderr);
		glow_exc_print_msg(e, stderr);
		glow_release(ret);
	} else if (glow_iserror(ret)) {
		status = 1;
		GlowError *e = glow_errvalue(ret);
		glow_err_traceback_print(e, stderr);
		glow_err_print_msg(e, stderr);
		glow_err_free(e);
	}

	glow_vm_pop_frame(vm);
	return status;
}

/*
 * Important note about the references Frames and CodeObjects have for one another:
 *
 * Frames have a `co` field which points to the CodeObject they are currently executing.
 * Similarly, CodeObjects have a `frame` field that points to the Frame that *finished*
 * executing them (this is used to avoid unnecessary creation of Frames). The point is
 * that a Frame's `co` field is only valid once that Frame is pushed, while a CodeObject's
 * `frame` field is only valid while that CodeObject is no longer being executed. In this
 * way, we resolve the issue of any circular dependencies and avoid running into problems
 * with recursive functions (CodeObjects actually retain the highest-level Frame in the
 * case of recursive calls).
 */

static GlowFrame *get_frame(GlowCodeObject *co)
{
	GlowFrame *frame = co->frame;
	co->frame = NULL;

	if (frame == NULL) {
		goto new_frame;
	}

	const bool owned = atomic_flag_test_and_set(&frame->owned);

	if (!owned) {
		frame->co = co;
		return frame;
	} else {
		goto new_frame;
	}

	new_frame:
	frame = glow_frame_make(co);
	frame->co = co;
	return frame;
}

void glow_vm_push_frame(GlowVM *vm, GlowCodeObject *co)
{
	GlowFrame *frame = get_frame(co);
	glow_vm_push_frame_direct(vm, frame);
}

void glow_vm_push_frame_direct(GlowVM *vm, GlowFrame *frame)
{
	frame->active = 1;
	frame->top_level = (vm->callstack == NULL);
	frame->prev = vm->callstack;
	vm->callstack = frame;
}

void glow_vm_pop_frame(GlowVM *vm)
{
	GlowFrame *frame = vm->callstack;
	vm->callstack = frame->prev;

	GlowCodeObject *co = frame->co;
	frame->active = 0;
	frame->co = NULL;
	atomic_flag_clear(&frame->owned);

	if (co != NULL) {
		if (!frame->persistent) {
			if (co->frame == NULL) {
				co->frame = frame;
			} else {
				glow_frame_free(frame);
			}
		}
		glow_releaseo(co);
	}
}

GlowFrame *glow_frame_make(GlowCodeObject *co)
{
	GlowFrame *frame = glow_malloc(sizeof(GlowFrame));

	const size_t n_locals = co->names.length;
	const size_t stack_depth = co->stack_depth;
	const size_t try_catch_depth = co->try_catch_depth;

	frame->co = NULL;  // `co` field only valid when frame is being executed
	frame->locals = glow_calloc(n_locals + stack_depth, sizeof(GlowValue));
	frame->n_locals = n_locals;

	frame->val_stack = frame->val_stack_base = frame->locals + n_locals;

	const size_t frees_len = co->frees.length;
	GlowStr *frees = glow_malloc(frees_len * sizeof(GlowStr));
	for (size_t i = 0; i < frees_len; i++) {
		frees[i] = GLOW_STR_INIT(co->frees.array[i].str, co->frees.array[i].length, 0);
	}
	frame->frees = frees;

	frame->exc_stack_base =
	        frame->exc_stack =
	                glow_malloc(try_catch_depth * sizeof(struct glow_exc_stack_element));

	frame->pos = 0;
	frame->return_value = glow_makeempty();
	frame->mailbox = NULL;
	static atomic_flag flag = ATOMIC_FLAG_INIT;
	frame->owned = flag;
	atomic_flag_clear(&frame->owned);

	frame->active = 0;
	frame->persistent = 0;
	frame->top_level = 0;
	frame->force_free_locals = 0;

	return frame;
}

void glow_frame_save_state(GlowFrame *frame,
                          const size_t pos,
                          GlowValue ret_val,
                          GlowValue *val_stack,
                          struct glow_exc_stack_element *exc_stack)
{
	frame->pos = pos;
	frame->val_stack = val_stack;
	frame->exc_stack = exc_stack;
	glow_release(&frame->return_value);
	frame->return_value = ret_val;
}

void glow_frame_reset(GlowFrame *frame)
{
	if (!frame->top_level || frame->force_free_locals) {
		const size_t n_locals = frame->n_locals;
		GlowValue *locals = frame->locals;

		for (size_t i = 0; i < n_locals; i++) {
			glow_release(&locals[i]);
			locals[i] = glow_makeempty();
		}
	}

	glow_release(&frame->return_value);
	frame->return_value = glow_makeempty();
	frame->val_stack = frame->val_stack_base;
	frame->exc_stack = frame->exc_stack_base;
	frame->pos = 0;
}

void glow_frame_free(GlowFrame *frame)
{
	if (frame == NULL) {
		return;
	}

	GlowValue *val_stack = frame->val_stack;
	const GlowValue *val_stack_base = frame->val_stack_base;
	while (val_stack != val_stack_base) {
		glow_release(--val_stack);
	}

	glow_frame_reset(frame);

	/*
	 * We don't free the module-level local variables,
	 * because these are actually global variables and
	 * may still be referred to via imports. These will
	 * eventually be freed once the VM instance itself
	 * is freed.
	 */
	if (!frame->top_level || frame->force_free_locals) {
		free(frame->locals);
	}

	if (frame->co != NULL) {
		glow_releaseo(frame->co);
	}

	glow_release(&frame->return_value);
	free(frame->frees);
	free(frame->exc_stack_base);
	free(frame);
}

/*
 * Assumes the symbol table and constant table have not yet been read.
 */
static void vm_push_module_frame(GlowVM *vm, GlowCode *code)
{
	assert(vm->module == NULL);
	GlowCodeObject *co = glow_codeobj_make_toplevel(code, "<module>", vm);
	glow_vm_push_frame(vm, co);
	vm->module = vm->callstack;
	vm->globals = (struct glow_value_array){.array = vm->module->locals,
	                                       .length = vm->module->n_locals};
	glow_util_str_array_dup(&co->names, &vm->global_names);
}

void glow_vm_eval_frame(GlowVM *vm)
{
#define GET_BYTE()    (bc[pos++])
#define GET_UINT16()  (pos += 2, ((bc[pos - 1] << 8) | bc[pos - 2]))

#define IN_TOP_FRAME()  (vm->callstack == vm->module)

#define STACK_POP()          (--stack)
#define STACK_POPN(n)        (stack -= (n))
#define STACK_TOP()          (&stack[-1])
#define STACK_SECOND()       (&stack[-2])
#define STACK_THIRD()        (&stack[-3])
#define STACK_PUSH(v)        (*stack++ = (v))
#define STACK_SET_TOP(v)     (stack[-1] = (v))
#define STACK_SET_SECOND(v)  (stack[-2] = (v))
#define STACK_SET_THIRD(v)   (stack[-3] = (v))
#define STACK_PURGE(wall)    do { while (stack != wall) { glow_release(STACK_POP());} } while (0)

#define EXC_STACK_PUSH(start, end, handler, purge_wall) \
	(*exc_stack++ = (struct glow_exc_stack_element){(start), (end), (handler), (purge_wall)})
#define EXC_STACK_POP()       (--exc_stack)
#define EXC_STACK_TOP()       (&exc_stack[-1])
#define EXC_STACK_EMPTY()     (exc_stack == exc_stack_base)

	GlowFrame *frame = vm->callstack;

	GlowValue *locals = frame->locals;
	GlowStr *frees = frame->frees;
	GlowCodeObject *co = frame->co;
	const GlowVM *co_vm = co->vm;
	GlowValue *globals = co_vm->globals.array;

	const struct glow_str_array symbols = co->names;
	const struct glow_str_array attrs = co->attrs;
	const struct glow_str_array global_symbols = co_vm->global_names;

	GlowValue *constants = co->consts.array;
	const byte *bc = co->bc;
	const GlowValue *stack_base = frame->val_stack_base;
	GlowValue *stack = frame->val_stack;
	GlowClass *ret_hint = GLOW_CODEOBJ_RET_HINT(co);

	const struct glow_exc_stack_element *exc_stack_base = frame->exc_stack_base;
	struct glow_exc_stack_element *exc_stack = frame->exc_stack;
	struct glow_mailbox *mb = frame->mailbox;

	/* position in the bytecode */
	size_t pos = frame->pos;

	GlowValue *v1, *v2, *v3;
	GlowValue res;

	head:
	while (true) {
		frame->pos = pos;

		while (!EXC_STACK_EMPTY() && (pos < EXC_STACK_TOP()->start || pos > EXC_STACK_TOP()->end)) {
			EXC_STACK_POP();
		}

		const byte opcode = GET_BYTE();

		switch (opcode) {
		case GLOW_INS_NOP:
			break;
		case GLOW_INS_LOAD_CONST: {
			const unsigned int id = GET_UINT16();
			v1 = &constants[id];
			glow_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case GLOW_INS_LOAD_NULL: {
			STACK_PUSH(glow_makenull());
			break;
		}
		case GLOW_INS_LOAD_ITER_STOP: {
			STACK_PUSH(glow_get_iter_stop());
			break;
		}
		/*
		 * Q: Why is the error check sandwiched between releasing v2 and
		 *    releasing v1?
		 *
		 * A: We want to use TOP/SET_TOP instead of POP/PUSH when we can,
		 *    and doing so in the case of binary operators means v1 will
		 *    still be on the stack while v2 will have been popped as we
		 *    perform the operation (e.g. op_add). Since the error section
		 *    purges the stack (which entails releasing everything on it),
		 *    releasing v1 before the error check would lead to an invalid
		 *    double-release of v1 in the case of an error/exception.
		 */
		case GLOW_INS_ADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_add(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_SUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_sub(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_MUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_mul(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_DIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_div(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_MOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_mod(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_POW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_pow(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_BITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_bitand(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_BITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_bitor(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_XOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_xor(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_BITNOT: {
			v1 = STACK_TOP();
			res = glow_op_bitnot(v1);

			if (glow_iserror(&res)) {
				goto error;
			}

			glow_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_SHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_shiftl(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_SHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_shiftr(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_AND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_and(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_OR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_or(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_NOT: {
			v1 = STACK_TOP();
			res = glow_op_not(v1);

			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_EQUAL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_eq(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_NOTEQ: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_neq(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_LT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_lt(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_GT: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_gt(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_LE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_le(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_GE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ge(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_UPLUS: {
			v1 = STACK_TOP();
			res = glow_op_plus(v1);

			if (glow_iserror(&res)) {
				goto error;
			}

			glow_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_UMINUS: {
			v1 = STACK_TOP();
			res = glow_op_minus(v1);

			if (glow_iserror(&res)) {
				goto error;
			}

			glow_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IADD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_iadd(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_ISUB: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_isub(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IMUL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_imul(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IDIV: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_idiv(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IMOD: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_imod(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IPOW: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ipow(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IBITAND: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ibitand(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IBITOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ibitor(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IXOR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ixor(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_ISHIFTL: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ishiftl(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_ISHIFTR: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_ishiftr(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_MAKE_RANGE: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_range_make(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IN: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_in(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_STORE: {
			v1 = STACK_POP();
			const unsigned int id = GET_UINT16();
			GlowValue old = locals[id];
			locals[id] = *v1;
			glow_release(&old);
			break;
		}
		case GLOW_INS_STORE_GLOBAL: {
			v1 = STACK_POP();
			const unsigned int id = GET_UINT16();
			GlowValue old = globals[id];
			globals[id] = *v1;
			glow_release(&old);
			break;
		}
		case GLOW_INS_LOAD: {
			const unsigned int id = GET_UINT16();
			v1 = &locals[id];

			if (glow_isempty(v1)) {
				res = glow_makeerr(glow_err_unbound(symbols.array[id].str));
				goto error;
			}

			glow_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case GLOW_INS_LOAD_GLOBAL: {
			const unsigned int id = GET_UINT16();
			v1 = &globals[id];

			if (glow_isempty(v1)) {
				res = glow_makeerr(glow_err_unbound(global_symbols.array[id].str));
				goto error;
			}

			glow_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case GLOW_INS_LOAD_ATTR: {
			v1 = STACK_TOP();
			const unsigned int id = GET_UINT16();
			const char *attr = attrs.array[id].str;
			res = glow_op_get_attr(v1, attr);

			if (glow_iserror(&res)) {
				goto error;
			}

			glow_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_SET_ATTR: {
			v1 = STACK_POP();
			v2 = STACK_POP();
			const unsigned int id = GET_UINT16();
			const char *attr = attrs.array[id].str;
			res = glow_op_set_attr(v1, attr, v2);

			glow_release(v1);
			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}

			break;
		}
		case GLOW_INS_LOAD_INDEX: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_get(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_SET_INDEX: {
			/* X[N] = Y */
			v3 = STACK_POP();  /* N */
			v2 = STACK_POP();  /* X */
			v1 = STACK_POP();  /* Y */

			res = glow_op_set(v2, v3, v1);

			glow_release(v1);
			glow_release(v2);
			glow_release(v3);

			if (glow_iserror(&res)) {
				goto error;
			}

			glow_release(&res);
			break;
		}
		case GLOW_INS_APPLY: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_apply(v2, v1);  // yes, the arguments are reversed

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}
			glow_release(v1);

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_IAPPLY: {
			v2 = STACK_POP();
			v1 = STACK_TOP();
			res = glow_op_iapply(v1, v2);

			glow_release(v2);
			if (glow_iserror(&res)) {
				goto error;
			}

			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_LOAD_NAME: {
			const unsigned int id = GET_UINT16();
			GlowStr *key = &frees[id];
			res = glow_strdict_get(&builtins_dict, key);

			if (!glow_isempty(&res)) {
				glow_retain(&res);
				STACK_PUSH(res);
				break;
			}

			res = glow_makeerr(glow_err_unbound(key->value));
			goto error;
			break;
		}
		case GLOW_INS_PRINT: {
			v1 = STACK_POP();

			/* res will be either an error or empty: */
			res = glow_op_print(v1, stdout);
			glow_release(v1);

			if (glow_iserror(&res)) {
				goto error;
			}

			break;
		}
		case GLOW_INS_JMP: {
			const unsigned int jmp = GET_UINT16();
			pos += jmp;
			break;
		}
		case GLOW_INS_JMP_BACK: {
			const unsigned int jmp = GET_UINT16();
			pos -= jmp;
			break;
		}
		case GLOW_INS_JMP_IF_TRUE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (glow_resolve_nonzero(glow_getclass(v1))(v1)) {
				pos += jmp;
			}
			glow_release(v1);
			break;
		}
		case GLOW_INS_JMP_IF_FALSE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (!glow_resolve_nonzero(glow_getclass(v1))(v1)) {
				pos += jmp;
			}
			glow_release(v1);
			break;
		}
		case GLOW_INS_JMP_BACK_IF_TRUE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (glow_resolve_nonzero(glow_getclass(v1))(v1)) {
				pos -= jmp;
			}
			glow_release(v1);
			break;
		}
		case GLOW_INS_JMP_BACK_IF_FALSE: {
			v1 = STACK_POP();
			const unsigned int jmp = GET_UINT16();
			if (!glow_resolve_nonzero(glow_getclass(v1))(v1)) {
				pos -= jmp;
			}
			glow_release(v1);
			break;
		}
		case GLOW_INS_JMP_IF_TRUE_ELSE_POP: {
			v1 = STACK_TOP();
			const unsigned int jmp = GET_UINT16();
			if (glow_resolve_nonzero(glow_getclass(v1))(v1)) {
				pos += jmp;
			} else {
				STACK_POP();
				glow_release(v1);
			}
			break;
		}
		case GLOW_INS_JMP_IF_FALSE_ELSE_POP: {
			v1 = STACK_TOP();
			const unsigned int jmp = GET_UINT16();
			if (!glow_resolve_nonzero(glow_getclass(v1))(v1)) {
				pos += jmp;
			} else {
				STACK_POP();
				glow_release(v1);
			}
			break;
		}
		case GLOW_INS_CALL: {
			const unsigned int x = GET_UINT16();
			const unsigned int nargs = (x & 0xff);
			const unsigned int nargs_named = (x >> 8);
			v1 = STACK_POP();
			res = glow_op_call(v1,
			                  stack - nargs_named*2 - nargs,
			                  stack - nargs_named*2,
			                  nargs,
			                  nargs_named);

			glow_release(v1);
			if (glow_iserror(&res)) {
				goto error;
			}

			for (unsigned int i = 0; i < nargs_named; i++) {
				glow_release(STACK_POP());  // value
				glow_release(STACK_POP());  // name
			}

			for (unsigned int i = 0; i < nargs; i++) {
				glow_release(STACK_POP());
			}

			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_RETURN: {
			v1 = STACK_POP();
			glow_retain(v1);
			glow_frame_reset(frame);
			frame->return_value = *v1;
			STACK_PURGE(stack_base);
			goto done;
		}
		case GLOW_INS_THROW: {
			v1 = STACK_POP();  // exception
			GlowClass *class = glow_getclass(v1);

			if (!glow_is_subclass(class, &glow_exception_class)) {
				res = glow_makeerr(glow_type_err_invalid_throw(class));
				goto error;
			}

			res = *v1;
			res.type = GLOW_VAL_TYPE_EXC;
			goto error;
		}
		case GLOW_INS_PRODUCE: {
			v1 = STACK_POP();
			glow_retain(v1);
			glow_frame_save_state(frame, pos, *v1, stack, exc_stack);
			goto done;
		}
		case GLOW_INS_TRY_BEGIN: {
			const unsigned int try_block_len = GET_UINT16();
			const unsigned int handler_offset = GET_UINT16();

			EXC_STACK_PUSH(pos, pos + try_block_len, pos + handler_offset, stack);

			break;
		}
		case GLOW_INS_TRY_END: {
			EXC_STACK_POP();
			break;
		}
		case GLOW_INS_JMP_IF_EXC_MISMATCH: {
			const unsigned int jmp = GET_UINT16();

			v1 = STACK_POP();  // exception type
			v2 = STACK_POP();  // exception

			GlowClass *class = glow_getclass(v1);

			if (class != &glow_meta_class) {
				res = glow_makeerr(glow_type_err_invalid_catch(class));
				goto error;
			}

			GlowClass *exc_type = (GlowClass *)glow_objvalue(v1);

			if (!glow_is_a(v2, exc_type)) {
				pos += jmp;
			}

			glow_release(v1);
			glow_release(v2);

			break;
		}
		case GLOW_INS_MAKE_LIST: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = glow_list_make(stack - len, len);
			} else {
				res = glow_list_make(NULL, 0);
			}

			STACK_POPN(len);
			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_MAKE_TUPLE: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = glow_tuple_make(stack - len, len);
			} else {
				res = glow_tuple_make(NULL, 0);
			}

			STACK_POPN(len);
			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_MAKE_SET: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = glow_set_make(stack - len, len);
			} else {
				res = glow_set_make(NULL, 0);
			}

			STACK_POPN(len);

			if (glow_iserror(&res)) {
				goto error;
			}

			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_MAKE_DICT: {
			const unsigned int len = GET_UINT16();

			if (len > 0) {
				res = glow_dict_make(stack - len, len);
			} else {
				res = glow_dict_make(NULL, 0);
			}

			STACK_POPN(len);

			if (glow_iserror(&res)) {
				goto error;
			}

			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_IMPORT: {
			const unsigned int id = GET_UINT16();
			res = vm_import(vm, symbols.array[id].str);

			if (glow_iserror(&res)) {
				goto error;
			}

			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_EXPORT: {
			const unsigned int id = GET_UINT16();
			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD should do all such
			 * checks
			 */
			glow_strdict_put_copy(&vm->exports,
			                     symbols.array[id].str,
			                     symbols.array[id].length,
			                     v1);
			break;
		}
		case GLOW_INS_EXPORT_GLOBAL: {
			const unsigned int id = GET_UINT16();
			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD_GLOBAL should do all
			 * such checks
			 */
			glow_strdict_put_copy(&vm->exports,
			                     global_symbols.array[id].str,
			                     global_symbols.array[id].length,
			                     v1);
			break;
		}
		case GLOW_INS_EXPORT_NAME: {
			const unsigned int id = GET_UINT16();
			v1 = STACK_POP();

			/* no need to do a bounds check on `id`, since
			 * the preceding INS_LOAD_NAME should do all
			 * such checks
			 */
			glow_strdict_put_copy(&vm->exports,
			                     frees[id].value,
			                     frees[id].len,
			                     v1);
			break;
		}
		case GLOW_INS_RECEIVE: {
			/*
			 * There's an important assumption that this opcode
			 * will only ever be executed by code running in an
			 * actor. Otherwise, some UB may result.
			 */
			res = glow_mailbox_pop(mb);

			if (glow_iserror(&res)) {
				goto error;
			}

			GlowMessage *msg = glow_objvalue(&res);

			if (glow_isempty(&msg->contents)) {
				glow_releaseo(msg);
				glow_frame_reset(frame);
				frame->return_value = glow_makenull();
				STACK_PURGE(stack_base);
				goto done;
			}

			STACK_PUSH(res);
			break;
		}
		case GLOW_INS_GET_ITER: {
			v1 = STACK_TOP();
			res = glow_op_iter(v1);

			if (glow_iserror(&res)) {
				goto error;
			}

			glow_release(v1);
			STACK_SET_TOP(res);
			break;
		}
		case GLOW_INS_LOOP_ITER: {
			v1 = STACK_TOP();
			const unsigned int jmp = GET_UINT16();

			res = glow_op_iternext(v1);

			if (glow_iserror(&res)) {
				goto error;
			}

			if (glow_is_iter_stop(&res)) {
				pos += jmp;
			} else {
				STACK_PUSH(res);
			}

			break;
		}
		case GLOW_INS_MAKE_FUNCOBJ: {
			const unsigned int arg          = GET_UINT16();
			const unsigned int num_hints    = (arg >> 8);
			const unsigned int num_defaults = (arg & 0xff);
			const unsigned int offset = num_defaults + num_hints;

			GlowCodeObject *co = glow_objvalue(stack - offset - 1);
			res = glow_codeobj_init_hints(co, stack - offset);

			if (glow_iserror(&res)) {
				goto error;
			}

			GlowValue fn = glow_funcobj_make(co);
			glow_funcobj_init_defaults(glow_objvalue(&fn), stack - num_defaults, num_defaults);

			for (unsigned i = 0; i < num_defaults; i++) {
				glow_release(STACK_POP());
			}

			for (unsigned i = 0; i < num_hints; i++) {
				glow_release(STACK_POP());
			}

			STACK_SET_TOP(fn);
			glow_releaseo(co);

			break;
		}
		case GLOW_INS_MAKE_GENERATOR: {
			const unsigned int arg          = GET_UINT16();
			const unsigned int num_hints    = (arg >> 8);
			const unsigned int num_defaults = (arg & 0xff);
			const unsigned int offset = num_defaults + num_hints;

			GlowCodeObject *co = glow_objvalue(stack - offset - 1);
			res = glow_codeobj_init_hints(co, stack - offset);

			if (glow_iserror(&res)) {
				goto error;
			}

			GlowValue gp = glow_gen_proxy_make(co);
			glow_gen_proxy_init_defaults(glow_objvalue(&gp), stack - num_defaults, num_defaults);

			for (unsigned i = 0; i < num_defaults; i++) {
				glow_release(STACK_POP());
			}

			for (unsigned i = 0; i < num_hints; i++) {
				glow_release(STACK_POP());
			}

			STACK_SET_TOP(gp);
			glow_releaseo(co);

			break;
		}
		case GLOW_INS_MAKE_ACTOR: {
			const unsigned int arg          = GET_UINT16();
			const unsigned int num_hints    = (arg >> 8);
			const unsigned int num_defaults = (arg & 0xff);
			const unsigned int offset = num_defaults + num_hints;

			GlowCodeObject *co = glow_objvalue(stack - offset - 1);
			res = glow_codeobj_init_hints(co, stack - offset);

			if (glow_iserror(&res)) {
				goto error;
			}

			GlowValue ap = glow_actor_proxy_make(co);
			glow_actor_proxy_init_defaults(glow_objvalue(&ap), stack - num_defaults, num_defaults);

			for (unsigned i = 0; i < num_defaults; i++) {
				glow_release(STACK_POP());
			}

			for (unsigned i = 0; i < num_hints; i++) {
				glow_release(STACK_POP());
			}

			STACK_SET_TOP(ap);
			glow_releaseo(co);
			break;
		}
		case GLOW_INS_SEQ_EXPAND: {
			const unsigned int n = GET_UINT16();
			v1 = STACK_POP();

			/* common case */
			if (glow_getclass(v1) == &glow_tuple_class) {
				GlowTupleObject *tup = glow_objvalue(v1);

				if (tup->count != n) {
					res = glow_seq_exp_exc_inconsistent(tup->count, n);
					glow_release(v1);
					goto error;
				}

				GlowValue *elems = tup->elements;
				for (unsigned i = 0; i < n; i++) {
					glow_retain(&elems[i]);
					STACK_PUSH(elems[i]);
				}

				glow_releaseo(tup);
			} else {
				GlowValue iter = glow_op_iter(v1);
				glow_release(v1);

				if (glow_iserror(&iter)) {
					res = iter;
					goto error;
				}

				unsigned int count = 0;
				while (true) {
					res = glow_op_iternext(&iter);

					if (glow_iserror(&res)) {
						glow_release(&iter);
						goto error;
					}

					if (glow_is_iter_stop(&res)) {
						break;
					}

					++count;

					if (count > n) {
						glow_release(&iter);
						glow_release(&res);
						res = glow_seq_exp_exc_inconsistent(count, n);
						goto error;
					}

					STACK_PUSH(res);
				}

				glow_release(&iter);

				if (count != n) {
					res = glow_seq_exp_exc_inconsistent(count, n);
					goto error;
				}
			}

			break;
		}
		case GLOW_INS_POP: {
			glow_release(STACK_POP());
			break;
		}
		case GLOW_INS_DUP: {
			v1 = STACK_TOP();
			glow_retain(v1);
			STACK_PUSH(*v1);
			break;
		}
		case GLOW_INS_DUP_TWO: {
			v1 = STACK_TOP();
			v2 = STACK_SECOND();
			glow_retain(v1);
			glow_retain(v2);
			STACK_PUSH(*v2);
			STACK_PUSH(*v1);
			break;
		}
		case GLOW_INS_ROT: {
			GlowValue v1 = *STACK_SECOND();
			STACK_SET_SECOND(*STACK_TOP());
			STACK_SET_TOP(v1);
			break;
		}
		case GLOW_INS_ROT_THREE: {
			GlowValue v1 = *STACK_TOP();
			GlowValue v2 = *STACK_SECOND();
			GlowValue v3 = *STACK_THIRD();
			STACK_SET_TOP(v2);
			STACK_SET_SECOND(v3);
			STACK_SET_THIRD(v1);
			break;
		}
		default: {
			GLOW_INTERNAL_ERROR();
			break;
		}
		}
	}

	error:
	switch (res.type) {
	case GLOW_VAL_TYPE_EXC: {
		if (EXC_STACK_EMPTY()) {
			STACK_PURGE(stack_base);
			glow_retain(&res);
			GlowException *e = glow_objvalue(&res);
			glow_exc_traceback_append(e, co->name, get_lineno(frame));
			glow_frame_reset(frame);
			frame->return_value = res;
			return;
		} else {
			const struct glow_exc_stack_element *exc = EXC_STACK_POP();
			STACK_PURGE(exc->purge_wall);
			STACK_PUSH(res);
			pos = exc->handler_pos;
			goto head;
		}
		break;
	}
	case GLOW_VAL_TYPE_ERROR: {
		GlowError *e = glow_errvalue(&res);
		glow_err_traceback_append(e, co->name, get_lineno(frame));
		glow_frame_reset(frame);
		STACK_PURGE(stack_base);
		frame->return_value = res;
		return;
	}
	default:
		GLOW_INTERNAL_ERROR();
	}

	done:
	if (ret_hint != NULL && !glow_is_a(&frame->return_value, ret_hint)) {
		res = glow_type_exc_hint_mismatch(glow_getclass(&frame->return_value), ret_hint);
		glow_release(&frame->return_value);
		frame->return_value = glow_makeempty();
		goto error;
	}

	return;

#undef STACK_POP
#undef STACK_TOP
#undef STACK_PUSH
}

void glow_vm_register_module(const GlowModule *module)
{
	GlowValue v = glow_makeobj((void *)module);
	glow_strdict_put(&builtin_modules_dict, module->name, &v, false);
}

static void vm_load_builtins(void)
{
	for (size_t i = 0; glow_builtins[i].name != NULL; i++) {
		glow_strdict_put(&builtins_dict, glow_builtins[i].name, (GlowValue *)&glow_builtins[i].value, false);
	}

	for (GlowClass **class = &classes[0]; *class != NULL; class++) {
		GlowValue v = glow_makeobj(*class);
		glow_strdict_put(&builtins_dict, (*class)->name, &v, false);
	}
}

static void vm_load_builtin_modules(void)
{
	for (size_t i = 0; glow_builtin_modules[i] != NULL; i++) {
		glow_vm_register_module(glow_builtin_modules[i]);
	}
}

static GlowValue vm_import(GlowVM *vm, const char *name)
{
	GlowValue cached = glow_strdict_get_cstr(&import_cache, name);

	if (!glow_isempty(&cached)) {
		glow_retain(&cached);
		return cached;
	}

	GlowCode code;
	int error = glow_load_from_file(name, false, &code);

	switch (error) {
	case GLOW_LOAD_ERR_NONE:
		break;
	case GLOW_LOAD_ERR_NOT_FOUND: {
		GlowValue builtin_module = glow_strdict_get_cstr(&builtin_modules_dict, name);

		if (glow_isempty(&builtin_module)) {
			return glow_import_exc_not_found(name);
		}

		return builtin_module;
	}
	case GLOW_LOAD_ERR_INVALID_SIGNATURE:
		return glow_makeerr(glow_err_invalid_file_signature_error(name));
	}

	GlowVM *vm2 = glow_vm_new();
	glow_current_vm_set(vm2);
	glow_vm_exec_code(vm2, &code);
	GlowStrDict *exports = &vm2->exports;
	GlowValue mod = glow_module_make(name, exports);
	glow_strdict_put(&import_cache, name, &mod, false);
	glow_current_vm_set(vm);
	vm_link(vm, vm2);

	glow_retain(&mod);
	return mod;
}

static unsigned int get_lineno(GlowFrame *frame)
{
	const size_t raw_pos = frame->pos;
	GlowCodeObject *co = frame->co;
	struct glow_code_cache *cache = co->cache;

	if (cache[raw_pos].lineno != 0) {
		return cache[raw_pos].lineno;
	}

	byte *bc = co->bc;
	const byte *lno_table = co->lno_table;
	const size_t first_lineno = co->first_lineno;

	byte *p = bc;
	byte *dest = &bc[raw_pos];
	size_t ins_pos = 0;

	/* translate raw position into actual instruction position */
	while (p != dest) {
		++ins_pos;
		const int size = glow_opcode_arg_size(*p);

		if (size < 0) {
			GLOW_INTERNAL_ERROR();
		}

		p += size + 1;

		if (p > dest) {
			GLOW_INTERNAL_ERROR();
		}
	}

	unsigned int lineno_offset = 0;
	size_t ins_offset = 0;
	while (true) {
		const byte ins_delta = *lno_table++;
		const byte lineno_delta = *lno_table++;

		if (ins_delta == 0 && lineno_delta == 0) {
			break;
		}

		ins_offset += ins_delta;

		if (ins_offset >= ins_pos) {
			break;
		}

		lineno_offset += lineno_delta;
	}

	unsigned int lineno = first_lineno + lineno_offset;
	cache[raw_pos].lineno = lineno;
	return lineno;
}
