#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/value.h"
#include "mruby/variable.h"
#include "mruby/opcode.h"
#include "mruby/irep.h"
#include "mruby/hash.h"
#include "mruby/proc.h"
#include "opinfo.h"

void mrb_method_missing(mrb_state *mrb, mrb_sym name, mrb_value self, mrb_value args);

static mrb_irep *
copy_irep(mrb_state *mrb, mrb_irep *irep)
{
  mrb_irep *nirep = mrb_add_irep(mrb);
  int i;

  *nirep = *irep;
  /* 3 means reserved meta area */
  nirep->iseq = (mrb_code*)mrb_malloc(mrb, sizeof(mrb_code)*(irep->ilen + 3));
  for (i = 0; i < irep->ilen; i++) {
    nirep->iseq[i] = irep->iseq[i];
  }

  nirep->reps = (mrb_irep**)mrb_malloc(mrb, sizeof(mrb_irep*)*irep->rlen);
  for (i = 0; i < irep->rlen; i++) {
    nirep->reps[i] = copy_irep(mrb, irep->reps[i]);
  }

  return nirep;
}

static void
patch_reps(mrb_state *mrb, mrb_irep *irep, int a, int level) {
  int i;

  for (i = 0; i < irep->ilen; i++) {
    mrb_code code = irep->iseq[i];
    switch (GET_OPCODE(code)) {
    case OP_GETUPVAR:
    case OP_SETUPVAR:
      if (level == GETARG_C(code)) {
	code = MKOP_ABC(GET_OPCODE(code), 
			GETARG_A(code), GETARG_B(code) + a, level);
	irep->iseq[i] = code;
      }

      break;

    case OP_ENTER:
      
      code = irep->iseq[i];
      //      irep->iseq[i++] = MKOP_Ax(OP_NOP, 0);
      irep->iseq[i] = code;
      
    default:
      /* Do nothing */
      break;
    }
  }

  for (i = 0; i < irep->rlen; i++) {
    patch_reps(mrb, irep->reps[i], a, level + 1);
  }
}

