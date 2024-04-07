#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "err.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "symtab.h"
#include "opcodes.h"
#include "code.h"
#include "util.h"
#include "compiler.h"

struct metadata {
	size_t bc_size;
	size_t lno_table_size;
	unsigned int max_vstack_depth;
	unsigned int max_try_catch_depth;
};

#define LBI_INIT_CAPACITY 5

static void write_byte(GlowCompiler *compiler, const byte p)
{
	glow_code_write_byte(&compiler->code, p);
}

static void write_ins(GlowCompiler *compiler, const GlowOpcode p, unsigned int lineno)
{
#define WB(p) glow_code_write_byte(&compiler->lno_table, p)

	const unsigned int curr_lineno = compiler->last_lineno;

	if (lineno > curr_lineno) {
		unsigned int ins_delta = compiler->last_ins_idx - compiler->first_ins_on_line_idx;
		unsigned int lineno_delta = lineno - curr_lineno;
		compiler->first_ins_on_line_idx = compiler->last_ins_idx;

		while (lineno_delta || ins_delta) {
			byte x = ins_delta < 0xff ? ins_delta : 0xff;
			byte y = lineno_delta < 0xff ? lineno_delta : 0xff;
			WB(x);
			WB(y);
			ins_delta -= x;
			lineno_delta -= y;
		}

		compiler->last_lineno = lineno;
	}

	++compiler->last_ins_idx;
	write_byte(compiler, p);

#undef WB
}

static void write_int(GlowCompiler *compiler, const int n)
{
	glow_code_write_int(&compiler->code, n);
}

static void write_uint16(GlowCompiler *compiler, const size_t n)
{
	glow_code_write_uint16(&compiler->code, n);
}

static void write_uint16_at(GlowCompiler *compiler, const size_t n, const size_t pos)
{
	glow_code_write_uint16_at(&compiler->code, n, pos);
}

static void write_double(GlowCompiler *compiler, const double d)
{
	glow_code_write_double(&compiler->code, d);
}

static void write_str(GlowCompiler *compiler, const GlowStr *str)
{
	glow_code_write_str(&compiler->code, str);
}

static void append(GlowCompiler *compiler, const GlowCode *code)
{
	glow_code_append(&compiler->code, code);
}

const byte glow_magic[] = {0xFE, 0xED, 0xF0, 0x0D};
const size_t glow_magic_size = sizeof(glow_magic);

/*
 * Compilation
 */
static void fill_ct(GlowCompiler *compiler, GlowProgram *program);
static void write_sym_table(GlowCompiler *compiler);
static void write_const_table(GlowCompiler *compiler);

static GlowOpcode to_opcode(GlowNodeType type);

#define DEFAULT_BC_CAPACITY        100
#define DEFAULT_LNO_TABLE_CAPACITY 30

static GlowCompiler *compiler_new(const char *filename, unsigned int first_lineno, GlowSymTable *st)
{
	GlowCompiler *compiler = glow_malloc(sizeof(GlowCompiler));
	compiler->filename = filename;
	glow_code_init(&compiler->code, DEFAULT_BC_CAPACITY);
	compiler->lbi = NULL;
	compiler->st = st;
	compiler->ct = glow_ct_new();
	compiler->try_catch_depth = 0;
	compiler->try_catch_depth_max = 0;
	glow_code_init(&compiler->lno_table, DEFAULT_LNO_TABLE_CAPACITY);
	compiler->first_lineno = first_lineno;
	compiler->first_ins_on_line_idx = 0;
	compiler->last_ins_idx = 0;
	compiler->last_lineno = first_lineno;
	compiler->in_generator = 0;

	return compiler;
}

/*
 * Note: this does not deallocate the compiler's code field.
 */
static void compiler_free(GlowCompiler *compiler, bool free_st)
{
	if (free_st) {
		glow_st_free(compiler->st);
	}
	glow_ct_free(compiler->ct);
	glow_code_dealloc(&compiler->code);
	glow_code_dealloc(&compiler->lno_table);
	free(compiler);
}

static struct glow_loop_block_info *lbi_new(size_t start_index, struct glow_loop_block_info *prev)
{
	struct glow_loop_block_info *lbi = glow_malloc(sizeof(*lbi));
	lbi->start_index = start_index;
	lbi->break_indices = glow_malloc(LBI_INIT_CAPACITY * sizeof(size_t));
	lbi->break_indices_size = 0;
	lbi->break_indices_capacity = LBI_INIT_CAPACITY;
	lbi->prev = prev;
	return lbi;
}

static void lbi_add_break_index(struct glow_loop_block_info *lbi, size_t break_index)
{
	size_t bi_size = lbi->break_indices_size;
	size_t bi_capacity = lbi->break_indices_capacity;

	if (bi_size == bi_capacity) {
		bi_capacity = (bi_capacity * 3)/2 + 1;
		lbi->break_indices = glow_realloc(lbi->break_indices, bi_capacity * sizeof(size_t));
		lbi->break_indices_capacity = bi_capacity;
	}

	lbi->break_indices[lbi->break_indices_size++] = break_index;
}

void lbi_free(struct glow_loop_block_info *lbi)
{
	free(lbi->break_indices);
	free(lbi);
}

static void compiler_push_loop(GlowCompiler *compiler, size_t start_index)
{
	compiler->lbi = lbi_new(start_index, compiler->lbi);
}

static void compiler_pop_loop(GlowCompiler *compiler)
{
	struct glow_loop_block_info *lbi = compiler->lbi;
	const size_t *break_indices = lbi->break_indices;
	const size_t break_indices_size = lbi->break_indices_size;
	const size_t end_index = compiler->code.size;

	for (size_t i = 0; i < break_indices_size; i++) {
		const size_t break_index = break_indices[i];
		write_uint16_at(compiler, end_index - break_index - 2, break_index);
	}

	compiler->lbi = lbi->prev;
	lbi_free(lbi);
}

static void compile_node(GlowCompiler *compiler, GlowAST *ast, bool toplevel);
static struct metadata compile_program(GlowCompiler *compiler, GlowProgram *program);

static void compile_load(GlowCompiler *compiler, GlowAST *ast);
static void compile_assignment(GlowCompiler *compiler, GlowAST *ast);

static void compile_and(GlowCompiler *compiler, GlowAST *ast);
static void compile_or(GlowCompiler *compiler, GlowAST *ast);

static void compile_call(GlowCompiler *compiler, GlowAST *ast);

static void compile_cond_expr(GlowCompiler *compiler, GlowAST *ast);

static void compile_block(GlowCompiler *compiler, GlowAST *ast);
static void compile_list(GlowCompiler *compiler, GlowAST *ast);
static void compile_tuple(GlowCompiler *compiler, GlowAST *ast);
static void compile_set(GlowCompiler *compiler, GlowAST *ast);
static void compile_dict(GlowCompiler *compiler, GlowAST *ast);
static void compile_dict_elem(GlowCompiler *compiler, GlowAST *ast);
static void compile_index(GlowCompiler *compiler, GlowAST *ast);

static void compile_if(GlowCompiler *compiler, GlowAST *ast);
static void compile_while(GlowCompiler *compiler, GlowAST *ast);
static void compile_for(GlowCompiler *compiler, GlowAST *ast);
static void compile_def(GlowCompiler *compiler, GlowAST *ast);
static void compile_gen(GlowCompiler *compiler, GlowAST *ast);
static void compile_act(GlowCompiler *compiler, GlowAST *ast);
static void compile_lambda(GlowCompiler *compiler, GlowAST *ast);
static void compile_break(GlowCompiler *compiler, GlowAST *ast);
static void compile_continue(GlowCompiler *compiler, GlowAST *ast);
static void compile_return(GlowCompiler *compiler, GlowAST *ast);
static void compile_throw(GlowCompiler *compiler, GlowAST *ast);
static void compile_produce(GlowCompiler *compiler, GlowAST *ast);
static void compile_receive(GlowCompiler *compiler, GlowAST *ast);
static void compile_try_catch(GlowCompiler *compiler, GlowAST *ast);
static void compile_import(GlowCompiler *compiler, GlowAST *ast);
static void compile_export(GlowCompiler *compiler, GlowAST *ast);

static void compile_get_attr(GlowCompiler *compiler, GlowAST *ast);

static int max_stack_depth(byte *bc, size_t len);

static struct metadata compile_raw(GlowCompiler *compiler, GlowProgram *program, bool is_single_expr)
{
	if (is_single_expr) {
		assert(program->next == NULL);
	}

