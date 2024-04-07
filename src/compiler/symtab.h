#ifndef GLOW_SYMTAB_H
#define GLOW_SYMTAB_H

#include <stdlib.h>
#include "ast.h"
#include "str.h"

typedef struct glow_st_symbol {
	GlowStr *key;
	unsigned int id;
	int hash;

	struct glow_st_symbol *next;  /* used internally to form hash table buckets */

	/* variable flags */
	unsigned bound_here : 1;
	unsigned global_var : 1;
	unsigned free_var   : 1;
	unsigned func_param : 1;
	unsigned decl_const : 1;
	unsigned attribute  : 1;
} GlowSTSymbol;

typedef enum {
	GLOW_MODULE,    /* top-level code */
	GLOW_FUNCTION,  /* function body */
	GLOW_CLASS      /* class body */
} GlowSTEContext;

struct glow_glow_sym_table;

typedef struct glow_st_entry {
	const char *name;
	GlowSTEContext context;

	/* name-to-symbol hash table */
	GlowSTSymbol **table;
	size_t table_size;
	size_t table_capacity;
	size_t table_threshold;

	unsigned int next_local_id;  /* used internally for assigning IDs to locals */
	size_t n_locals;

	/* attribute-to-symbol hash table */
	GlowSTSymbol **attributes;
	size_t attr_size;
	size_t attr_capacity;
	size_t attr_threshold;

	/* used internally for assigning IDs to attributes */
	unsigned int next_attr_id;

	/* used internally for assigning IDs to free variables */
	unsigned int next_free_var_id;

	struct glow_st_entry *parent;
	struct glow_glow_sym_table *sym_table;

	/* child vector */
	struct glow_st_entry **children;
	size_t n_children;
	size_t children_capacity;

	size_t child_pos;  /* used internally for traversing symbol tables */
} GlowSTEntry;

typedef struct glow_glow_sym_table {
	const char *filename;

	GlowSTEntry *ste_module;
	GlowSTEntry *ste_current;

	GlowSTEntry *ste_attributes;
} GlowSymTable;

GlowSymTable *glow_st_new(const char *filename);

void glow_st_populate(GlowSymTable *st, GlowProgram *program);

GlowSTSymbol *glow_ste_get_symbol(GlowSTEntry *ste, GlowStr *ident);

GlowSTSymbol *glow_ste_get_attr_symbol(GlowSTEntry *ste, GlowStr *ident);

void glow_st_free(GlowSymTable *st);

#endif /* GLOW_SYMTAB_H */
