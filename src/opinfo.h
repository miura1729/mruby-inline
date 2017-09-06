enum optype {
  OPTYPE_NONE,
  OPTYPE_A,
  OPTYPE_AB,
  OPTYPE_ABC,
  OPTYPE_ABxC,
  OPTYPE_ABsC,
  OPTYPE_ABsCx,
  OPTYPE_ABCx,
  OPTYPE_ABxCx,
  OPTYPE_ABx,
  OPTYPE_ABs,
  OPTYPE_ABp,
  OPTYPE_Bx,
  OPTYPE_sBx,
  OPTYPE_AsBx,
  OPTYPE_Ax,
  OPTYPE_Abc
};

static enum optype optype_list[] = {
  /* OP_NOP=0,                                                             */
  OPTYPE_NONE,
  /* OP_MOVE,      A B     R(A) := R(B)                                    */
  OPTYPE_AB,
  /* OP_LOADL,     A Bx    R(A) := Pool(Bx)                                */
  OPTYPE_ABp,
  /* OP_LOADI,     A sBx   R(A) := sBx                                     */
  OPTYPE_AsBx,
  /* OP_LOADSYM,   A Bx    R(A) := Syms(Bx)                                */
  OPTYPE_ABs,
  /* OP_LOADNIL,   A       R(A) := nil                                     */
  OPTYPE_A,
  /* OP_LOADSELF,  A       R(A) := self                                    */
  OPTYPE_A,
  /* OP_LOADT,     A       R(A) := true                                    */
  OPTYPE_A,
  /* OP_LOADF,     A       R(A) := false                                   */
  OPTYPE_A,

  /* OP_GETGLOBAL, A Bx    R(A) := getglobal(Syms(Bx))                     */
  OPTYPE_ABs,
  /* OP_SETGLOBAL, A Bx    setglobal(Syms(Bx), R(A))                       */
  OPTYPE_ABs,
  /* OP_GETSPECIAL,A Bx    R(A) := Special[Bx]                             */
  OPTYPE_ABx,
  /* OP_SETSPECIAL,A Bx    Special[Bx] := R(A)                             */
  OPTYPE_ABx, 
  /* OP_GETIV,     A Bx    R(A) := ivget(Syms(Bx))                         */
  OPTYPE_ABs,
  /* OP_GETIV2,     A Bx    R(A) := ivget(Syms(Bx))                         */
  OPTYPE_ABs,
 /* OP_SETIV,     A Bx    ivset(Syms(Bx),R(A))                            */
  OPTYPE_ABs,
 /* OP_SETIV2,     A Bx    ivset(Syms(Bx),R(A))                            */
  OPTYPE_ABs,
  /* OP_GETCV,     A Bx    R(A) := cvget(Syms(Bx))                         */
  OPTYPE_ABs,
  /* OP_SETCV,     A Bx    cvset(Syms(Bx),R(A))                            */
  OPTYPE_ABs,
  /* OP_GETCONST,  A Bx    R(A) := constget(Syms(Bx))                      */
  OPTYPE_ABs,
  /* OP_SETCONST,  A Bx    constset(Syms(Bx),R(A))                         */
  OPTYPE_ABs,
  /* OP_GETMCNST,  A Bx    R(A) := R(A)::Syms(Bx)                          */
  OPTYPE_ABs,
  /* OP_SETMCNST,  A Bx    R(A+1)::Syms(Bx) := R(A)                        */
  OPTYPE_ABs,
  /* OP_GETUPVAR,  A B C   R(A) := uvget(B,C)                              */
  OPTYPE_ABxCx,
  /* OP_SETUPVAR,  A B C   uvset(B,C,R(A))                                 */
  OPTYPE_ABxCx,

  /* OP_JMP,       sBx     pc+=sBx                                         */
  OPTYPE_sBx,
  /* OP_JMPIF,     A sBx   if R(A) pc+=sBx                                 */
  OPTYPE_AsBx,
  /* OP_JMPNOT,    A sBx   if !R(A) pc+=sBx                                */
  OPTYPE_AsBx,
  /* OP_ONERR,     sBx     rescue_push(pc+sBx)                             */
  OPTYPE_sBx,
  /* OP_RESCUE,    A       clear(exc); R(A) := exception (ignore when A=0) */
  OPTYPE_A,
  /* OP_POPERR,    A       A.times{rescue_pop()}                           */
  OPTYPE_A,
  /* OP_RAISE,     A       raise(R(A))                                     */
  OPTYPE_A,
  /* OP_EPUSH,     Bx      ensure_push(SEQ[Bx])                            */
  OPTYPE_Bx,
  /* OP_EPOP,      A       A.times{ensure_pop().call}                      */
  OPTYPE_A,

  /* OP_SEND,      A B C   R(A) := call(R(A),Syms(B),R(A+1),...,R(A+C))    */
  OPTYPE_ABsCx,
  /* OP_SENDB,     A B C   R(A) := call(R(A),Syms(B),R(A+1),...,R(A+C),&R(A+C+1))*/
  OPTYPE_ABsCx,
  /* OP_FSEND,     A B C   R(A) := fcall(R(A),Syms(B),R(A+1),...,R(A+C-1)) */
  OPTYPE_ABsCx,
  /* OP_CALL,      A       R(A) := self.call(frame.argc, frame.argv)       */
  OPTYPE_A,
  /* OP_SUPER,     A C     R(A) := super(R(A+1),... ,R(A+C+1))             */
  OPTYPE_ABxCx,
  /* OP_ARGARY,    A Bx    R(A) := argument array (16=6:1:5:4)             */
  OPTYPE_ABx,
  /* OP_ENTER,     Ax      arg setup according to flags (23=5:5:1:5:5:1:1) */
  OPTYPE_Ax,
  /* OP_KARG,      A B C   R(A) := kdict[Syms(B)]; if C kdict.rm(Syms(B))  */
  OPTYPE_ABsCx,
  /* OP_KDICT,     A C     R(A) := kdict                                   */
  OPTYPE_ABxC,