	fill_ct(compiler, program);
	write_sym_table(compiler);
	write_const_table(compiler);

	const size_t start_size = compiler->code.size;

	for (struct glow_ast_list *node = program; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, !is_single_expr);
	}

	if (is_single_expr) {
		write_ins(compiler, GLOW_INS_RETURN, 0);
	} else {
		write_ins(compiler,
		          compiler->in_generator ? GLOW_INS_LOAD_ITER_STOP : GLOW_INS_LOAD_NULL,
		          0);
		write_ins(compiler, GLOW_INS_RETURN, 0);
	}

	GlowCode *code = &compiler->code;
	GlowCode *lno_table = &compiler->lno_table;

	/* two zeros mark the end of the line number table */
	glow_code_write_byte(lno_table, 0);
	glow_code_write_byte(lno_table, 0);

	const size_t final_size = code->size;
	const size_t bc_size = final_size - start_size;
	unsigned int max_vstack_depth = max_stack_depth(code->bc, code->size);
	unsigned int max_try_catch_depth = compiler->try_catch_depth_max;

	/*
	 * What follows is somewhat delicate: we want the line number
	 * table to come before the symbol/constant tables in the compiled
	 * code, but we do not have a completed line number table until
	 * compilation is complete, so we copy everything into a new Code
	 * instance and use that as our finished product.
	 */
	const size_t lno_table_size = lno_table->size;
	GlowCode complete;
	glow_code_init(&complete, 2 + 2 + lno_table_size + final_size);
	glow_code_write_uint16(&complete, compiler->first_lineno);
	glow_code_write_uint16(&complete, lno_table_size);
	glow_code_append(&complete, lno_table);
	glow_code_append(&complete, code);
	glow_code_dealloc(code);
	compiler->code = complete;

	/* return some data describing what we compiled */
	struct metadata metadata;
	metadata.bc_size = bc_size;
	metadata.lno_table_size = lno_table_size;
	metadata.max_vstack_depth = max_vstack_depth;
	metadata.max_try_catch_depth = max_try_catch_depth;
	return metadata;
}

static struct metadata compile_program(GlowCompiler *compiler, GlowProgram *program)
{
	glow_st_populate(compiler->st, program);
	return compile_raw(compiler, program, false);
}

static void compile_const(GlowCompiler *compiler, GlowAST *ast)
{
	const unsigned int lineno = ast->lineno;
	GlowCTConst value;

	switch (ast->type) {
	case GLOW_NODE_INT:
		value.type = GLOW_CT_INT;
		value.value.i = ast->v.int_val;
		break;
	case GLOW_NODE_FLOAT:
		value.type = GLOW_CT_DOUBLE;
		value.value.d = ast->v.float_val;
		break;
	case GLOW_NODE_STRING:
		value.type = GLOW_CT_STRING;
		value.value.s = ast->v.str_val;
		break;
	case GLOW_NODE_DEF:
	case GLOW_NODE_GEN:
	case GLOW_NODE_ACT:
	case GLOW_NODE_LAMBDA: {
		const unsigned int const_id = glow_ct_poll_codeobj(compiler->ct);
		write_ins(compiler, GLOW_INS_LOAD_CONST, lineno);
		write_uint16(compiler, const_id);
		return;
	}
	default:
		GLOW_INTERNAL_ERROR();
	}

	const unsigned int const_id = glow_ct_id_for_const(compiler->ct, value);
	write_ins(compiler, GLOW_INS_LOAD_CONST, lineno);
	write_uint16(compiler, const_id);
}

static void compile_load(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_IDENT);

	const unsigned int lineno = ast->lineno;
	const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		GLOW_INTERNAL_ERROR();
	}

	if (sym->bound_here) {
		write_ins(compiler, GLOW_INS_LOAD, lineno);
	} else if (sym->global_var) {
		write_ins(compiler, GLOW_INS_LOAD_GLOBAL, lineno);
	} else {
		assert(sym->free_var);
		write_ins(compiler, GLOW_INS_LOAD_NAME, lineno);
	}

	write_uint16(compiler, sym->id);
}

static void compile_assignment(GlowCompiler *compiler, GlowAST *ast)
{
	const GlowNodeType type = ast->type;
	if (!GLOW_NODE_TYPE_IS_ASSIGNMENT(type)) {
		GLOW_INTERNAL_ERROR();
	}

	const unsigned int lineno = ast->lineno;

	GlowAST *lhs = ast->left;
	GlowAST *rhs = ast->right;

	/*
	 *         (assign)
	 *        /       \
	 *       .        rhs
	 *      / \
	 *    id  attr
	 */
	if (lhs->type == GLOW_NODE_DOT) {
		const GlowSTSymbol *sym = glow_ste_get_attr_symbol(compiler->st->ste_current, lhs->right->v.ident);
		const unsigned int sym_id = sym->id;

		if (sym == NULL) {
			GLOW_INTERNAL_ERROR();
		}

		if (type == GLOW_NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
			compile_node(compiler, lhs->left, false);
			write_ins(compiler, GLOW_INS_SET_ATTR, lineno);
			write_uint16(compiler, sym_id);
		} else {
			/* compound assignment */
			compile_node(compiler, lhs->left, false);
			write_ins(compiler, GLOW_INS_DUP, lineno);
			write_ins(compiler, GLOW_INS_LOAD_ATTR, lineno);
			write_uint16(compiler, sym_id);
			compile_node(compiler, rhs, false);
			write_ins(compiler, to_opcode(type), lineno);
			write_ins(compiler, GLOW_INS_ROT, lineno);
			write_ins(compiler, GLOW_INS_SET_ATTR, lineno);
			write_uint16(compiler, sym_id);
		}
	} else if (lhs->type == GLOW_NODE_INDEX) {
		if (type == GLOW_NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
			compile_node(compiler, lhs->left, false);
			compile_node(compiler, lhs->right, false);
			write_ins(compiler, GLOW_INS_SET_INDEX, lineno);
		} else {
			/* compound assignment */
			compile_node(compiler, lhs->left, false);
			compile_node(compiler, lhs->right, false);
			write_ins(compiler, GLOW_INS_DUP_TWO, lineno);
			write_ins(compiler, GLOW_INS_LOAD_INDEX, lineno);
			compile_node(compiler, rhs, false);
			write_ins(compiler, to_opcode(type), lineno);
			write_ins(compiler, GLOW_INS_ROT_THREE, lineno);
			write_ins(compiler, GLOW_INS_SET_INDEX, lineno);
		}
	} else {
		const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, lhs->v.ident);

		if (sym == NULL) {
			GLOW_INTERNAL_ERROR();
		}

		const unsigned int sym_id = sym->id;
		assert(sym->bound_here || sym->global_var);

		if (type == GLOW_NODE_ASSIGN) {
			compile_node(compiler, rhs, false);
		} else {
			/* compound assignment */
			compile_load(compiler, lhs);
			compile_node(compiler, rhs, false);
			write_ins(compiler, to_opcode(type), lineno);
		}

		byte store_ins;
		if (sym->bound_here) {
			store_ins = GLOW_INS_STORE;
		} else if (sym->global_var) {
			store_ins = GLOW_INS_STORE_GLOBAL;
		} else {
			GLOW_INTERNAL_ERROR();
		}

		write_ins(compiler, store_ins, lineno);
		write_uint16(compiler, sym_id);
	}
}

static void compile_call(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_CALL);

	const unsigned int lineno = ast->lineno;

	unsigned int unnamed_args = 0;
	unsigned int named_args = 0;

	bool flip = false;  // sanity check: no unnamed args after named ones
	for (struct glow_ast_list *node = ast->v.params; node != NULL; node = node->next) {
		if (GLOW_NODE_TYPE_IS_ASSIGNMENT(node->ast->type)) {
			GLOW_AST_TYPE_ASSERT(node->ast, GLOW_NODE_ASSIGN);
			GLOW_AST_TYPE_ASSERT(node->ast->left, GLOW_NODE_IDENT);
			flip = true;

			GlowCTConst name;
			name.type = GLOW_CT_STRING;
			name.value.s = node->ast->left->v.ident;
			const unsigned int id = glow_ct_id_for_const(compiler->ct, name);

			write_ins(compiler, GLOW_INS_LOAD_CONST, lineno);
			write_uint16(compiler, id);
			compile_node(compiler, node->ast->right, false);

			++named_args;
		} else {
			assert(!flip);
			compile_node(compiler, node->ast, false);

			++unnamed_args;
		}
	}

	assert(unnamed_args <= 0xff && named_args <= 0xff);

	compile_node(compiler, ast->left, false);  // callable
	write_ins(compiler, GLOW_INS_CALL, lineno);
	write_uint16(compiler, (named_args << 8) | unnamed_args);
}

