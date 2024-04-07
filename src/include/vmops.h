#ifndef GLOW_VMOPS_H
#define GLOW_VMOPS_H

#include "strobject.h"
#include "object.h"

GlowValue glow_op_hash(GlowValue *v);

GlowValue glow_op_str(GlowValue *v);

GlowValue glow_op_print(GlowValue *v, FILE *out);

GlowValue glow_op_add(GlowValue *a, GlowValue *b);

GlowValue glow_op_sub(GlowValue *a, GlowValue *b);

GlowValue glow_op_mul(GlowValue *a, GlowValue *b);

GlowValue glow_op_div(GlowValue *a, GlowValue *b);

GlowValue glow_op_mod(GlowValue *a, GlowValue *b);

GlowValue glow_op_pow(GlowValue *a, GlowValue *b);

GlowValue glow_op_bitand(GlowValue *a, GlowValue *b);

GlowValue glow_op_bitor(GlowValue *a, GlowValue *b);

GlowValue glow_op_xor(GlowValue *a, GlowValue *b);

GlowValue glow_op_bitnot(GlowValue *a);

GlowValue glow_op_shiftl(GlowValue *a, GlowValue *b);

GlowValue glow_op_shiftr(GlowValue *a, GlowValue *b);

GlowValue glow_op_and(GlowValue *a, GlowValue *b);

GlowValue glow_op_or(GlowValue *a, GlowValue *b);

GlowValue glow_op_not(GlowValue *a);

GlowValue glow_op_eq(GlowValue *a, GlowValue *b);

GlowValue glow_op_neq(GlowValue *a, GlowValue *b);

GlowValue glow_op_lt(GlowValue *a, GlowValue *b);

GlowValue glow_op_gt(GlowValue *a, GlowValue *b);

GlowValue glow_op_le(GlowValue *a, GlowValue *b);

GlowValue glow_op_ge(GlowValue *a, GlowValue *b);

GlowValue glow_op_plus(GlowValue *a);

GlowValue glow_op_minus(GlowValue *a);

GlowValue glow_op_iadd(GlowValue *a, GlowValue *b);

GlowValue glow_op_isub(GlowValue *a, GlowValue *b);

GlowValue glow_op_imul(GlowValue *a, GlowValue *b);

GlowValue glow_op_idiv(GlowValue *a, GlowValue *b);

GlowValue glow_op_imod(GlowValue *a, GlowValue *b);

GlowValue glow_op_ipow(GlowValue *a, GlowValue *b);

GlowValue glow_op_ibitand(GlowValue *a, GlowValue *b);

GlowValue glow_op_ibitor(GlowValue *a, GlowValue *b);

GlowValue glow_op_ixor(GlowValue *a, GlowValue *b);

GlowValue glow_op_ishiftl(GlowValue *a, GlowValue *b);

GlowValue glow_op_ishiftr(GlowValue *a, GlowValue *b);

GlowValue glow_op_get(GlowValue *v, GlowValue *idx);

GlowValue glow_op_set(GlowValue *v, GlowValue *idx, GlowValue *e);

GlowValue glow_op_apply(GlowValue *v, GlowValue *fn);

GlowValue glow_op_iapply(GlowValue *v, GlowValue *fn);

GlowValue glow_op_get_attr(GlowValue *v, const char *attr);

GlowValue glow_op_get_attr_default(GlowValue *v, const char *attr);

GlowValue glow_op_set_attr(GlowValue *v, const char *attr, GlowValue *new);

GlowValue glow_op_set_attr_default(GlowValue *v, const char *attr, GlowValue *new);

GlowValue glow_op_call(GlowValue *v,
                 GlowValue *args,
                 GlowValue *args_named,
                 const size_t nargs,
                 const size_t nargs_named);

GlowValue glow_op_in(GlowValue *element, GlowValue *collection);

GlowValue glow_op_iter(GlowValue *v);

GlowValue glow_op_iternext(GlowValue *v);

#endif /* GLOW_VMOPS_H */
