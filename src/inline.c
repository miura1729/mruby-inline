#include "mruby.h"
#include "mruby/class.h"
#include "mruby/value.h"
#include "mruby/variable.h"
#include "mruby/opcode.h"
#include "mruby/irep.h"
#include "mruby/hash.h"
#include "mruby/proc.h"
#include "opinfo.h"

static mrb_code *
patch_irep_for_inline(mrb_state *mrb, mrb_irep *src, mrb_irep *dst, int a)
{
  mrb_code *send_pc;
  mrb_code *ent;
  mrb_code *curpos;
  mrb_code code;
  int i;
  size_t symbase = dst->slen;
  size_t poolbase = dst->plen;

  /* extend iseq */
  ent = dst->iseq + dst->ilen;
  curpos = ent;
  dst->ilen += src->ilen + 2;	/* 2 means meta info */
  dst->iseq = mrb_realloc(mrb, dst->iseq, dst->ilen * sizeof(mrb_code));
  send_pc = mrb->c->ci->pc - 1;

  /* extend syms */
  if (src->slen > 0) {
    dst->slen += src->slen;
    dst->syms = mrb_realloc(mrb, dst->syms, dst->slen * sizeof(mrb_sym));
    for (i = 0; i < src->slen; i++) {
      dst->syms[symbase + i] = src->syms[i];
    }
  }

  /* extend pool */
  if (src->plen > 0) {
    dst->plen += src->plen;
    dst->pool = mrb_realloc(mrb, dst->pool, dst->plen * sizeof(mrb_value));
    for (i = 0; i < src->plen; i++) {
      dst->pool[poolbase + i] = src->pool[i];
    }
  }

  /* Meta data */
  *(curpos++) = MKOP_A(OP_NOP, src->ilen); /* size */
  *(curpos++) = MKOP_A(OP_NOP, src->ilen);
  *send_pc = MKOP_sBx(OP_JMP, curpos - send_pc);

  /* Patched inlined code */
  for (i = 0; i < src->ilen; i++) {
    code = src->iseq[i];
    switch(GET_OPCODE(code)) {
    case OP_RETURN:
      code = MKOP_sBx(OP_JMP, send_pc - curpos + 1);
      break;
    default:
      /* do nothing */
      break;
    }

    /* Shift regster number */
    switch(optype_list[GET_OPCODE(code)]) {
    case OPTYPE_NONE:
    case OPTYPE_Bx:
    case OPTYPE_sBx:
    case OPTYPE_Ax:
      /* do nothing */
      break;

    case OPTYPE_A:
      code = MKOP_A(GET_OPCODE(code), GETARG_A(code) + a);
      break;

    case OPTYPE_AB:
      code = MKOP_AB(GET_OPCODE(code), GETARG_A(code) + a, GETARG_B(code) + a);
      break;

    case OPTYPE_ABC:
      code = MKOP_ABC(GET_OPCODE(code), 
		      GETARG_A(code) + a, GETARG_B(code) + a, GETARG_C(code) + a);
      break;

    case OPTYPE_ABsC:
      code = MKOP_ABC(GET_OPCODE(code), 
		      GETARG_A(code) + a, GETARG_B(code) + symbase, GETARG_C(code) + a);
      break;

    case OPTYPE_ABxC:
      code = MKOP_ABC(GET_OPCODE(code), 
		      GETARG_A(code) + a, GETARG_B(code), GETARG_C(code) + a);
      break;

    case OPTYPE_ABCx:
      code = MKOP_ABC(GET_OPCODE(code), 
		      GETARG_A(code) + a, GETARG_B(code) + a, GETARG_C(code));
      break;


    case OPTYPE_ABxCx:
      code = MKOP_ABC(GET_OPCODE(code), 
		      GETARG_A(code) + a, GETARG_B(code), GETARG_C(code));
      break;

    case OPTYPE_ABsCx:
      code = MKOP_ABC(GET_OPCODE(code), 
		      GETARG_A(code) + a, GETARG_B(code) + symbase, GETARG_C(code));
      break;

    case OPTYPE_ABx:
      code = MKOP_ABx(GET_OPCODE(code), GETARG_A(code) + a, GETARG_Bx(code));
      break;

    case OPTYPE_ABp:
      code = MKOP_ABx(GET_OPCODE(code), GETARG_A(code) + a, GETARG_Bx(code) + poolbase);
      break;

    case OPTYPE_ABs:
      code = MKOP_ABx(GET_OPCODE(code), GETARG_A(code) + a, GETARG_Bx(code) + symbase);
      break;

    case OPTYPE_AsBx:
      code = MKOP_AsBx(GET_OPCODE(code), GETARG_A(code) + a, GETARG_sBx(code));
      break;

    }

    *(curpos++) = code;
  }

  return ent + 2;
}