static void compile_cond_expr(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_COND_EXPR);

	const unsigned int lineno = ast->lineno;

	compile_node(compiler, ast->v.middle, false);  // condition
	write_ins(compiler, GLOW_INS_JMP_IF_FALSE, lineno);
	const size_t jmp_to_false_index = compiler->code.size;
	write_uint16(compiler, 0);

	compile_node(compiler, ast->left, false);  // true branch
	write_ins(compiler, GLOW_INS_JMP, lineno);
	const size_t jmp_out_index = compiler->code.size;
	write_uint16(compiler, 0);

	write_uint16_at(compiler, compiler->code.size - jmp_to_false_index - 2, jmp_to_false_index);

	compile_node(compiler, ast->right, false);  // false branch

	write_uint16_at(compiler, compiler->code.size - jmp_out_index - 2, jmp_out_index);
}

static void compile_and(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_AND);
	compile_node(compiler, ast->left, false);
	write_ins(compiler, GLOW_INS_JMP_IF_FALSE_ELSE_POP, ast->left->lineno);
	size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);  // placeholder for jump offset
	compile_node(compiler, ast->right, false);
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);
}

static void compile_or(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_OR);
	compile_node(compiler, ast->left, false);
	write_ins(compiler, GLOW_INS_JMP_IF_TRUE_ELSE_POP, ast->left->lineno);
	size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);  // placeholder for jump offset
	compile_node(compiler, ast->right, false);
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);
}


static void compile_block(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_BLOCK);

	for (struct glow_ast_list *node = ast->v.block; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, true);
	}
}

static void compile_list(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_LIST);

	size_t len = 0;
	for (struct glow_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++len;
	}

	write_ins(compiler, GLOW_INS_MAKE_LIST, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_tuple(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_TUPLE);

	size_t len = 0;
	for (struct glow_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++len;
	}

	write_ins(compiler, GLOW_INS_MAKE_TUPLE, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_set(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_SET);

	size_t len = 0;
	for (struct glow_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		compile_node(compiler, node->ast, false);
		++len;
	}

	write_ins(compiler, GLOW_INS_MAKE_SET, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_dict(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_DICT);

	size_t len = 0;
	for (struct glow_ast_list *node = ast->v.list; node != NULL; node = node->next) {
		GLOW_AST_TYPE_ASSERT(node->ast, GLOW_NODE_DICT_ELEM);
		compile_node(compiler, node->ast, false);
		len += 2;
	}

	write_ins(compiler, GLOW_INS_MAKE_DICT, ast->lineno);
	write_uint16(compiler, len);
}

static void compile_dict_elem(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_DICT_ELEM);
	compile_node(compiler, ast->left, false);
	compile_node(compiler, ast->right, false);
}

static void compile_index(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_INDEX);
	compile_node(compiler, ast->left, false);
	compile_node(compiler, ast->right, false);
	write_ins(compiler, GLOW_INS_LOAD_INDEX, ast->lineno);
}

static void compile_if(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_IF);

	GlowAST *else_chain_base = ast->v.middle;
	unsigned int n_elifs = 0;

	for (GlowAST *node = else_chain_base; node != NULL; node = node->v.middle) {
		if (node->type == GLOW_NODE_ELSE) {
			assert(node->v.middle == NULL);   // this'd better be the last node
		} else {
			assert(node->type == GLOW_NODE_ELIF);  // only ELSE and ELIF nodes allowed here
			++n_elifs;
		}
	}

	/*
	 * Placeholder indices for jump offsets following the ELSE/ELIF bodies.
	 */
	size_t *jmp_placeholder_indices = glow_malloc((1 + n_elifs) * sizeof(size_t));
	size_t node_index = 0;

	for (GlowAST *node = ast; node != NULL; node = node->v.middle) {
		GlowNodeType type = node->type;
		const unsigned int lineno = node->lineno;

		switch (type) {
		case GLOW_NODE_IF:
		case GLOW_NODE_ELIF: {
			compile_node(compiler, node->left, false);  // condition
			write_ins(compiler, GLOW_INS_JMP_IF_FALSE, lineno);
			const size_t jmp_to_next_index = compiler->code.size;
			write_uint16(compiler, 0);

			compile_node(compiler, node->right, true);  // body
			write_ins(compiler, GLOW_INS_JMP, lineno);
			const size_t jmp_out_index = compiler->code.size;
			write_uint16(compiler, 0);

			jmp_placeholder_indices[node_index++] = jmp_out_index;
			write_uint16_at(compiler, compiler->code.size - jmp_to_next_index - 2, jmp_to_next_index);
			break;
		}
		case GLOW_NODE_ELSE: {
			compile_node(compiler, node->left, true);  // body
			break;
		}
		default:
			GLOW_INTERNAL_ERROR();
		}
	}

	const size_t final_size = compiler->code.size;

	for (size_t i = 0; i <= n_elifs; i++) {
		const size_t jmp_placeholder_index = jmp_placeholder_indices[i];
		write_uint16_at(compiler, final_size - jmp_placeholder_index - 2, jmp_placeholder_index);
	}

	free(jmp_placeholder_indices);
}

static void compile_while(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_WHILE);

	const size_t loop_start_index = compiler->code.size;
	compile_node(compiler, ast->left, false);  // condition
	write_ins(compiler, GLOW_INS_JMP_IF_FALSE, 0);

	// jump placeholder:
	const size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);

	compiler_push_loop(compiler, loop_start_index);
	compile_node(compiler, ast->right, true);  // body

	write_ins(compiler, GLOW_INS_JMP_BACK, 0);
	write_uint16(compiler, compiler->code.size - loop_start_index + 2);

	// fill in placeholder:
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);

	compiler_pop_loop(compiler);
}

static void compile_for(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_FOR);
	const unsigned int lineno = ast->lineno;

	GlowAST *lcv = ast->left;  // loop control variable
	GlowAST *iter = ast->right;
	GlowAST *body = ast->v.middle;

	compile_node(compiler, iter, false);
	write_ins(compiler, GLOW_INS_GET_ITER, lineno);

	const size_t loop_start_index = compiler->code.size;
	compiler_push_loop(compiler, loop_start_index);
	write_ins(compiler, GLOW_INS_LOOP_ITER, iter->lineno);

	// jump placeholder:
	const size_t jump_index = compiler->code.size;
	write_uint16(compiler, 0);

	if (lcv->type == GLOW_NODE_IDENT) {
		const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, lcv->v.ident);

		if (sym == NULL) {
			GLOW_INTERNAL_ERROR();
		}

		write_ins(compiler, GLOW_INS_STORE, lineno);
		write_uint16(compiler, sym->id);
	} else {
		GLOW_AST_TYPE_ASSERT(lcv, GLOW_NODE_TUPLE);

		write_ins(compiler, GLOW_INS_SEQ_EXPAND, lcv->lineno);

		unsigned int count = 0;
		for (struct glow_ast_list *node = lcv->v.list; node != NULL; node = node->next) {
			++count;
		}

		write_uint16(compiler, count);

		/* sequence is expanded left-to-right, so we have to store in reverse */
		for (int i = count-1; i >= 0; i--) {
			struct glow_ast_list *node = lcv->v.list;
			for (int j = 0; j < i; j++) {
				node = node->next;
			}

			GLOW_AST_TYPE_ASSERT(node->ast, GLOW_NODE_IDENT);
			const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, node->ast->v.ident);

			if (sym == NULL) {
				GLOW_INTERNAL_ERROR();
			}

			write_ins(compiler, GLOW_INS_STORE, lineno);
			write_uint16(compiler, sym->id);
		}
	}

	compile_node(compiler, body, true);

	write_ins(compiler, GLOW_INS_JMP_BACK, 0);
	write_uint16(compiler, compiler->code.size - loop_start_index + 2);

	// fill in placeholder:
	write_uint16_at(compiler, compiler->code.size - jump_index - 2, jump_index);

	compiler_pop_loop(compiler);

	write_ins(compiler, GLOW_INS_POP, 0);  // pop the iterator left behind by GET_ITER
}

#define COMPILE_DEF 0
#define COMPILE_GEN 1
#define COMPILE_ACT 2

