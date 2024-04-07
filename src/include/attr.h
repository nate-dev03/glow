#ifndef GLOW_ATTR_H
#define GLOW_ATTR_H

enum glow_attr_type {
	GLOW_ATTR_T_CHAR,
	GLOW_ATTR_T_BYTE,
	GLOW_ATTR_T_SHORT,
	GLOW_ATTR_T_INT,
	GLOW_ATTR_T_LONG,
	GLOW_ATTR_T_UBYTE,
	GLOW_ATTR_T_USHORT,
	GLOW_ATTR_T_UINT,
	GLOW_ATTR_T_ULONG,
	GLOW_ATTR_T_SIZE_T,
	GLOW_ATTR_T_BOOL,
	GLOW_ATTR_T_FLOAT,
	GLOW_ATTR_T_DOUBLE,
	GLOW_ATTR_T_STRING,
	GLOW_ATTR_T_OBJECT
};

/*
 * A class's member attributes are delineated via
 * an array of this structure:
 */
struct glow_attr_member {
	const char *name;
	const enum glow_attr_type type;
	const size_t offset;
	const int flags;
};

#define GLOW_ATTR_FLAG_READONLY    (1 << 2)
#define GLOW_ATTR_FLAG_TYPE_STRICT (1 << 3)

struct glow_value;
typedef struct glow_value (*MethodFunc)(struct glow_value *this,
                                       struct glow_value *args,
                                       struct glow_value *args_named,
                                       size_t nargs,
                                       size_t nargs_named);

/*
 * A class's method attributes are delineated via
 * an array of this structure:
 */
struct glow_attr_method {
	const char *name;
	const MethodFunc meth;
};

typedef struct glow_attr_dict_entry {
	const char *key;
	unsigned int value;
	int hash;
	struct glow_attr_dict_entry *next;
} GlowAttrDictEntry;

/*
 * Mapping of attribute names to attribute info.
 *
 * Maps key (char *) to value (unsigned int) where the first
 * bit of the value is an error bit indicating if a given key
 * was found in the dict (1) or not (0) and the second bit
 * is a flag indicating whether the attribute is a member (0)
 * or a method (1) and the remaining bits contain the index of
 * the attribute in the corresponding array.
 *
 * Attribute dictionaries should never be modified after they
 * are initialized. Also, the same key should never be added
 * twice (even if the value is the same both times).
 */
typedef struct glow_attr_dict {
	GlowAttrDictEntry **table;
	size_t table_size;
	size_t table_capacity;
} GlowAttrDict;

#define GLOW_ATTR_DICT_FLAG_FOUND  (1 << 0)
#define GLOW_ATTR_DICT_FLAG_METHOD (1 << 1)

void glow_attr_dict_init(GlowAttrDict *d, const size_t max_size);
unsigned int glow_attr_dict_get(GlowAttrDict *d, const char *key);
void glow_attr_dict_register_members(GlowAttrDict *d, struct glow_attr_member *members);
void glow_attr_dict_register_methods(GlowAttrDict *d, struct glow_attr_method *methods);

#endif /* GLOW_ATTR_H */
