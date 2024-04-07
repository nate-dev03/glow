#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "object.h"
#include "strobject.h"
#include "method.h"
#include "attr.h"
#include "iter.h"
#include "exc.h"
#include "err.h"
#include "util.h"
#include "vmops.h"

/*
 * General operations
 * ------------------
 */

GlowValue glow_op_hash(GlowValue *v)
{
	GlowClass *class = glow_getclass(v);
	GlowUnOp hash = glow_resolve_hash(class);

	if (!hash) {
		return glow_type_exc_unsupported_1("hash", class);
	}

	GlowValue res = hash(v);

	if (glow_iserror(&res)) {
		return res;
	}

	if (!glow_isint(&res)) {
		glow_release(&res);
		return GLOW_TYPE_EXC("hash method did not return an integer value");
	}

	return res;
}

GlowValue glow_op_str(GlowValue *v)
{
	GlowClass *class = glow_getclass(v);
	GlowUnOp str = glow_resolve_str(class);
	GlowValue res = str(v);

	if (glow_iserror(&res)) {
		return res;
	}

	if (glow_getclass(&res) != &glow_str_class) {
		glow_release(&res);
		return GLOW_TYPE_EXC("str method did not return a string object");
	}

	return res;
}

GlowValue glow_op_print(GlowValue *v, FILE *out)
{
	switch (v->type) {
	case GLOW_VAL_TYPE_NULL:
		fprintf(out, "null\n");
		break;
	case GLOW_VAL_TYPE_BOOL:
		fprintf(out, glow_boolvalue(v) ? "true\n" : "false\n");
		break;
	case GLOW_VAL_TYPE_INT:
		fprintf(out, "%ld\n", glow_intvalue(v));
		break;
	case GLOW_VAL_TYPE_FLOAT:
		fprintf(out, "%f\n", glow_floatvalue(v));
		break;
	case GLOW_VAL_TYPE_OBJECT: {
		const GlowObject *o = glow_objvalue(v);
		GlowPrintFunc print = glow_resolve_print(o->class);

		if (print) {
			print(v, out);
		} else {
			GlowValue str_v = glow_op_str(v);

			if (glow_iserror(&str_v)) {
				return str_v;
			}

			GlowStrObject *str = glow_objvalue(&str_v);
			fprintf(out, "%s\n", str->str.value);
			glow_releaseo(str);
		}
		break;
	}
	case GLOW_VAL_TYPE_EXC: {
		const GlowException *exc = glow_objvalue(v);
		fprintf(out, "%s\n", exc->msg);
		break;
	}
	case GLOW_VAL_TYPE_EMPTY:
	case GLOW_VAL_TYPE_ERROR:
	case GLOW_VAL_TYPE_UNSUPPORTED_TYPES:
	case GLOW_VAL_TYPE_DIV_BY_ZERO:
		GLOW_INTERNAL_ERROR();
		break;
	}

	return glow_makeempty();
}