static void compile_def_or_gen_or_act(GlowCompiler *compiler, GlowAST *ast, const int select)
{
	switch (select) {
	case COMPILE_DEF:
		GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_DEF);
		break;
	case COMPILE_GEN:
		GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_GEN);
		break;
	case COMPILE_ACT:
		GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_ACT);
		break;
	default:
		GLOW_INTERNAL_ERROR();
	}

	const unsigned int lineno = ast->lineno;
	const GlowAST *name = ast->left;

	/* A function definition is essentially the assignment of a CodeObject to a variable. */
	const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, name->v.ident);

	if (sym == NULL) {
		GLOW_INTERNAL_ERROR();
	}

	compile_const(compiler, ast);

	/* type hints */
	unsigned int num_hints = 0;
	for (struct glow_ast_list *param = ast->v.params; param != NULL; param = param->next) {
		GlowAST *v = param->ast;

		if (v->type == GLOW_NODE_ASSIGN) {
			v = v->left;
		}

		GLOW_AST_TYPE_ASSERT(v, GLOW_NODE_IDENT);

		if (v->left != NULL) {
			compile_load(compiler, v->left);
		} else {
			write_ins(compiler, GLOW_INS_LOAD_NULL, lineno);
		}

		++num_hints;
	}

	if (name->left != NULL) {
		GLOW_AST_TYPE_ASSERT(name->left, GLOW_NODE_IDENT);
		compile_load(compiler, name->left);
	} else {
		write_ins(compiler, GLOW_INS_LOAD_NULL, lineno);
	}

	++num_hints;

	/* defaults */
	bool flip = false;  // sanity check: no non-default args after default ones
	unsigned int num_defaults = 0;
	for (struct glow_ast_list *param = ast->v.params; param != NULL; param = param->next) {
		if (param->ast->type == GLOW_NODE_ASSIGN) {
			flip = true;
			GLOW_AST_TYPE_ASSERT(param->ast->left, GLOW_NODE_IDENT);
			compile_node(compiler, param->ast->right, false);
			++num_defaults;
		} else {
			assert(!flip);
		}
	}

	assert(num_defaults <= 0xff);
	assert(num_hints <= 0xff);

	switch (select) {
	case COMPILE_DEF:
		write_ins(compiler, GLOW_INS_MAKE_FUNCOBJ, lineno);
		break;
	case COMPILE_GEN:
		write_ins(compiler, GLOW_INS_MAKE_GENERATOR, lineno);
		break;
	case COMPILE_ACT:
		write_ins(compiler, GLOW_INS_MAKE_ACTOR, lineno);
		break;
	default:
		GLOW_INTERNAL_ERROR();
	}

	write_uint16(compiler, (num_hints << 8) | num_defaults);

	write_ins(compiler, GLOW_INS_STORE, lineno);
	write_uint16(compiler, sym->id);
}

static void compile_def(GlowCompiler *compiler, GlowAST *ast)
{
	compile_def_or_gen_or_act(compiler, ast, COMPILE_DEF);
}

static void compile_gen(GlowCompiler *compiler, GlowAST *ast)
{
	compile_def_or_gen_or_act(compiler, ast, COMPILE_GEN);
}

static void compile_act(GlowCompiler *compiler, GlowAST *ast)
{
	compile_def_or_gen_or_act(compiler, ast, COMPILE_ACT);
}

#undef COMPILE_DEF
#undef COMPILE_GEN
#undef COMPILE_ACT

static void compile_lambda(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_LAMBDA);
	compile_const(compiler, ast);
	write_ins(compiler, GLOW_INS_MAKE_FUNCOBJ, ast->lineno);
	write_uint16(compiler, 0);
}

static void compile_break(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_BREAK);
	const unsigned int lineno = ast->lineno;

	if (compiler->lbi == NULL) {
		GLOW_INTERNAL_ERROR();
	}

	write_ins(compiler, GLOW_INS_JMP, lineno);
	const size_t break_index = compiler->code.size;
	write_uint16(compiler, 0);

	/*
	 * We don't know where to jump to until we finish compiling
	 * the entire loop, so we keep a list of "breaks" and fill
	 * in their jumps afterwards.
	 */
	lbi_add_break_index(compiler->lbi, break_index);
}

static void compile_continue(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_CONTINUE);
	const unsigned int lineno = ast->lineno;

	if (compiler->lbi == NULL) {
		GLOW_INTERNAL_ERROR();
	}

	write_ins(compiler, GLOW_INS_JMP_BACK, lineno);
	const size_t start_index = compiler->lbi->start_index;
	write_uint16(compiler, compiler->code.size - start_index + 2);
}

static void compile_return(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_RETURN);
	const unsigned int lineno = ast->lineno;

	if (ast->left != NULL) {
		compile_node(compiler, ast->left, false);
	} else {
		write_ins(compiler,
		          compiler->in_generator ? GLOW_INS_LOAD_ITER_STOP : GLOW_INS_LOAD_NULL,
		          lineno);
	}

	write_ins(compiler, GLOW_INS_RETURN, lineno);
}

static void compile_throw(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_THROW);
	const unsigned int lineno = ast->lineno;
	compile_node(compiler, ast->left, false);
	write_ins(compiler, GLOW_INS_THROW, lineno);
}

static void compile_produce(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_PRODUCE);
	const unsigned int lineno = ast->lineno;
	compile_node(compiler, ast->left, false);
	write_ins(compiler, GLOW_INS_PRODUCE, lineno);
}

static void compile_receive(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_RECEIVE);
	const unsigned int lineno = ast->lineno;
	const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, ast->left->v.ident);

	if (sym == NULL || !sym->bound_here) {
		GLOW_INTERNAL_ERROR();
	}

	write_ins(compiler, GLOW_INS_RECEIVE, lineno);
	write_ins(compiler, GLOW_INS_STORE, lineno);
	write_uint16(compiler, sym->id);
}

static void compile_try_catch(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_TRY_CATCH);
	const unsigned int try_lineno = ast->lineno;
	const unsigned int catch_lineno = ast->right->lineno;

	unsigned int exc_count = 0;
	for (struct glow_ast_list *node = ast->v.excs; node != NULL; node = node->next) {
		++exc_count;
	}
	assert(exc_count > 0);
	assert(exc_count == 1);  // TODO: handle 2+ exceptions (this is currently valid syntactically)

	/* === Try Block === */
	write_ins(compiler, GLOW_INS_TRY_BEGIN, try_lineno);
	const size_t try_block_size_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for try-length */
	const size_t handler_offset_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for handler offset */

	compiler->try_catch_depth += exc_count;

	if (compiler->try_catch_depth > compiler->try_catch_depth_max) {
		compiler->try_catch_depth_max = compiler->try_catch_depth;
	}

	compile_node(compiler, ast->left, true);  /* try block */
	compiler->try_catch_depth -= exc_count;

	write_ins(compiler, GLOW_INS_TRY_END, catch_lineno);
	write_uint16_at(compiler, compiler->code.size - try_block_size_index - 4, try_block_size_index);

	write_ins(compiler, GLOW_INS_JMP, catch_lineno);  /* jump past exception handlers if no exception was thrown */
	const size_t jmp_over_handlers_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for jump offset */

	write_uint16_at(compiler, compiler->code.size - handler_offset_index - 2, handler_offset_index);

	/* === Handler === */
	write_ins(compiler, GLOW_INS_DUP, catch_lineno);
	compile_node(compiler, ast->v.excs->ast, false);
	write_ins(compiler, GLOW_INS_JMP_IF_EXC_MISMATCH, catch_lineno);
	const size_t exc_mismatch_jmp_index = compiler->code.size;
	write_uint16(compiler, 0);  /* placeholder for jump offset */

	write_ins(compiler, GLOW_INS_POP, catch_lineno);
	compile_node(compiler, ast->right, true);  /* catch */

	/* jump over re-throw */
	write_ins(compiler, GLOW_INS_JMP, catch_lineno);
	write_uint16(compiler, 1);

	write_uint16_at(compiler, compiler->code.size - exc_mismatch_jmp_index - 2, exc_mismatch_jmp_index);

	write_ins(compiler, GLOW_INS_THROW, catch_lineno);

	write_uint16_at(compiler, compiler->code.size - jmp_over_handlers_index - 2, jmp_over_handlers_index);
}

static void compile_import(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_IMPORT);

	ast = ast->left;

	const unsigned int lineno = ast->lineno;
	const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		GLOW_INTERNAL_ERROR();
	}

	const unsigned int sym_id = sym->id;

	write_ins(compiler, GLOW_INS_IMPORT, lineno);
	write_uint16(compiler, sym_id);
	write_ins(compiler, GLOW_INS_STORE, lineno);
	write_uint16(compiler, sym_id);
}