  /* OP_RETURN,    A B     return R(A) (B=normal,in-block return/break)    */
  OPTYPE_AB,
  /* OP_TAILCALL,  A B C   return call(R(A),Syms(B),*R(C))                 */
  OPTYPE_ABsC,
  /* OP_BLKPUSH,   A Bx    R(A) := block (16=6:1:5:4)                      */
  OPTYPE_ABx,

  /* OP_ADD,       A B C   R(A) := R(A)+R(A+1) (Syms[B]=:+,C=1)            */
  OPTYPE_ABsCx,
  /* OP_ADDI,      A B C   R(A) := R(A)+C (Syms[B]=:+)                     */
  OPTYPE_ABsCx,
  /* OP_SUB,       A B C   R(A) := R(A)-R(A+1) (Syms[B]=:-,C=1)            */
  OPTYPE_ABsCx,
  /* OP_SUBI,      A B C   R(A) := R(A)-C (Syms[B]=:-)                     */
  OPTYPE_ABsCx,
  /* OP_MUL,       A B C   R(A) := R(A)*R(A+1) (Syms[B]=:*,C=1)            */
  OPTYPE_ABsCx,
  /* OP_DIV,       A B C   R(A) := R(A)/R(A+1) (Syms[B]=:/,C=1)            */
  OPTYPE_ABsCx,
  /* OP_EQ,        A B C   R(A) := R(A)==R(A+1) (Syms[B]=:==,C=1)          */
  OPTYPE_ABsCx,
  /* OP_LT,        A B C   R(A) := R(A)<R(A+1)  (Syms[B]=:<,C=1)           */
  OPTYPE_ABsCx,
  /* OP_LE,        A B C   R(A) := R(A)<=R(A+1) (Syms[B]=:<=,C=1)          */
  OPTYPE_ABsCx,
  /* OP_GT,        A B C   R(A) := R(A)>R(A+1)  (Syms[B]=:>,C=1)           */
  OPTYPE_ABsCx,
  /* OP_GE,        A B C   R(A) := R(A)>=R(A+1) (Syms[B]=:>=,C=1)          */
  OPTYPE_ABsCx,

  /* OP_ARRAY,     A B C   R(A) := ary_new(R(B),R(B+1)..R(B+C))            */
  OPTYPE_ABCx,
  /* OP_ARYCAT,    A B     ary_cat(R(A),R(B))                              */
  OPTYPE_AB,
  /* OP_ARYPUSH,   A B     ary_push(R(A),R(B))                             */
  OPTYPE_AB,
  /* OP_AREF,      A B C   R(A) := R(B)[C]                                 */
  OPTYPE_ABCx,
  /* OP_ASET,      A B C   R(B)[C] := R(A)                                 */
  OPTYPE_AB,
  /* OP_APOST,     A B C   *R(A),R(A+1)..R(A+C) := R(A)                    */
  OPTYPE_ABxCx,

  /* OP_STRING,    A Bx    R(A) := str_dup(Lit(Bx))                        */
  OPTYPE_ABp,
  /* OP_STRCAT,    A B     str_cat(R(A),R(B))                              */
  OPTYPE_AB,

  /* OP_HASH,      A B C   R(A) := hash_new(R(B),R(B+1)..R(B+C))           */
  OPTYPE_ABCx,
  /* OP_LAMBDA,    A Bz Cz R(A) := lambda(SEQ[Bz],Cz)                      */
  OPTYPE_ABxCx,
  /* OP_RANGE,     A B C   R(A) := range_new(R(B),R(B+1),C)                */
  OPTYPE_ABCx,

  /* OP_OCLASS,    A       R(A) := ::Object                                */
  OPTYPE_A,
  /* OP_CLASS,     A B     R(A) := newclass(R(A),Syms(B),R(A+1))           */
  OPTYPE_ABs,
  /* OP_MODULE,    A B     R(A) := newmodule(R(A),Syms(B))                 */
  OPTYPE_ABs,
  /* OP_EXEC,      A Bx    R(A) := blockexec(R(A),SEQ[Bx])                 */
  OPTYPE_ABx,
  /* OP_METHOD,    A B     R(A).newmethod(Syms(B),R(A+1))                  */
  OPTYPE_ABs,
  /* OP_SCLASS,    A B     R(A) := R(B).singleton_class                    */
  OPTYPE_AB,
  /* OP_TCLASS,    A       R(A) := target_class                            */
  OPTYPE_A,

  /* OP_DEBUG,     A B C   print R(A),R(B),R(C)                            */
  OPTYPE_ABC,
  /* OP_STOP,              stop VM                                         */
  OPTYPE_NONE,
  /* OP_ERR,       Bx      raise RuntimeError with message Lit(Bx)         */
  OPTYPE_Bx,

  /* OP_RSVD1,             reserved instruction #1                         */
  OPTYPE_NONE,
  /* OP_RSVD2,             reserved instruction #2                         */
  OPTYPE_NONE,
  /* OP_RSVD3,             reserved instruction #3                         */
  OPTYPE_NONE,
  /* OP_RSVD4,             reserved instruction #4                         */
  OPTYPE_NONE,
  /* OP_RSVD5,             reserved instruction #5                         */
  OPTYPE_NONE,
};
  
