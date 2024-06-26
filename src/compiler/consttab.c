#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "str.h"
#include "util.h"
#include "err.h"
#include "consttab.h"

static int const_hash(GlowCTConst *key);
static bool const_eq(GlowCTConst *key1, GlowCTConst *key2);
static GlowConstTable *ct_new_specific(const size_t capacity, const float load_factor);
static void ct_grow(GlowConstTable *ct, const size_t new_capacity);

static int const_hash(GlowCTConst *key)
{
	switch (key->type) {
	case GLOW_CT_INT:
		return glow_util_hash_int(key->value.i);
	case GLOW_CT_DOUBLE:
		return glow_util_hash_double(key->value.d);
	case GLOW_CT_STRING:
		return glow_str_hash(key->value.s);
	case GLOW_CT_CODEOBJ:
		return 0;
	}

	GLOW_INTERNAL_ERROR();
	return 0;
}

static bool const_eq(GlowCTConst *key1, GlowCTConst *key2)
{
	if (key1->type != key2->type) {
		return false;
	}

	switch (key1->type) {
	case GLOW_CT_INT:
		return key1->value.i == key2->value.i;
	case GLOW_CT_DOUBLE:
		return key1->value.d == key2->value.d;
	case GLOW_CT_STRING:
		return glow_str_eq(key1->value.s, key2->value.s);
	case GLOW_CT_CODEOBJ:
		return false;
	}

	GLOW_INTERNAL_ERROR();
	return 0;
}

GlowConstTable *glow_ct_new(void)
{
	return ct_new_specific(GLOW_CT_CAPACITY, GLOW_CT_LOADFACTOR);
}

static GlowConstTable *ct_new_specific(const size_t capacity, const float load_factor)
{
	// the capacity should be a power of 2:
	size_t capacity_real;

	if ((capacity & (capacity - 1)) == 0) {
		capacity_real = capacity;
	} else {
		capacity_real = 1;

		while (capacity_real < capacity) {
			capacity_real <<= 1;
		}
	}

	GlowConstTable *ct = glow_malloc(sizeof(GlowConstTable));
	ct->table = glow_malloc(capacity_real * sizeof(GlowCTEntry *));
	for (size_t i = 0; i < capacity_real; i++) {
		ct->table[i] = NULL;
	}
	ct->table_size = 0;
	ct->capacity = capacity_real;
	ct->load_factor = load_factor;
	ct->threshold = (size_t)(capacity * load_factor);
	ct->next_id = 0;
	ct->codeobjs_head = NULL;
	ct->codeobjs_tail = NULL;
	ct->codeobjs_size = 0;

	return ct;
}

unsigned int glow_ct_id_for_const(GlowConstTable *ct, GlowCTConst key)
{
	if (key.type == GLOW_CT_CODEOBJ) {
		GlowCTEntry *new = glow_malloc(sizeof(GlowCTEntry));
		new->key = key;
		new->value = ct->next_id++;
		new->hash = 0;
		new->next = NULL;

		if (ct->codeobjs_head == NULL) {
			assert(ct->codeobjs_tail == NULL);
			ct->codeobjs_head = new;
			ct->codeobjs_tail = new;
		} else {
			ct->codeobjs_tail->next = new;
			ct->codeobjs_tail = new;
		}

		++ct->codeobjs_size;
		return new->value;
	}

	const int hash = glow_util_hash_secondary(const_hash(&key));
	const size_t index = hash & (ct->capacity - 1);

	for (GlowCTEntry *entry = ct->table[index];
	     entry != NULL;
	     entry = entry->next) {

		if (hash == entry->hash && const_eq(&key, &entry->key)) {
			return entry->value;
		}
	}

	GlowCTEntry *new = glow_malloc(sizeof(GlowCTEntry));
	new->key = key;
	new->value = ct->next_id++;
	new->hash = hash;
	new->next = ct->table[index];
	ct->table[index] = new;

	++ct->table_size;

	if (ct->table_size > ct->threshold) {
		ct_grow(ct, 2 * ct->capacity);
	}

	return new->value;
}

unsigned int glow_ct_poll_codeobj(GlowConstTable *ct)
{
	GlowCTEntry *head = ct->codeobjs_head;
	assert(head != NULL);
	unsigned int value = head->value;
	ct->codeobjs_head = head->next;
	free(head);

	if (ct->codeobjs_head == NULL) {
		ct->codeobjs_tail = NULL;
	}

	--ct->codeobjs_size;
	return value;
}

void ct_grow(GlowConstTable *ct, const size_t new_capacity)
{
	if (new_capacity == 0) {
		return;
	}

	// the capacity should be a power of 2:
	size_t capacity_real;

	if ((new_capacity & (new_capacity - 1)) == 0) {
		capacity_real = new_capacity;
	} else {
		capacity_real = 1;

		while (capacity_real < new_capacity) {
			capacity_real <<= 1;
		}
	}

	GlowCTEntry **new_table = glow_malloc(capacity_real * sizeof(GlowCTEntry *));
	for (size_t i = 0; i < capacity_real; i++) {
		new_table[i] = NULL;
	}
	const size_t capacity = ct->capacity;

	for (size_t i = 0; i < capacity; i++) {
		GlowCTEntry *e = ct->table[i];

		while (e != NULL) {
			GlowCTEntry *next = e->next;
			const size_t index = (e->hash & (capacity_real - 1));
			e->next = new_table[index];
			new_table[index] = e;
			e = next;
		}
	}

	free(ct->table);
	ct->table = new_table;
	ct->capacity = capacity_real;
	ct->threshold = (size_t)(capacity_real * ct->load_factor);
}

static void ct_entry_free(GlowCTEntry *entry)
{
	free(entry);
}

void glow_ct_free(GlowConstTable *ct)
{
	assert(ct->codeobjs_size == 0);

	const size_t capacity = ct->capacity;

	for (size_t i = 0; i < capacity; i++) {
		GlowCTEntry *entry = ct->table[i];

		while (entry != NULL) {
			GlowCTEntry *temp = entry;
			entry = entry->next;
			ct_entry_free(temp);
		}
	}

	free(ct->table);
	free(ct);
}