static void compile_export(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_EXPORT);

	compile_load(compiler, ast->left);

	ast = ast->left;

	const unsigned int lineno = ast->lineno;
	const GlowSTSymbol *sym = glow_ste_get_symbol(compiler->st->ste_current, ast->v.ident);

	if (sym == NULL) {
		GLOW_INTERNAL_ERROR();
	}

	if (sym->bound_here) {
		write_ins(compiler, GLOW_INS_EXPORT, lineno);
	} else if (sym->global_var) {
		write_ins(compiler, GLOW_INS_EXPORT_GLOBAL, lineno);
	} else {
		assert(sym->free_var);
		write_ins(compiler, GLOW_INS_EXPORT_NAME, lineno);
	}

	write_uint16(compiler, sym->id);
}

static void compile_get_attr(GlowCompiler *compiler, GlowAST *ast)
{
	GLOW_AST_TYPE_ASSERT(ast, GLOW_NODE_DOT);
	const unsigned int lineno = ast->lineno;

	GlowStr *attr = ast->right->v.ident;
	GlowSTSymbol *attr_sym = glow_ste_get_attr_symbol(compiler->st->ste_current, attr);

	compile_node(compiler, ast->left, false);
	write_ins(compiler, GLOW_INS_LOAD_ATTR, lineno);
	write_uint16(compiler, attr_sym->id);
}

/*
 * Converts AST node type to corresponding (most relevant) opcode.
 * For compound-assignment types, converts to the corresponding
 * in-place binary opcode.
 */
static GlowOpcode to_opcode(GlowNodeType type)
{
	switch (type) {
	case GLOW_NODE_ADD:
		return GLOW_INS_ADD;
	case GLOW_NODE_SUB:
		return GLOW_INS_SUB;
	case GLOW_NODE_MUL:
		return GLOW_INS_MUL;
	case GLOW_NODE_DIV:
		return GLOW_INS_DIV;
	case GLOW_NODE_MOD:
		return GLOW_INS_MOD;
	case GLOW_NODE_POW:
		return GLOW_INS_POW;
	case GLOW_NODE_BITAND:
		return GLOW_INS_BITAND;
	case GLOW_NODE_BITOR:
		return GLOW_INS_BITOR;
	case GLOW_NODE_XOR:
		return GLOW_INS_XOR;
	case GLOW_NODE_BITNOT:
		return GLOW_INS_BITNOT;
	case GLOW_NODE_SHIFTL:
		return GLOW_INS_SHIFTL;
	case GLOW_NODE_SHIFTR:
		return GLOW_INS_SHIFTR;
	case GLOW_NODE_AND:
		return GLOW_INS_AND;
	case GLOW_NODE_OR:
		return GLOW_INS_OR;
	case GLOW_NODE_NOT:
		return GLOW_INS_NOT;
	case GLOW_NODE_EQUAL:
		return GLOW_INS_EQUAL;
	case GLOW_NODE_NOTEQ:
		return GLOW_INS_NOTEQ;
	case GLOW_NODE_LT:
		return GLOW_INS_LT;
	case GLOW_NODE_GT:
		return GLOW_INS_GT;
	case GLOW_NODE_LE:
		return GLOW_INS_LE;
	case GLOW_NODE_GE:
		return GLOW_INS_GE;
	case GLOW_NODE_APPLY:
		return GLOW_INS_APPLY;
	case GLOW_NODE_UPLUS:
		return GLOW_INS_NOP;
	case GLOW_NODE_UMINUS:
		return GLOW_INS_UMINUS;
	case GLOW_NODE_ASSIGN:
		return GLOW_INS_STORE;
	case GLOW_NODE_ASSIGN_ADD:
		return GLOW_INS_IADD;
	case GLOW_NODE_ASSIGN_SUB:
		return GLOW_INS_ISUB;
	case GLOW_NODE_ASSIGN_MUL:
		return GLOW_INS_IMUL;
	case GLOW_NODE_ASSIGN_DIV:
		return GLOW_INS_IDIV;
	case GLOW_NODE_ASSIGN_MOD:
		return GLOW_INS_IMOD;
	case GLOW_NODE_ASSIGN_POW:
		return GLOW_INS_IPOW;
	case GLOW_NODE_ASSIGN_BITAND:
		return GLOW_INS_IBITAND;
	case GLOW_NODE_ASSIGN_BITOR:
		return GLOW_INS_IBITOR;
	case GLOW_NODE_ASSIGN_XOR:
		return GLOW_INS_IXOR;
	case GLOW_NODE_ASSIGN_SHIFTL:
		return GLOW_INS_ISHIFTL;
	case GLOW_NODE_ASSIGN_SHIFTR:
		return GLOW_INS_ISHIFTR;
	case GLOW_NODE_ASSIGN_APPLY:
		return GLOW_INS_IAPPLY;
	case GLOW_NODE_IN:
		return GLOW_INS_IN;
	case GLOW_NODE_DOTDOT:
		return GLOW_INS_MAKE_RANGE;
	default:
		GLOW_INTERNAL_ERROR();
		return 0;
	}
}

static void compile_node(GlowCompiler *compiler, GlowAST *ast, bool toplevel)
{
	if (ast == NULL) {
		return;
	}

	const unsigned int lineno = ast->lineno;

	switch (ast->type) {
	case GLOW_NODE_NULL:
		write_ins(compiler, GLOW_INS_LOAD_NULL, lineno);
		break;
	case GLOW_NODE_INT:
	case GLOW_NODE_FLOAT:
	case GLOW_NODE_STRING:
		compile_const(compiler, ast);
		break;
	case GLOW_NODE_IDENT:
		compile_load(compiler, ast);
		break;
	case GLOW_NODE_ADD:
	case GLOW_NODE_SUB:
	case GLOW_NODE_MUL:
	case GLOW_NODE_DIV:
	case GLOW_NODE_MOD:
	case GLOW_NODE_POW:
	case GLOW_NODE_BITAND:
	case GLOW_NODE_BITOR:
	case GLOW_NODE_XOR:
	case GLOW_NODE_SHIFTL:
	case GLOW_NODE_SHIFTR:
	case GLOW_NODE_EQUAL:
	case GLOW_NODE_NOTEQ:
	case GLOW_NODE_LT:
	case GLOW_NODE_GT:
	case GLOW_NODE_LE:
	case GLOW_NODE_GE:
	case GLOW_NODE_APPLY:
	case GLOW_NODE_DOTDOT:
	case GLOW_NODE_IN:
		compile_node(compiler, ast->left, false);
		compile_node(compiler, ast->right, false);
		write_ins(compiler, to_opcode(ast->type), lineno);
		break;
	case GLOW_NODE_AND:
		compile_and(compiler, ast);
		break;
	case GLOW_NODE_OR:
		compile_or(compiler, ast);
		break;
	case GLOW_NODE_DOT:
		compile_get_attr(compiler, ast);
		break;
	case GLOW_NODE_ASSIGN:
	case GLOW_NODE_ASSIGN_ADD:
	case GLOW_NODE_ASSIGN_SUB:
	case GLOW_NODE_ASSIGN_MUL:
	case GLOW_NODE_ASSIGN_DIV:
	case GLOW_NODE_ASSIGN_MOD:
	case GLOW_NODE_ASSIGN_POW:
	case GLOW_NODE_ASSIGN_BITAND:
	case GLOW_NODE_ASSIGN_BITOR:
	case GLOW_NODE_ASSIGN_XOR:
	case GLOW_NODE_ASSIGN_SHIFTL:
	case GLOW_NODE_ASSIGN_SHIFTR:
	case GLOW_NODE_ASSIGN_APPLY:
		compile_assignment(compiler, ast);
		break;
	case GLOW_NODE_BITNOT:
	case GLOW_NODE_NOT:
	case GLOW_NODE_UPLUS:
	case GLOW_NODE_UMINUS:
		compile_node(compiler, ast->left, false);
		write_ins(compiler, to_opcode(ast->type), lineno);
		break;
	case GLOW_NODE_COND_EXPR:
		compile_cond_expr(compiler, ast);
		break;
	case GLOW_NODE_PRINT:
		compile_node(compiler, ast->left, false);
		write_ins(compiler, GLOW_INS_PRINT, lineno);
		break;
	case GLOW_NODE_IF:
		compile_if(compiler, ast);
		break;
	case GLOW_NODE_WHILE:
		compile_while(compiler, ast);
		break;
	case GLOW_NODE_FOR:
		compile_for(compiler, ast);
		break;
	case GLOW_NODE_DEF:
		compile_def(compiler, ast);
		break;
	case GLOW_NODE_GEN:
		compile_gen(compiler, ast);
		break;
	case GLOW_NODE_ACT:
		compile_act(compiler, ast);
		break;
	case GLOW_NODE_LAMBDA:
		compile_lambda(compiler, ast);
		break;
	case GLOW_NODE_BREAK:
		compile_break(compiler, ast);
		break;
	case GLOW_NODE_CONTINUE:
		compile_continue(compiler, ast);
		break;
	case GLOW_NODE_RETURN:
		compile_return(compiler, ast);
		break;
	case GLOW_NODE_THROW:
		compile_throw(compiler, ast);
		break;
	case GLOW_NODE_PRODUCE:
		compile_produce(compiler, ast);
		break;
	case GLOW_NODE_RECEIVE:
		compile_receive(compiler, ast);
		break;
	case GLOW_NODE_TRY_CATCH:
		compile_try_catch(compiler, ast);
		break;
	case GLOW_NODE_IMPORT:
		compile_import(compiler, ast);
		break;
	case GLOW_NODE_EXPORT:
		compile_export(compiler, ast);
		break;
	case GLOW_NODE_BLOCK:
		compile_block(compiler, ast);
		break;
	case GLOW_NODE_LIST:
		compile_list(compiler, ast);
		break;
	case GLOW_NODE_TUPLE:
		compile_tuple(compiler, ast);
		break;
	case GLOW_NODE_SET:
		compile_set(compiler, ast);
		break;
	case GLOW_NODE_DICT:
		compile_dict(compiler, ast);
		break;
	case GLOW_NODE_DICT_ELEM:
		compile_dict_elem(compiler, ast);
		break;
	case GLOW_NODE_CALL:
		compile_call(compiler, ast);
		if (toplevel) {
			write_ins(compiler, GLOW_INS_POP, lineno);
		}
		break;
	case GLOW_NODE_INDEX:
		compile_index(compiler, ast);
		break;
	default:
		GLOW_INTERNAL_ERROR();
	}
}

