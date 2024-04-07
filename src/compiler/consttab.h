#ifndef GLOW_CONSTTAB_H
#define GLOW_CONSTTAB_H

#include <stdlib.h>
#include <stdbool.h>
#include "str.h"
#include "code.h"

#define GLOW_CT_CAPACITY 16
#define GLOW_CT_LOADFACTOR 0.75f

typedef enum {
	GLOW_CT_INT,
	GLOW_CT_DOUBLE,
	GLOW_CT_STRING,
	GLOW_CT_CODEOBJ
} GlowConstType;

typedef struct {
	GlowConstType type;

	union {
		int i;
		double d;
		GlowStr *s;
		GlowCode *c;
	} value;
} GlowCTConst;

typedef struct glow_ct_entry {
	GlowCTConst key;

	unsigned int value;  // index of the constant
	int hash;
	struct glow_ct_entry *next;
} GlowCTEntry;

/*
 * Simple constant table
 */
typedef struct {
	GlowCTEntry **table;
	size_t table_size;
	size_t capacity;

	float load_factor;
	size_t threshold;

	unsigned int next_id;

	/*
	 * Code objects work somewhat differently in the constant
	 * indexing mechanism, so they are dealt with separately.
	 */
	GlowCTEntry *codeobjs_head;
	GlowCTEntry *codeobjs_tail;
	size_t codeobjs_size;
} GlowConstTable;

GlowConstTable *glow_ct_new(void);
unsigned int glow_ct_id_for_const(GlowConstTable *ct, GlowCTConst key);
unsigned int glow_ct_poll_codeobj(GlowConstTable *ct);
void glow_ct_free(GlowConstTable *ct);

#endif /* GLOW_CONSTTAB_H */
