#include "mruby.h"
#include "mruby/class.h"
#include "mruby/value.h"
#include "mruby/variable.h"
#include "mruby/data.h"
#include "mruby/opcode.h"
#include "mruby/irep.h"
#include "mruby/hash.h"
#include "mruby/proc.h"

static mrb_value
mrb_inline_missing(mrb_state *mrb, mrb_value self)
{
  return self;
}

static void
mrb_irepobj_free(mrb_state *mrb, void *ptr)
{
  /* Do nothing */
}

static struct mrb_data_type mrb_irep_type = { "Irep", mrb_irepobj_free };

static mrb_value
mrb_irep_wrap(mrb_state *mrb, struct RClass *tc, struct mrb_irep *irep)
{
  return mrb_obj_value(Data_Wrap_Struct(mrb, tc, &mrb_irep_type, irep));
}

static mrb_value
mrb_inline_make_inline_method(mrb_state *mrb, mrb_value self)
{
  struct RObject *obj = mrb_obj_ptr(self);
  mrb_value iml;
  mrb_irep *irep;
  struct RProc *m;
  mrb_value mid;
  struct RClass *c = mrb_class(mrb, self);

  mrb_get_args(mrb, "o", &mid);

  iml = mrb_obj_iv_get(mrb, obj, mrb_intern_lit(mrb, "__inline_method_list__"));
  if (mrb_nil_p(iml)) {
    iml = mrb_hash_new(mrb);
  }

  m = mrb_method_search_vm(mrb, &c, mrb_symbol(mid));
  irep = m->body.irep;
  mrb_hash_set(mrb, iml, mid, mrb_irep_wrap(mrb, c, irep));
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