/*
 * Symbol table format:
 *
 * - ST_ENTRY_BEGIN
 * - 4-byte int: no. of locals (N)
 * - N null-terminated strings representing local variable names
 *   ...
 * - 4-byte int: no. of attributes (M)
 * - M null-terminated strings representing attribute names
 *   ...
 * - ST_ENTRY_END
 */
static void write_sym_table(GlowCompiler *compiler)
{
	const GlowSTEntry *ste = compiler->st->ste_current;
	const size_t n_locals = ste->next_local_id;
	const size_t n_attrs = ste->next_attr_id;
	const size_t n_free = ste->next_free_var_id;

	GlowStr **locals_sorted = glow_malloc(n_locals * sizeof(GlowStr *));
	GlowStr **frees_sorted = glow_malloc(n_free * sizeof(GlowStr *));

	const size_t table_capacity = ste->table_capacity;

	for (size_t i = 0; i < table_capacity; i++) {
		for (GlowSTSymbol *e = ste->table[i]; e != NULL; e = e->next) {
			if (e->bound_here) {
				locals_sorted[e->id] = e->key;
			} else if (e->free_var) {
				frees_sorted[e->id] = e->key;
			}
		}
	}

	GlowStr **attrs_sorted = glow_malloc(n_attrs * sizeof(GlowStr *));

	const size_t attr_capacity = ste->attr_capacity;

	for (size_t i = 0; i < attr_capacity; i++) {
		for (GlowSTSymbol *e = ste->attributes[i]; e != NULL; e = e->next) {
			attrs_sorted[e->id] = e->key;
		}
	}

	write_byte(compiler, GLOW_ST_ENTRY_BEGIN);

	write_uint16(compiler, n_locals);
	for (size_t i = 0; i < n_locals; i++) {
		write_str(compiler, locals_sorted[i]);
	}

	write_uint16(compiler, n_attrs);
	for (size_t i = 0; i < n_attrs; i++) {
		write_str(compiler, attrs_sorted[i]);
	}

	write_uint16(compiler, n_free);
	for (size_t i = 0; i < n_free; i++) {
		write_str(compiler, frees_sorted[i]);
	}

	write_byte(compiler, GLOW_ST_ENTRY_END);

	free(locals_sorted);
	free(frees_sorted);
	free(attrs_sorted);
}

static void write_const_table(GlowCompiler *compiler)
{
	const GlowConstTable *ct = compiler->ct;
	const size_t size = ct->table_size + ct->codeobjs_size;

	write_byte(compiler, GLOW_CT_ENTRY_BEGIN);
	write_uint16(compiler, size);

	GlowCTConst *sorted = glow_malloc(size * sizeof(GlowCTConst));

	const size_t capacity = ct->capacity;
	for (size_t i = 0; i < capacity; i++) {
		for (GlowCTEntry *e = ct->table[i]; e != NULL; e = e->next) {
			sorted[e->value] = e->key;
		}
	}

	for (GlowCTEntry *e = ct->codeobjs_head; e != NULL; e = e->next) {
		sorted[e->value] = e->key;
	}

	for (size_t i = 0; i < size; i++) {
		switch (sorted[i].type) {
		case GLOW_CT_INT:
			write_byte(compiler, GLOW_CT_ENTRY_INT);
			write_int(compiler, sorted[i].value.i);
			break;
		case GLOW_CT_DOUBLE:
			write_byte(compiler, GLOW_CT_ENTRY_FLOAT);
			write_double(compiler, sorted[i].value.d);
			break;
		case GLOW_CT_STRING:
			write_byte(compiler, GLOW_CT_ENTRY_STRING);
			write_str(compiler, sorted[i].value.s);
			break;
		case GLOW_CT_CODEOBJ:
			write_byte(compiler, GLOW_CT_ENTRY_CODEOBJ);

			GlowCode *co_code = sorted[i].value.c;

			size_t name_len = 0;
			while (co_code->bc[name_len] != '\0') {
				++name_len;
			}

			/*
			 * Write size of actual CodeObject bytecode, excluding
			 * metadata (name, argcount, stack_depth, try_catch_depth):
			 */
			write_uint16(compiler, co_code->size - (name_len + 1) - 2 - 2 - 2);

			append(compiler, co_code);
			glow_code_dealloc(co_code);
			free(co_code);
			break;
		}
	}
	free(sorted);

	write_byte(compiler, GLOW_CT_ENTRY_END);
}