static mrb_value
mrb_inline_missing(mrb_state *mrb, mrb_value self)
{
  struct RProc *caller_proc;
  struct RProc *callee_proc;
  mrb_irep *caller_irep;
  mrb_irep *callee_irep;
  mrb_value mid;
  mrb_int argc;
  mrb_value *argv;
  mrb_value iml;
  mrb_value pobj;
  mrb_code *entry;
  int a;

  mrb_get_args(mrb, "o*", &mid, &argv, &argc);
  
  caller_proc = mrb->c->ci[-1].proc;
  caller_irep = caller_proc->body.irep;

  iml = mrb_obj_iv_get(mrb, mrb_obj_ptr(self), mrb_intern_lit(mrb, "__inline_method_list__"));
  
  pobj = mrb_hash_get(mrb, iml, mid);
  callee_proc = mrb_proc_ptr(pobj);
  callee_irep = callee_proc->body.irep;
  a = mrb->c->ci->acc;
  if (caller_irep->nregs < callee_irep->nregs + a) {
    caller_irep->nregs = callee_irep->nregs + a;
  }
  entry = patch_irep_for_inline(mrb, callee_irep, caller_irep, a);

  return self;
}

static mrb_value
mrb_inline_make_inline_method(mrb_state *mrb, mrb_value self)
{
  struct RObject *obj = mrb_obj_ptr(self);
  mrb_value iml;
  struct RProc *m;
  mrb_value mid;
  struct RClass *c = mrb_class(mrb, self);

  mrb_get_args(mrb, "o", &mid);

  iml = mrb_obj_iv_get(mrb, obj, mrb_intern_lit(mrb, "__inline_method_list__"));
  if (mrb_nil_p(iml)) {
    iml = mrb_hash_new(mrb);
  }

  m = mrb_method_search_vm(mrb, &c, mrb_symbol(mid));
  mrb_hash_set(mrb, iml, mid, mrb_obj_value(m));
  mrb_obj_iv_set(mrb, obj, mrb_intern_lit(mrb, "__inline_method_list__"), iml);
  mrb_undef_method(mrb, c, mrb_sym2name(mrb, mrb_symbol(mid)));

  return self;
}

mrb_value
mrb_inline_included(mrb_state *mrb, mrb_value self)
{
  mrb_value klass;
  struct RClass *clsptr;
  
  mrb_get_args(mrb, "o", &klass);
  clsptr = mrb_class_ptr(klass);
  mrb_define_method(mrb, clsptr, "make_inline_method", mrb_inline_make_inline_method, MRB_ARGS_REQ(1));

  return mrb_nil_value();
}

void
mrb_mruby_inline_gem_init(mrb_state *mrb)
{
  struct RClass *inlin;

  inlin = mrb_define_module(mrb, "Inline");
  mrb_define_class_method(mrb, inlin, "included", mrb_inline_included, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, inlin, "method_missing",          mrb_inline_missing,          MRB_ARGS_ANY());
}

void
mrb_mruby_inline_gem_final(mrb_state *mrb)
{
}