#define MAKE_VM_BINOP(op, tok) \
GlowValue glow_op_##op(GlowValue *a, GlowValue *b) \
{ \
	GlowClass *class = glow_getclass(a); \
	GlowBinOp binop = glow_resolve_##op(class); \
	bool r_op = false; \
	GlowValue result; \
\
	if (!binop) { \
		GlowClass *class2 = glow_getclass(b); \
		binop = glow_resolve_r##op(class2); \
\
		if (!binop) { \
			goto type_error; \
		} \
\
		r_op = true; \
		result = binop(b, a); \
	} else { \
		result = binop(a, b); \
	} \
\
	if (glow_iserror(&result)) { \
		return result; \
	} \
\
	if (glow_isut(&result)) { \
		if (r_op) { \
			goto type_error; \
		} \
\
		GlowClass *class2 = glow_getclass(b); \
		binop = glow_resolve_r##op(class2); \
\
		if (!binop) { \
			goto type_error; \
		} \
\
		result = binop(b, a); \
\
		if (glow_iserror(&result)) { \
			return result; \
		} \
\
		if (glow_isut(&result)) { \
			goto type_error; \
		} \
	} \
\
	if (glow_isdbz(&result)) { \
		goto div_by_zero_error; \
	} \
\
	return result; \
\
	type_error: \
	return glow_type_exc_unsupported_2(#tok, class, glow_getclass(b)); \
\
	div_by_zero_error: \
	return glow_makeerr(glow_err_div_by_zero()); \
}

#define MAKE_VM_UNOP(op, tok) \
GlowValue glow_op_##op(GlowValue *a) \
{ \
	GlowClass *class = glow_getclass(a); \
	const GlowUnOp unop = glow_resolve_##op(class); \
\
	if (!unop) { \
		return glow_type_exc_unsupported_1(#tok, class); \
	} \
\
	return unop(a); \
}

MAKE_VM_BINOP(add, +)
MAKE_VM_BINOP(sub, -)
MAKE_VM_BINOP(mul, *)
MAKE_VM_BINOP(div, /)
MAKE_VM_BINOP(mod, %)
MAKE_VM_BINOP(pow, **)
MAKE_VM_BINOP(bitand, &)
MAKE_VM_BINOP(bitor, |)
MAKE_VM_BINOP(xor, ^)
MAKE_VM_UNOP(bitnot, ~)
MAKE_VM_BINOP(shiftl, <<)
MAKE_VM_BINOP(shiftr, >>)

/*
 * Logical boolean operations
 * --------------------------
 * Note that the `nonzero` method should be available
 * for all types since it is defined by `obj_class`. Hence,
 * no error checking is done here.
 */

GlowValue glow_op_and(GlowValue *a, GlowValue *b)
{
	GlowClass *class_a = glow_getclass(a);
	GlowClass *class_b = glow_getclass(b);
	const BoolUnOp bool_a = glow_resolve_nonzero(class_a);
	const BoolUnOp bool_b = glow_resolve_nonzero(class_b);

	return glow_makebool(bool_a(a) && bool_b(b));
}

GlowValue glow_op_or(GlowValue *a, GlowValue *b)
{
	GlowClass *class_a = glow_getclass(a);
	GlowClass *class_b = glow_getclass(b);
	const BoolUnOp bool_a = glow_resolve_nonzero(class_a);
	const BoolUnOp bool_b = glow_resolve_nonzero(class_b);

	return glow_makebool(bool_a(a) || bool_b(b));
}

GlowValue glow_op_not(GlowValue *a)
{
	GlowClass *class = glow_getclass(a);
	const BoolUnOp bool_a = glow_resolve_nonzero(class);
	return glow_makebool(!bool_a(a));
}

/*
 * Comparison operations
 * ---------------------
 */

#define MAKE_VM_CMPOP(op, tok) \
GlowValue glow_op_##op(GlowValue *a, GlowValue *b) \
{ \
	GlowClass *class = glow_getclass(a); \
	const GlowBinOp cmp = glow_resolve_cmp(class); \
\
	if (!cmp) { \
		goto error; \
	} \
\
	GlowValue result = cmp(a, b); \
\
	if (glow_iserror(&result)) { \
		return result; \
	} \
\
	if (glow_isut(&result)) { \
		goto error; \
	} \
\
	if (!glow_isint(&result)) { \
		return GLOW_TYPE_EXC("comparison did not return an integer value"); \
	} \
\
	return glow_makeint(glow_intvalue(&result) tok 0); \
\
	error: \
	return glow_type_exc_unsupported_2(#tok, class, glow_getclass(b)); \
}

GlowValue glow_op_eq(GlowValue *a, GlowValue *b)
{
	GlowClass *class = glow_getclass(a);
	GlowBinOp eq = glow_resolve_eq(class);

	if (!eq) {
		return glow_type_exc_unsupported_2("==", class, glow_getclass(b));
	}

	GlowValue res = eq(a, b);

	if (glow_iserror(&res)) {
		return res;
	}

	if (!glow_isbool(&res)) {
		glow_release(&res);
		return GLOW_TYPE_EXC("equals method did not return a boolean value");
	}

	return res;
}

GlowValue glow_op_neq(GlowValue *a, GlowValue *b)
{
	GlowClass *class = glow_getclass(a);
	GlowBinOp eq = glow_resolve_eq(class);

	if (!eq) {
		return glow_type_exc_unsupported_2("!=", class, glow_getclass(b));
	}

	GlowValue res = eq(a, b);

	if (glow_iserror(&res)) {
		return res;
	}

	if (!glow_isbool(&res)) {
		glow_release(&res);
		return GLOW_TYPE_EXC("equals method did not return a boolean value");
	}

	return glow_makebool(!glow_boolvalue(&res));
}

MAKE_VM_CMPOP(lt, <)
MAKE_VM_CMPOP(gt, >)
MAKE_VM_CMPOP(le, <=)
MAKE_VM_CMPOP(ge, >=)

/*
 * Other unary operations
 * ----------------------
 */

MAKE_VM_UNOP(plus, unary +)
MAKE_VM_UNOP(minus, unary -)

/*
 * In-place binary operations
 * --------------------------
 * These get called internally when a compound assignment is
 * performed.
 *
 * If the class of the LHS does not provide the corresponding
 * in-place method, the general binary method is used instead
 * (e.g. if a class does not provide `iadd`, `add` will be
 * looked up and used).
 */

#define MAKE_VM_IBINOP(op, tok) \
GlowValue glow_op_i##op(GlowValue *a, GlowValue *b) \
{ \
	GlowClass *class = glow_getclass(a); \
	GlowBinOp binop = glow_resolve_i##op(class); \
	bool r_op = false; \
	bool i_op = true; \
	GlowValue result; \
\
	if (!binop) { \
		binop = glow_resolve_##op(class); \
		if (!binop) { \
			GlowClass *class2 = glow_getclass(b); \
			binop = glow_resolve_r##op(class2); \
\
			if (!binop) { \
				goto type_error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} else { \
			result = binop(a, b); \
		} \
		i_op = false; \
	} else { \
		result = binop(a, b); \
	} \
\
	if (glow_iserror(&result)) { \
		return result; \
	} \
\
	while (glow_isut(&result)) { \
		if (i_op) { \
			binop = glow_resolve_##op(class); \
\
			if (!binop) { \
				GlowClass *class2 = glow_getclass(b); \
				binop = glow_resolve_r##op(class2); \
\
				if (!binop) { \
					goto type_error; \
				} else { \
					result = binop(b, a); \
				} \
\
				r_op = true; \
			} else { \
				result = binop(a, b); \
			} \
\
			i_op = false; \
		} else if (r_op) { \
			goto type_error; \
		} else { \
			GlowClass *class2 = glow_getclass(b); \
			binop = glow_resolve_r##op(class2); \
\
			if (!binop) { \
				goto type_error; \
			} else { \
				result = binop(b, a); \
			} \
\
			r_op = true; \
		} \
\
		if (glow_iserror(&result)) { \
			return result; \
		} \
	} \
\
	if (glow_isdbz(&result)) { \
		goto div_by_zero_error; \
	} \
\
	return result; \
\
	type_error: \
	return glow_type_exc_unsupported_2(#tok, class, glow_getclass(b)); \
\
	div_by_zero_error: \
	return glow_makeerr(glow_err_div_by_zero()); \
}

MAKE_VM_IBINOP(add, +=)
MAKE_VM_IBINOP(sub, -=)
MAKE_VM_IBINOP(mul, *=)
MAKE_VM_IBINOP(div, /=)
MAKE_VM_IBINOP(mod, %=)
MAKE_VM_IBINOP(pow, **=)
MAKE_VM_IBINOP(bitand, &=)
MAKE_VM_IBINOP(bitor, |=)
MAKE_VM_IBINOP(xor, ^=)
MAKE_VM_IBINOP(shiftl, <<=)
MAKE_VM_IBINOP(shiftr, >>=)

GlowValue glow_op_get(GlowValue *v, GlowValue *idx)
{
	GlowClass *class = glow_getclass(v);
	GlowBinOp get = glow_resolve_get(class);

	if (!get) {
		return glow_type_exc_cannot_index(class);
	}

	return get(v, idx);
}

GlowValue glow_op_set(GlowValue *v, GlowValue *idx, GlowValue *e)
{
	GlowClass *class = glow_getclass(v);
	GlowSeqSetFunc set = glow_resolve_set(class);

	if (!set) {
		return glow_type_exc_cannot_index(class);
	}

	return set(v, idx, e);
}

GlowValue glow_op_apply(GlowValue *v, GlowValue *fn)
{
	GlowClass *class = glow_getclass(v);
	GlowBinOp apply = glow_resolve_apply(class);

	if (!apply) {
		return glow_type_exc_cannot_apply(class);
	}

	GlowClass *fn_class = glow_getclass(fn);
	if (!glow_resolve_call(fn_class)) {
		return glow_type_exc_not_callable(fn_class);
	}

	return apply(v, fn);
}

GlowValue glow_op_iapply(GlowValue *v, GlowValue *fn)
{
	GlowClass *class = glow_getclass(v);
	GlowBinOp iapply = glow_resolve_iapply(class);

	GlowClass *fn_class = glow_getclass(fn);

	if (!glow_resolve_call(fn_class)) {
		return glow_type_exc_not_callable(fn_class);
	}

	if (!iapply) {
		GlowBinOp apply = glow_resolve_apply(class);

		if (!apply) {
			return glow_type_exc_cannot_apply(class);
		}

		return apply(v, fn);
	}

	return iapply(v, fn);
}

GlowValue glow_op_get_attr(GlowValue *v, const char *attr)
{
	GlowClass *class = glow_getclass(v);
	GlowAttrGetFunc attr_get = glow_resolve_attr_get(class);

	if (attr_get) {
		return attr_get(v, attr);
	} else {
		return glow_op_get_attr_default(v, attr);
	}
}

GlowValue glow_op_get_attr_default(GlowValue *v, const char *attr)
{
	GlowClass *class = glow_getclass(v);

	const unsigned int value = glow_attr_dict_get(&class->attr_dict, attr);

	if (!(value & GLOW_ATTR_DICT_FLAG_FOUND)) {
		goto get_attr_error_not_found;
	}

	const bool is_method = (value & GLOW_ATTR_DICT_FLAG_METHOD);
	const unsigned int idx = (value >> 2);

	GlowValue res;

	if (is_method) {
		const struct glow_attr_method *method = &class->methods[idx];
		res = glow_methobj_make(v, method->meth);
	} else {
		if (!glow_isobject(v)) {
			goto get_attr_error_not_found;
		}

		GlowObject *o = glow_objvalue(v);
		const struct glow_attr_member *member = &class->members[idx];
		const size_t offset = member->offset;

		switch (member->type) {
		case GLOW_ATTR_T_CHAR: {
			char *c = glow_malloc(1);
			*c = glow_getmember(o, offset, char);
			res = glow_strobj_make(GLOW_STR_INIT(c, 1, 1));
			break;
		}
		case GLOW_ATTR_T_BYTE: {
			const long n = glow_getmember(o, offset, char);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_SHORT: {
			const long n = glow_getmember(o, offset, short);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_INT: {
			const long n = glow_getmember(o, offset, int);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_LONG: {
			const long n = glow_getmember(o, offset, long);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_UBYTE: {
			const long n = glow_getmember(o, offset, unsigned char);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_USHORT: {
			const long n = glow_getmember(o, offset, unsigned short);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_UINT: {
			const long n = glow_getmember(o, offset, unsigned int);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_ULONG: {
			const long n = glow_getmember(o, offset, unsigned long);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_SIZE_T: {
			const long n = glow_getmember(o, offset, size_t);
			res = glow_makeint(n);
			break;
		}
		case GLOW_ATTR_T_BOOL: {
			const bool b = glow_getmember(o, offset, bool);
			res = glow_makebool(b);
			break;
		}
		case GLOW_ATTR_T_FLOAT: {
			const double d = glow_getmember(o, offset, float);
			res = glow_makefloat(d);
			break;
		}
		case GLOW_ATTR_T_DOUBLE: {
			const double d = glow_getmember(o, offset, double);
			res = glow_makefloat(d);
			break;
		}
		case GLOW_ATTR_T_STRING: {
			char *str = glow_getmember(o, offset, char *);
			const size_t len = strlen(str);
			char *copy = glow_malloc(len);
			memcpy(copy, str, len);
			res = glow_strobj_make(GLOW_STR_INIT(copy, len, 1));
			break;
		}
		case GLOW_ATTR_T_OBJECT: {
			GlowObject *obj = glow_getmember(o, offset, GlowObject *);
			glow_retaino(obj);
			res = glow_makeobj(obj);
			break;
		}
		}
	}

	return res;

	get_attr_error_not_found:
	return glow_attr_exc_not_found(class, attr);
}

GlowValue glow_op_set_attr(GlowValue *v, const char *attr, GlowValue *new)
{
	GlowClass *class = glow_getclass(v);
	GlowAttrSetFunc attr_set = glow_resolve_attr_set(class);

	if (attr_set) {
		return attr_set(v, attr, new);
	} else {
		return glow_op_set_attr_default(v, attr, new);
	}
}

GlowValue glow_op_set_attr_default(GlowValue *v, const char *attr, GlowValue *new)
{
	GlowClass *v_class = glow_getclass(v);
	GlowClass *new_class = glow_getclass(new);

	if (!glow_isobject(v)) {
		goto set_attr_error_not_found;
	}

	const unsigned int value = glow_attr_dict_get(&v_class->attr_dict, attr);

	if (!(value & GLOW_ATTR_DICT_FLAG_FOUND)) {
		goto set_attr_error_not_found;
	}

	const bool is_method = (value & GLOW_ATTR_DICT_FLAG_METHOD);

	if (is_method) {
		goto set_attr_error_readonly;
	}

	const unsigned int idx = (value >> 2);

	const struct glow_attr_member *member = &v_class->members[idx];

	const int member_flags = member->flags;

	if (member_flags & GLOW_ATTR_FLAG_READONLY) {
		goto set_attr_error_readonly;
	}

	const size_t offset = member->offset;

	GlowObject *o = glow_objvalue(v);
	char *o_raw = (char *)o;

	switch (member->type) {
	case GLOW_ATTR_T_CHAR: {
		if (new_class != &glow_str_class) {
			goto set_attr_error_mismatch;
		}

		GlowStrObject *str = glow_objvalue(new);
		const size_t len = str->str.len;

		if (len != 1) {
			goto set_attr_error_mismatch;
		}

		const char c = str->str.value[0];
		char *member_raw = (char *)(o_raw + offset);
		*member_raw = c;
		break;
	}
	case GLOW_ATTR_T_BYTE: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		char *member_raw = (char *)(o_raw + offset);
		*member_raw = (char)n;
		break;
	}
	case GLOW_ATTR_T_SHORT: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		short *member_raw = (short *)(o_raw + offset);
		*member_raw = (short)n;
		break;
	}
	case GLOW_ATTR_T_INT: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		int *member_raw = (int *)(o_raw + offset);
		*member_raw = (int)n;
		break;
	}
	case GLOW_ATTR_T_LONG: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		long *member_raw = (long *)(o_raw + offset);
		*member_raw = n;
		break;
	}
	case GLOW_ATTR_T_UBYTE: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		unsigned char *member_raw = (unsigned char *)(o_raw + offset);
		*member_raw = (unsigned char)n;
		break;
	}
	case GLOW_ATTR_T_USHORT: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		unsigned short *member_raw = (unsigned short *)(o_raw + offset);
		*member_raw = (unsigned short)n;
		break;
	}
	case GLOW_ATTR_T_UINT: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		unsigned int *member_raw = (unsigned int *)(o_raw + offset);
		*member_raw = (unsigned int)n;
		break;
	}
	case GLOW_ATTR_T_ULONG: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		unsigned long *member_raw = (unsigned long *)(o_raw + offset);
		*member_raw = (unsigned long)n;
		break;
	}
	case GLOW_ATTR_T_SIZE_T: {
		if (!glow_isint(new)) {
			goto set_attr_error_mismatch;
		}
		const long n = glow_intvalue(new);
		size_t *member_raw = (size_t *)(o_raw + offset);
		*member_raw = (size_t)n;
		break;
	}
	case GLOW_ATTR_T_BOOL: {
		if (!glow_isbool(new)) {
			goto set_attr_error_mismatch;
		}
		const bool b = glow_boolvalue(new);
		bool *member_raw = (bool *)(o_raw + offset);
		*member_raw = b;
		break;
	}
	case GLOW_ATTR_T_FLOAT: {
		float d;

		/* ints get promoted */
		if (glow_isint(new)) {
			d = (float)glow_intvalue(new);
		} else if (!glow_isfloat(new)) {
			d = 0;
			goto set_attr_error_mismatch;
		} else {
			d = (float)glow_floatvalue(new);
		}

		float *member_raw = (float *)(o_raw + offset);
		*member_raw = d;
		break;
	}
	case GLOW_ATTR_T_DOUBLE: {
		double d;

		/* ints get promoted */
		if (glow_isint(new)) {
			d = (double)glow_intvalue(new);
		} else if (!glow_isfloat(new)) {
			d = 0;
			goto set_attr_error_mismatch;
		} else {
			d = glow_floatvalue(new);
		}

		float *member_raw = (float *)(o_raw + offset);
		*member_raw = d;
		break;
	}
	case GLOW_ATTR_T_STRING: {
		if (new_class != &glow_str_class) {
			goto set_attr_error_mismatch;
		}

		GlowStrObject *str = glow_objvalue(new);

		char **member_raw = (char **)(o_raw + offset);
		*member_raw = (char *)str->str.value;
		break;
	}
	case GLOW_ATTR_T_OBJECT: {
		if (!glow_isobject(new)) {
			goto set_attr_error_mismatch;
		}

		GlowObject **member_raw = (GlowObject **)(o_raw + offset);

		if ((member_flags & GLOW_ATTR_FLAG_TYPE_STRICT) &&
		    ((*member_raw)->class != new_class)) {

			goto set_attr_error_mismatch;
		}

		GlowObject *new_o = glow_objvalue(new);
		if (*member_raw != NULL) {
			glow_releaseo(*member_raw);
		}
		glow_retaino(new_o);
		*member_raw = new_o;
		break;
	}
	}

	return glow_makefalse();

	set_attr_error_not_found:
	return glow_attr_exc_not_found(v_class, attr);

	set_attr_error_readonly:
	return glow_attr_exc_readonly(v_class, attr);

	set_attr_error_mismatch:
	return glow_attr_exc_mismatch(v_class, attr, new_class);
}

GlowValue glow_op_call(GlowValue *v,
                     GlowValue *args,
                     GlowValue *args_named,
                     const size_t nargs,
                     const size_t nargs_named)
{
	GlowClass *class = glow_getclass(v);
	const GlowCallFunc call = glow_resolve_call(class);

	if (!call) {
		return glow_type_exc_not_callable(class);
	}

	return call(v, args, args_named, nargs, nargs_named);
}

GlowValue glow_op_in(GlowValue *element, GlowValue *collection)
{
	GlowClass *class = glow_getclass(collection);
	const GlowBinOp contains = glow_resolve_contains(class);

	if (contains) {
		GlowValue ret = contains(collection, element);

		if (glow_iserror(&ret)) {
			return ret;
		}

		if (!glow_isbool(&ret)) {
			return GLOW_TYPE_EXC("contains method did not return a boolean value");
		}

		return ret;
	}

	const GlowUnOp iter_fn = glow_resolve_iter(class);

	if (!iter_fn) {
		return glow_type_exc_not_iterable(class);
	}

	GlowValue iter = iter_fn(collection);
	GlowClass *iter_class = glow_getclass(&iter);
	const GlowUnOp iternext = glow_resolve_iternext(iter_class);

	if (!iternext) {
		return glow_type_exc_not_iterator(iter_class);
	}

	while (true) {
		GlowValue next = iternext(&iter);

		if (glow_is_iter_stop(&next)) {
			break;
		}

		if (glow_iserror(&next)) {
			glow_release(&iter);
			return next;
		}

		GlowValue eq = glow_op_eq(&next, element);

		glow_release(&next);

		if (glow_iserror(&eq)) {
			glow_release(&iter);
			return eq;
		}

		if (glow_isint(&eq) && glow_intvalue(&eq) != 0) {
			glow_release(&iter);
			return glow_maketrue();
		}
	}

	glow_release(&iter);
	return glow_makefalse();
}

GlowValue glow_op_iter(GlowValue *v)
{
	GlowClass *class = glow_getclass(v);
	const GlowUnOp iter = glow_resolve_iter(class);

	if (!iter) {
		return glow_type_exc_not_iterable(class);
	}

	return iter(v);
}

GlowValue glow_op_iternext(GlowValue *v)
{
	GlowClass *class = glow_getclass(v);
	const GlowUnOp iternext = glow_resolve_iternext(class);

	if (!iternext) {
		return glow_type_exc_not_iterator(class);
	}

	return iternext(v);
}