static void fill_ct_from_ast(GlowCompiler *compiler, GlowAST *ast)
{
	if (ast == NULL) {
		return;
	}

	GlowCTConst value;

	switch (ast->type) {
	case GLOW_NODE_INT:
		value.type = GLOW_CT_INT;
		value.value.i = ast->v.int_val;
		break;
	case GLOW_NODE_FLOAT:
		value.type = GLOW_CT_DOUBLE;
		value.value.d = ast->v.float_val;
		break;
	case GLOW_NODE_STRING:
		value.type = GLOW_CT_STRING;
		value.value.s = ast->v.str_val;
		break;
	case GLOW_NODE_DEF:
	case GLOW_NODE_GEN:
	case GLOW_NODE_ACT:
	case GLOW_NODE_LAMBDA: {
		value.type = GLOW_CT_CODEOBJ;

		GlowSymTable *st = compiler->st;
		GlowSTEntry *parent = compiler->st->ste_current;
		GlowSTEntry *child = parent->children[parent->child_pos++];

		unsigned int nargs;

		const bool def_or_gen_or_act =
		        (ast->type == GLOW_NODE_DEF || ast->type == GLOW_NODE_GEN || ast->type == GLOW_NODE_ACT);

		if (def_or_gen_or_act) {
			nargs = 0;
			for (struct glow_ast_list *param = ast->v.params; param != NULL; param = param->next) {
				if (param->ast->type == GLOW_NODE_ASSIGN) {
					fill_ct_from_ast(compiler, param->ast->right);
				}
				++nargs;
			}
		} else {
			nargs = ast->v.max_dollar_ident;
		}

		GlowBlock *body;

		if (def_or_gen_or_act) {
			body = ast->right->v.block;
		} else {
			body = glow_ast_list_new();
			body->ast = ast->left;
		}

		unsigned int lineno;
		if (body == NULL) {
			lineno = ast->right->lineno;
		} else {
			lineno = body->ast->lineno;
		}

		st->ste_current = child;
		GlowCompiler *sub = compiler_new(compiler->filename, lineno, st);
		if (ast->type == GLOW_NODE_GEN) {
			sub->in_generator = 1;
		}

		struct metadata metadata = compile_raw(sub, body, (ast->type == GLOW_NODE_LAMBDA));
		st->ste_current = parent;

		GlowCode *subcode = &sub->code;

		const unsigned int max_vstack_depth = metadata.max_vstack_depth;
		const unsigned int max_try_catch_depth = metadata.max_try_catch_depth;

		GlowCode *fncode = glow_malloc(sizeof(GlowCode));

#define LAMBDA "<lambda>"
		GlowStr name = (def_or_gen_or_act ? *ast->left->v.ident : GLOW_STR_INIT(LAMBDA, strlen(LAMBDA), 0));
#undef LAMBDA

		glow_code_init(fncode, (name.len + 1) + 2 + 2 + 2 + subcode->size);  // total size
		glow_code_write_str(fncode, &name);                                  // name
		glow_code_write_uint16(fncode, nargs);                               // argument count
		glow_code_write_uint16(fncode, max_vstack_depth);                    // max stack depth
		glow_code_write_uint16(fncode, max_try_catch_depth);                 // max try-catch depth
		glow_code_append(fncode, subcode);

		compiler_free(sub, false);
		value.value.c = fncode;

		if (!def_or_gen_or_act) {
			free(body);
		}

		break;
	}
	case GLOW_NODE_IF:
		fill_ct_from_ast(compiler, ast->left);
		fill_ct_from_ast(compiler, ast->right);

		for (GlowAST *node = ast->v.middle; node != NULL; node = node->v.middle) {
			fill_ct_from_ast(compiler, node);
		}
		return;
	case GLOW_NODE_FOR:
		fill_ct_from_ast(compiler, ast->v.middle);
		goto end;
	case GLOW_NODE_BLOCK:
		for (struct glow_ast_list *node = ast->v.block; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case GLOW_NODE_LIST:
	case GLOW_NODE_TUPLE:
	case GLOW_NODE_SET:
	case GLOW_NODE_DICT:
		for (struct glow_ast_list *node = ast->v.list; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case GLOW_NODE_CALL:
		for (struct glow_ast_list *node = ast->v.params; node != NULL; node = node->next) {
			GlowAST *ast = node->ast;
			if (ast->type == GLOW_NODE_ASSIGN) {
				GLOW_AST_TYPE_ASSERT(ast->left, GLOW_NODE_IDENT);
				value.type = GLOW_CT_STRING;
				value.value.s = ast->left->v.ident;
				glow_ct_id_for_const(compiler->ct, value);
				fill_ct_from_ast(compiler, ast->right);
			} else {
				fill_ct_from_ast(compiler, ast);
			}
		}
		goto end;
	case GLOW_NODE_TRY_CATCH:
		for (struct glow_ast_list *node = ast->v.excs; node != NULL; node = node->next) {
			fill_ct_from_ast(compiler, node->ast);
		}
		goto end;
	case GLOW_NODE_COND_EXPR:
		fill_ct_from_ast(compiler, ast->v.middle);
		goto end;
	default:
		goto end;
	}

	glow_ct_id_for_const(compiler->ct, value);
	return;

	end:
	fill_ct_from_ast(compiler, ast->left);
	fill_ct_from_ast(compiler, ast->right);
}

static void fill_ct(GlowCompiler *compiler, GlowProgram *program)
{
	for (struct glow_ast_list *node = program; node != NULL; node = node->next) {
		fill_ct_from_ast(compiler, node->ast);
	}
}

static int stack_delta(GlowOpcode opcode, int arg);
static int read_arg(GlowOpcode opcode, byte **bc);

static int max_stack_depth(byte *bc, size_t len)
{
	const byte *end = bc + len;

	/*
	 * Skip over symbol table and
	 * constant table...
	 */

	if (*bc == GLOW_ST_ENTRY_BEGIN) {
		++bc;  // ST_ENTRY_BEGIN
		const size_t n_locals = glow_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_locals; i++) {
			while (*bc++ != '\0');
		}

		const size_t n_attrs = glow_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_attrs; i++) {
			while (*bc++ != '\0');
		}

		const size_t n_frees = glow_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < n_frees; i++) {
			while (*bc++ != '\0');
		}

		++bc;  // ST_ENTRY_END
	}

	if (*bc == GLOW_CT_ENTRY_BEGIN) {
		++bc;  // CT_ENTRY_BEGIN
		const size_t ct_size = glow_util_read_uint16_from_stream(bc);
		bc += 2;

		for (size_t i = 0; i < ct_size; i++) {
			switch (*bc++) {
			case GLOW_CT_ENTRY_INT:
				bc += GLOW_INT_SIZE;
				break;
			case GLOW_CT_ENTRY_FLOAT:
				bc += GLOW_DOUBLE_SIZE;
				break;
			case GLOW_CT_ENTRY_STRING: {
				while (*bc++ != '\0');
				break;
			}
			case GLOW_CT_ENTRY_CODEOBJ: {
				size_t colen = glow_util_read_uint16_from_stream(bc);
				bc += 2;

				while (*bc++ != '\0');  // name
				bc += 2;  // arg count
				bc += 2;  // stack depth
				bc += 2;  // try-catch depth

				for (size_t i = 0; i < colen; i++) {
					++bc;
				}
				break;
			}
			}
		}

		++bc;  // CT_ENTRY_END
	}

	/*
	 * Begin max depth computation...
	 */

	int depth = 0;
	int max_depth = 0;

	while (bc != end) {
		byte opcode = *bc++;

		int arg = read_arg(opcode, &bc);
		int delta = stack_delta(opcode, arg);

		depth += delta;
		if (depth < 0) {
			depth = 0;
		}

		if (depth > max_depth) {
			max_depth = depth;
		}
	}

	return max_depth;
}

int glow_opcode_arg_size(GlowOpcode opcode)
{
	switch (opcode) {
	case GLOW_INS_NOP:
		return 0;
	case GLOW_INS_LOAD_CONST:
		return 2;
	case GLOW_INS_LOAD_NULL:
	case GLOW_INS_LOAD_ITER_STOP:
		return 0;
	case GLOW_INS_ADD:
	case GLOW_INS_SUB:
	case GLOW_INS_MUL:
	case GLOW_INS_DIV:
	case GLOW_INS_MOD:
	case GLOW_INS_POW:
	case GLOW_INS_BITAND:
	case GLOW_INS_BITOR:
	case GLOW_INS_XOR:
	case GLOW_INS_BITNOT:
	case GLOW_INS_SHIFTL:
	case GLOW_INS_SHIFTR:
	case GLOW_INS_AND:
	case GLOW_INS_OR:
	case GLOW_INS_NOT:
	case GLOW_INS_EQUAL:
	case GLOW_INS_NOTEQ:
	case GLOW_INS_LT:
	case GLOW_INS_GT:
	case GLOW_INS_LE:
	case GLOW_INS_GE:
	case GLOW_INS_UPLUS:
	case GLOW_INS_UMINUS:
	case GLOW_INS_IADD:
	case GLOW_INS_ISUB:
	case GLOW_INS_IMUL:
	case GLOW_INS_IDIV:
	case GLOW_INS_IMOD:
	case GLOW_INS_IPOW:
	case GLOW_INS_IBITAND:
	case GLOW_INS_IBITOR:
	case GLOW_INS_IXOR:
	case GLOW_INS_ISHIFTL:
	case GLOW_INS_ISHIFTR:
	case GLOW_INS_MAKE_RANGE:
	case GLOW_INS_IN:
		return 0;
	case GLOW_INS_STORE:
	case GLOW_INS_STORE_GLOBAL:
	case GLOW_INS_LOAD:
	case GLOW_INS_LOAD_GLOBAL:
	case GLOW_INS_LOAD_ATTR:
	case GLOW_INS_SET_ATTR:
		return 2;
	case GLOW_INS_LOAD_INDEX:
	case GLOW_INS_SET_INDEX:
	case GLOW_INS_APPLY:
	case GLOW_INS_IAPPLY:
		return 0;
	case GLOW_INS_LOAD_NAME:
		return 2;
	case GLOW_INS_PRINT:
		return 0;
	case GLOW_INS_JMP:
	case GLOW_INS_JMP_BACK:
	case GLOW_INS_JMP_IF_TRUE:
	case GLOW_INS_JMP_IF_FALSE:
	case GLOW_INS_JMP_BACK_IF_TRUE:
	case GLOW_INS_JMP_BACK_IF_FALSE:
	case GLOW_INS_JMP_IF_TRUE_ELSE_POP:
	case GLOW_INS_JMP_IF_FALSE_ELSE_POP:
	case GLOW_INS_CALL:
		return 2;
	case GLOW_INS_RETURN:
	case GLOW_INS_THROW:
	case GLOW_INS_PRODUCE:
		return 0;
	case GLOW_INS_TRY_BEGIN:
		return 4;
	case GLOW_INS_TRY_END:
		return 0;
	case GLOW_INS_JMP_IF_EXC_MISMATCH:
		return 2;
	case GLOW_INS_MAKE_LIST:
	case GLOW_INS_MAKE_TUPLE:
	case GLOW_INS_MAKE_SET:
	case GLOW_INS_MAKE_DICT:
		return 2;
	case GLOW_INS_IMPORT:
	case GLOW_INS_EXPORT:
	case GLOW_INS_EXPORT_GLOBAL:
	case GLOW_INS_EXPORT_NAME:
		return 2;
	case GLOW_INS_RECEIVE:
		return 0;
	case GLOW_INS_GET_ITER:
		return 0;
	case GLOW_INS_LOOP_ITER:
		return 2;
	case GLOW_INS_MAKE_FUNCOBJ:
	case GLOW_INS_MAKE_GENERATOR:
	case GLOW_INS_MAKE_ACTOR:
		return 2;
	case GLOW_INS_SEQ_EXPAND:
		return 2;
	case GLOW_INS_POP:
	case GLOW_INS_DUP:
	case GLOW_INS_DUP_TWO:
	case GLOW_INS_ROT:
	case GLOW_INS_ROT_THREE:
		return 0;
	default:
		return -1;
	}
}