static mrb_code *
patch_irep_for_inline(mrb_state *mrb, mrb_irep *src, mrb_irep *dst, int a)
{
  mrb_code *send_pc;
  mrb_code *curpos;
  mrb_code *ent;
  mrb_code code;
  int i;
  size_t symbase = dst->slen;
  size_t poolbase = dst->plen;
  size_t repsbase = dst->rlen;
  size_t entpos;
  size_t pcoff;
  int jmpptr;
  int excodenum;
  mrb_code jmptab[100];

  /* extend iseq */
  entpos = dst->ilen;
  pcoff = mrb->c->ci->pc - dst->iseq;

  /* extend regarea */
  dst->nregs += src->nregs;
  mrb->c->ci[-1].nregs = dst->nregs;

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

  /* extend reps */
  if (src->rlen > 0) {
    dst->rlen += src->rlen;
    dst->reps = mrb_realloc(mrb, dst->reps, dst->rlen * sizeof(mrb_irep *));
    for (i = 0; i < src->rlen; i++) {
      dst->reps[repsbase + i] = copy_irep(mrb, src->reps[i]);
    }
  }

  /* Patched inlined code */
  /* Jump address adjust instruction inclrease and count new isize */
  jmpptr = 0;
  excodenum = 0;
  for (i = 0; i < src->ilen; i++) {
    int incnum = 0;
    int j;
    int off;
    
    code = src->iseq[i];
    switch(GET_OPCODE(code)) {
    case OP_JMP:
    case OP_JMPIF:
    case OP_JMPNOT:
      off = GETARG_sBx(code);
      incnum = 0;
      if (off > 0) {
	for (j = i + 1; j <= i + off; j++) {
	  mrb_code cd = src->iseq[j];
	  switch (GET_OPCODE(cd)) {
	  case OP_RETURN:
	  case OP_GETIV:
	    incnum++;
	    break;
	  }
	}
      }
      else {
	for (j = i + off - 1; j < i; j++) {
	  mrb_code cd = src->iseq[j];
	  switch (GET_OPCODE(cd)) {
	  case OP_RETURN:
	  case OP_GETIV:
	    incnum--;
	    break;
	  }
	}
      }
      break;

    default:
      break;
    }

    switch(GET_OPCODE(code)) {
      /* increse instructions */
    case OP_RETURN:
    case OP_GETIV:
      excodenum++;
      break;

    case OP_JMP:
      jmptab[jmpptr++] = MKOP_sBx(GET_OPCODE(code), GETARG_sBx(code) + incnum);
      break;

    case OP_JMPIF:
    case OP_JMPNOT:
      jmptab[jmpptr++] = MKOP_AsBx(GET_OPCODE(code), GETARG_A(code), GETARG_sBx(code) + incnum);
      break;
      
    default:
      break;
    }
  }

  dst->ilen += src->ilen + 2 + excodenum; /* 2 is meta info */
  if (dst->flags & MRB_ISEQ_NO_FREE)  {
    mrb_code *new = mrb_malloc(mrb, dst->ilen * sizeof(mrb_code));
    memcpy(new, dst->iseq, dst->ilen * sizeof(mrb_code));
    dst->iseq = new;
  }
  else {
    dst->iseq = mrb_realloc(mrb, dst->iseq, dst->ilen * sizeof(mrb_code));
  }
#ifdef MRBJIT
  mrb_free(mrb, dst->prof_info);
  if (dst->jit_entry_tab) {
    mrb_free(mrb, dst->jit_entry_tab->body);
    mrb_free(mrb, dst->jit_entry_tab);
  }
  mrbjit_make_jit_entry_tab(mrb, dst, dst->ilen);
  dst->prof_info = (int *)mrb_calloc(mrb, dst->ilen, sizeof(int));
#endif
  send_pc =  dst->iseq + pcoff - 1;
  ent = dst->iseq + entpos;
  curpos = ent;

  /* Meta data */
  *(curpos++) = MKOP_A(OP_NOP, src->ilen); /* size */
  *(curpos++) = MKOP_A(OP_NOP, src->ilen);
  *send_pc = MKOP_sBx(OP_JMP, curpos - send_pc);

  jmpptr = 0;
  for (i = 0; i < src->ilen; i++) {
    code = src->iseq[i];
    switch(GET_OPCODE(code)) {
    case OP_JMP:
    case OP_JMPIF:
    case OP_JMPNOT:
      code = jmptab[jmpptr++];
      break;

    case OP_RETURN:
      code = MKOP_AB(OP_MOVE, a, GETARG_A(code) + a);
      *(curpos++) = code;
      code = MKOP_sBx(OP_JMP, send_pc - curpos + 1);
      break;


    case OP_GETIV:
      *(curpos++) = MKOP_AB(OP_MOVE, GETARG_A(code) + a, a);
      code = MKOP_ABx(OP_GETIV2, GETARG_A(code), GETARG_Bx(code));
      break;

    case OP_LOADSELF:
      code = MKOP_AB(OP_MOVE, GETARG_A(code), 0);
      break;

    case OP_LAMBDA:
      code = MKOP_Abc(OP_LAMBDA, GETARG_A(code), GETARG_b(code) + repsbase, GETARG_c(code));
      patch_reps(mrb, dst->reps[GETARG_b(code)], a, 0);
      break;

    case OP_ENTER:
      code = MKOPCODE(OP_NOP);
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
  struct RClass *curcls;
  mrb_irep *caller_irep;
  mrb_irep *callee_irep;
  mrb_value mid;
  mrb_int argc;
  mrb_value *argv;
  mrb_value iml;
  mrb_value pobj;
  mrb_value block;
  int a;
  int i;

  mrb_get_args(mrb, "*!&", &argv, &argc, &block);
  mid = argv[0];
  iml = mrb_obj_iv_get(mrb, mrb_class(mrb, self), mrb_intern_lit(mrb, "__inline_method_list__"));
  if (mrb_nil_p(iml)) {
    iml = mrb_obj_iv_get(mrb, mrb_class_ptr(self), mrb_intern_lit(mrb, "__inline_method_list__"));
    if (mrb_nil_p(iml)) {
      mrb_method_missing(mrb, mrb_symbol(mid), self, mrb_ary_new_from_values(mrb, argc, argv));
      return self;
    }
  }
  //mrb_p(mrb, iml);
  

  for (i = 1; i < argc; i++) {
    mrb->c->stack[i] = argv[i];
  }
  mrb->c->stack[argc] = block;
  caller_proc = mrb->c->ci[-1].proc;
  caller_irep = caller_proc->body.irep;
  curcls = mrb_class(mrb, self);
  if (curcls != mrb->c->ci->target_class && 0) {
    /* duplicate irep and proc and redefine as method for current class */
    mrb_irep *nirep = mrb_add_irep(mrb);
    struct RProc *nproc;

    *nirep = *caller_irep;
    caller_irep = nirep;
    nproc = (struct RProc*)mrb_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
    mrb_proc_copy(nproc, caller_proc);
    caller_proc = nproc;
    mrb_define_method_raw(mrb, curcls, mrb_symbol(mid), caller_proc);
  }

  pobj = mrb_hash_get(mrb, iml, mid);
  /*mrb_p(mrb, iml);*/
  /*mrb_p(mrb, mid);*/
  callee_proc = mrb_proc_ptr(pobj);
  callee_irep = callee_proc->body.irep;
  a = mrb->c->ci->acc;

  mrb->c->ci->pc =  patch_irep_for_inline(mrb, callee_irep, caller_irep, a);
  mrb->c->ci->target_class = 0;

  return self;
}

static mrb_value
mrb_inline_make_inline_method(mrb_state *mrb, mrb_value self)
{
  struct RObject *obj = mrb_obj_ptr(self);
  mrb_value iml;
  struct RProc *m;
  mrb_value mid;
  struct RClass *c = mrb_class_ptr(self);

  mrb_get_args(mrb, "o", &mid);

  iml = mrb_obj_iv_get(mrb, obj, mrb_intern_lit(mrb, "__inline_method_list__"));
  if (mrb_nil_p(iml)) {
    iml = mrb_hash_new(mrb);
  }

  m = mrb_method_search_vm(mrb, &c, mrb_symbol(mid));
  if (m == NULL) {
    c = mrb_class(mrb, self);
    m = mrb_method_search_vm(mrb, &c, mrb_symbol(mid));
    c = mrb_class(mrb, self);
  }
  else {
    c = mrb_class_ptr(self);
  }

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
  mrb_define_module_function(mrb, clsptr, "make_inline_method", mrb_inline_make_inline_method, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, clsptr, "method_missing", mrb_inline_missing, MRB_ARGS_ANY());

  return mrb_nil_value();
}

void
mrb_mruby_inline_gem_init(mrb_state *mrb)
{
  struct RClass *inlin;

  inlin = mrb_define_module(mrb, "Inline");
  mrb_define_class_method(mrb, inlin, "included", mrb_inline_included, MRB_ARGS_REQ(1));
}

void
mrb_mruby_inline_gem_final(mrb_state *mrb)
{
}
