/* Copyright (C) 1986, SICS, Sweden.  All rights reserved. */

#include "datadefs.h"
#include "support.h"
#include <math.h>

static char *illexp = "illegal arithmetic expression";

TAGGED fu1_minus(X0)
     TAGGED X0;
{
  register TAGGED t,t1;
  INTEGER item;
  
  t=X0; NDEREF(t,t1,illexp);
  t1 = TaggedZero-(t-TaggedZero);
  if (!(t&QMask) && TagIsSmall(t1))
    return t1;
  else if (NumberIsFloat(t))
    return MakeFloat(-GetFloat(t));
  else
    return (item = -GetInteger(t), MakeInteger(item));
}

TAGGED fu1_plus(X0)
     TAGGED X0;
{
  register TAGGED t,t1;

  t=X0; NDEREF(t,t1,illexp);
  return t;
}

TAGGED fu1_integer(X0)
     TAGGED X0;
{
  register TAGGED t,t1;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  if (NumberIsFloat(t))
    item = GetInteger(t),
    t = MakeInteger(item);
  return t;
}

TAGGED fu1_float(X0)
     TAGGED X0;
{
  register TAGGED t,t1;

  t=X0; NDEREF(t,t1,illexp);
  if (!NumberIsFloat(t))
    t = MakeFloat(GetFloat(t));
  return t;
}

TAGGED fu1_add1(X0)
     TAGGED X0;
{
  register TAGGED t,t1;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  t1 = t+(1<<SmallShift);
  if (!(t&QMask) && TagIsSmall(t1))
    return t1;
  else if (NumberIsFloat(t))
    return MakeFloat(GetFloat(t) + 1.0);
  else
    return (item = GetInteger(t) + 1, MakeInteger(item));
}

TAGGED fu1_sub1(X0)
     TAGGED X0;
{
  register TAGGED t,t1;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  t1 = t-(1<<SmallShift);
  if (!(t&QMask) && TagIsSmall(t1))
    return t1;
  else if (NumberIsFloat(t))
    return MakeFloat(GetFloat(t) - 1.0);
  else
    return (item = GetInteger(t) - 1, MakeInteger(item));
}

				/* binary functions */

TAGGED fu2_plus(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  t1 = t+(u-TaggedZero);
  if (!((t|u)&QMask) && TagIsSmall(t1))
    return t1;
  else if (NumberIsFloat(t) || NumberIsFloat(u))
    return MakeFloat(GetFloat(t) + GetFloat(u));
  else
    return (item = GetInteger(t) + GetInteger(u), MakeInteger(item));
}

TAGGED fu2_minus(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  t1 = t-(u-TaggedZero);
  if (!((t|u)&QMask) && TagIsSmall(t1))
    return t1;
  else if (NumberIsFloat(t) || NumberIsFloat(u))
    return MakeFloat(GetFloat(t) - GetFloat(u));
  else
    return (item = GetInteger(t) - GetInteger(u), MakeInteger(item));
}

TAGGED fu2_times(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  if (NumberIsFloat(t) || NumberIsFloat(u))
    return MakeFloat(GetFloat(t) * GetFloat(u));
  else
    return (item = GetInteger(t) * GetInteger(u), MakeInteger(item));
}

TAGGED fu2_fdivide(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  return MakeFloat(GetFloat(t) / GetFloat(u));
}

TAGGED fu2_idivide(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  return (item = GetInteger(t) / GetInteger(u), MakeInteger(item));
}

TAGGED fu2_mod(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  return (item = GetInteger(t) % GetInteger(u), MakeInteger(item));
}

TAGGED fu1_not(X0)
     TAGGED X0;
{
  register TAGGED t,t1;
  INTEGER item;
  
  t=X0; NDEREF(t,t1,illexp);
  if (!(t&QMask))
    return t^(QMask-(1<<SmallShift));
  else
    return (item = ~GetInteger(t), MakeInteger(item));
}

TAGGED fu2_xor(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  if (!((t|u)&QMask))
    return t^u^TaggedZero;
  else
    return (item = GetInteger(t)^GetInteger(u), MakeInteger(item));
}

TAGGED fu2_and(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  if (!((t|u)&QMask))
    return ((t^ZMask)&(u^ZMask))^ZMask;
  else
    return (item = GetInteger(t) & GetInteger(u), MakeInteger(item));
}

TAGGED fu2_or(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  if (!((t|u)&QMask))
    return ((t^ZMask)|(u^ZMask))^ZMask;
  else
    return (item = GetInteger(t) | GetInteger(u), MakeInteger(item));
}

TAGGED fu2_lsh(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  register INTEGER i;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  i = GetInteger(u);
  item = GetInteger(t);
  item = (i<0 ? item >> (-i) : item << i);
  return MakeInteger(item);
}

TAGGED fu2_rsh(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;
  register INTEGER i;
  INTEGER item;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  i = GetInteger(u);
  item = GetInteger(t);
  item = (i<0 ? item << (-i) : item >> i);
  return MakeInteger(item);
}

/*---------------------------------------------------------------------------*/

TAGGED fu1_sin(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeFloat(sin(GetFloat(t)));
}

TAGGED fu1_cos(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeFloat(cos(GetFloat(t)));
}

TAGGED fu1_tan(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeFloat(tan(GetFloat(t)));
}

TAGGED fu1_atan(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeFloat(atan(GetFloat(t)));
}

TAGGED fu1_sqrt(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeFloat(sqrt(GetFloat(t)));
}

TAGGED fu2_pow(X0,X1)
     TAGGED X0,X1;
{
  register TAGGED t1,t,u;

  t=X0; NDEREF(t,t1,illexp);
  u=X1; NDEREF(u,t1,illexp);
  return MakeFloat(pow(GetFloat(t), GetFloat(u)));
}

TAGGED fu1_round(X0)
     TAGGED X0;
{
  register TAGGED t1,t;
#if hpux
  double td ;
  short sign ;
#endif

  t=X0; NDEREF(t,t1,illexp);
#if ! hpux
  return MakeInteger(irint(GetFloat(t)));
#else
  td = GetFloat( t ) ;
  sign = td < 0 ;
  return MakeInteger( ( unsigned long int )( sign ? - floor(fabs(td)+0.5) : floor(fabs(td)+0.5) ) ) ;
#endif
}

TAGGED fu1_floor(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeInteger((INTEGER)floor(GetFloat(t)));
}

TAGGED fu1_ceil(X0)
     TAGGED X0;
{
  register TAGGED t1,t;

  t=X0; NDEREF(t,t1,illexp);
  return MakeInteger((INTEGER)ceil(GetFloat(t)));
}

TAGGED fu1_abs(X0)
     TAGGED X0;
{
  register TAGGED t1,t;
  INTEGER i;
  FLOAT f;

  t=X0; NDEREF(t,t1,illexp);
  if (IsFloat(t))
    return MakeFloat(((f=GetFloat(t))>=0 ? f : -f));
  return MakeInteger(((i=GetInteger(t))>=0 ? i : -i));
}