static int read_arg(GlowOpcode opcode, byte **bc)
{
	int size = glow_opcode_arg_size(opcode);

	if (size < 0) {
		GLOW_INTERNAL_ERROR();
	}

	int arg;

	switch (size) {
	case 0:
		arg = 0;
		break;
	case 1:
		arg = *(*bc++);
		break;
	case 2:
	case 4:  /* only return the first 2 bytes */
		arg = glow_util_read_uint16_from_stream(*bc);
		*bc += size;
		break;
	default:
		GLOW_INTERNAL_ERROR();
		arg = 0;
		break;
	}

	return arg;
}

/*
 * Calculates the value stack depth change resulting from
 * executing the given opcode with the given argument.
 *
 * The use of this function relies on the assumption that
 * individual statements, upon completion, leave the stack
 * depth unchanged (i.e. at 0).
 */
static int stack_delta(GlowOpcode opcode, int arg)
{
	switch (opcode) {
	case GLOW_INS_NOP:
		return 0;
	case GLOW_INS_LOAD_CONST:
	case GLOW_INS_LOAD_NULL:
	case GLOW_INS_LOAD_ITER_STOP:
		return 1;
	case GLOW_INS_ADD:
	case GLOW_INS_SUB:
	case GLOW_INS_MUL:
	case GLOW_INS_DIV:
	case GLOW_INS_MOD:
	case GLOW_INS_POW:
	case GLOW_INS_BITAND:
	case GLOW_INS_BITOR:
	case GLOW_INS_XOR:
		return -1;
	case GLOW_INS_BITNOT:
		return 0;
	case GLOW_INS_SHIFTL:
	case GLOW_INS_SHIFTR:
	case GLOW_INS_AND:
	case GLOW_INS_OR:
		return -1;
	case GLOW_INS_NOT:
		return 0;
	case GLOW_INS_EQUAL:
	case GLOW_INS_NOTEQ:
	case GLOW_INS_LT:
	case GLOW_INS_GT:
	case GLOW_INS_LE:
	case GLOW_INS_GE:
		return -1;
	case GLOW_INS_UPLUS:
	case GLOW_INS_UMINUS:
		return 0;
	case GLOW_INS_IADD:
	case GLOW_INS_ISUB:
	case GLOW_INS_IMUL:
	case GLOW_INS_IDIV:
	case GLOW_INS_IMOD:
	case GLOW_INS_IPOW:
	case GLOW_INS_IBITAND:
	case GLOW_INS_IBITOR:
	case GLOW_INS_IXOR:
	case GLOW_INS_ISHIFTL:
	case GLOW_INS_ISHIFTR:
	case GLOW_INS_MAKE_RANGE:
	case GLOW_INS_IN:
		return -1;
	case GLOW_INS_STORE:
	case GLOW_INS_STORE_GLOBAL:
		return -1;
	case GLOW_INS_LOAD:
	case GLOW_INS_LOAD_GLOBAL:
		return 1;
	case GLOW_INS_LOAD_ATTR:
		return 0;
	case GLOW_INS_SET_ATTR:
		return -2;
	case GLOW_INS_LOAD_INDEX:
		return -1;
	case GLOW_INS_SET_INDEX:
		return -3;
	case GLOW_INS_APPLY:
	case GLOW_INS_IAPPLY:
		return -1;
	case GLOW_INS_LOAD_NAME:
		return 1;
	case GLOW_INS_PRINT:
		return -1;
	case GLOW_INS_JMP:
	case GLOW_INS_JMP_BACK:
		return 0;
	case GLOW_INS_JMP_IF_TRUE:
	case GLOW_INS_JMP_IF_FALSE:
	case GLOW_INS_JMP_BACK_IF_TRUE:
	case GLOW_INS_JMP_BACK_IF_FALSE:
		return -1;
	case GLOW_INS_JMP_IF_TRUE_ELSE_POP:
	case GLOW_INS_JMP_IF_FALSE_ELSE_POP:
		return 0;  // -1 if jump not taken
	case GLOW_INS_CALL:
		return -((arg & 0xff) + 2*(arg >> 8));
	case GLOW_INS_RETURN:
	case GLOW_INS_THROW:
	case GLOW_INS_PRODUCE:
		return -1;
	case GLOW_INS_TRY_BEGIN:
		return 0;
	case GLOW_INS_TRY_END:
		return 1;  // technically 0 but exception should be on stack before reaching handlers
	case GLOW_INS_JMP_IF_EXC_MISMATCH:
		return -2;
	case GLOW_INS_MAKE_LIST:
	case GLOW_INS_MAKE_TUPLE:
	case GLOW_INS_MAKE_SET:
	case GLOW_INS_MAKE_DICT:
		return -arg + 1;
	case GLOW_INS_IMPORT:
		return 1;
	case GLOW_INS_EXPORT:
	case GLOW_INS_EXPORT_GLOBAL:
	case GLOW_INS_EXPORT_NAME:
		return -1;
	case GLOW_INS_RECEIVE:
		return 1;
	case GLOW_INS_GET_ITER:
		return 0;
	case GLOW_INS_LOOP_ITER:
		return 1;
	case GLOW_INS_MAKE_FUNCOBJ:
	case GLOW_INS_MAKE_GENERATOR:
	case GLOW_INS_MAKE_ACTOR:
		return -((arg & 0xff) + (arg >> 8));
	case GLOW_INS_SEQ_EXPAND:
		return -1 + arg;
	case GLOW_INS_POP:
		return -1;
	case GLOW_INS_DUP:
		return 1;
	case GLOW_INS_DUP_TWO:
		return 2;
	case GLOW_INS_ROT:
	case GLOW_INS_ROT_THREE:
		return 0;
	}

	GLOW_INTERNAL_ERROR();
	return 0;
}

void glow_compile(const char *name, GlowProgram *prog, FILE *out)
{
	GlowCompiler *compiler = compiler_new(name, 1, glow_st_new(name));

	struct metadata metadata = compile_program(compiler, prog);

	/*
	 * Every glowc file should start with the
	 * "magic" bytes:
	 */
	for (size_t i = 0; i < glow_magic_size; i++) {
		fputc(glow_magic[i], out);
	}

	/*
	 * Directly after the magic bytes, we write
	 * the maximum value stack depth at module
	 * level, followed by the maximum try-catch
	 * depth:
	 */
	byte buf[2];

	glow_util_write_uint16_to_stream(buf, metadata.max_vstack_depth);
	for (size_t i = 0; i < sizeof(buf); i++) {
		fputc(buf[i], out);
	}

	glow_util_write_uint16_to_stream(buf, metadata.max_try_catch_depth);
	for (size_t i = 0; i < sizeof(buf); i++) {
		fputc(buf[i], out);
	}

	/*
	 * And now we write the actual bytecode:
	 */
	fwrite(compiler->code.bc, 1, compiler->code.size, out);

	compiler_free(compiler, true);
}
