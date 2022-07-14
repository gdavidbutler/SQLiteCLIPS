/*
 * SQLiteCLIPS - a SQLite virtual table for CLIPS template facts
 * Copyright (C) 2021-2022 G. David Butler <gdb@dbSystems.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sqlite3.h"
#include "clips.h"

/*
** CREATE VIRTUAL TABLE name USING CLIPS("templateName");
**
** Columns are CLIPS templates' "single" slots that allow INTEGER, FLOAT and / or STRING types
** Column ROWID (fact index) can't be set on INSERT nor changed on UPDATE
** Fact duplicates are controlled by CLIPS' setting "set-fact-duplication"
** Otherwise use EXISTS
*/

struct clpVtb {
  sqlite3_vtab v;
  sqlite3 *d;
  Environment *e;
  Deftemplate *t;
  struct {        /* slot */
    char *n;      /* name */
    enum st {     /* type bit mask */
     stNone    = 0
    ,stNil     = 1
    ,stInteger = 2
    ,stFloat   = 4
    ,stString  = 8
    } t;
  } *s;
  unsigned int n;
};

static int
clpDis(
  sqlite3_vtab *vt
){
#define V ((struct clpVtb *)vt)
  while (V->n)
    sqlite3_free((V->s + --V->n)->n);
  sqlite3_free(V->s);
  sqlite3_free(V);
  return (SQLITE_OK);
#undef V
}

static int
clpCon(
  sqlite3 *db
 ,void *ev
 ,int ac
 ,const char *const *av
 ,sqlite3_vtab **vt
 ,char **er
){
  struct clpVtb *v;
  char *s;
  CLIPSValue *p;
  CLIPSValue v1;
  CLIPSValue v2;
  unsigned long z;

  if (ac < 4) {
    *er = sqlite3_mprintf("template missing");
    return (SQLITE_ERROR);
  }
  if (!(v = sqlite3_malloc(sizeof (*v)))
   || !(s = sqlite3_mprintf("%s", *(av + 3)))) {
    sqlite3_free(v);
    return (SQLITE_NOMEM);
  }
  if (*s == '"' && *(s + (z = strlen(s)) - 1) == '"') {
    z -= 2;
    memmove(s, s + 1, z);
    *(s + z) = '\0';
  }
  v->d = db;
  v->e = ev;
  v->s = 0;
  v->n = 0;
  if (!(v->t = FindDeftemplate(v->e, s))) {
    sqlite3_free(s);
    clpDis(&v->v);
    *er = sqlite3_mprintf("template not found %s", s);
    return (SQLITE_ERROR);
  }
  sqlite3_free(s);
  DeftemplateSlotNames(v->t, &v1);
  if (!(s = sqlite3_mprintf("CREATE TABLE \"x\"("/*)*/))) {
    clpDis(&v->v);
    return (SQLITE_NOMEM);
  }
  for (z = 0; z < v1.multifieldValue->length; ++z) {
    void *t;
    char *d;
    unsigned long y;
    int st;

    if (!(p = v1.multifieldValue->contents + z)
     || !DeftemplateSlotSingleP(v->t, p->lexemeValue->contents)
     || !DeftemplateSlotTypes(v->t, p->lexemeValue->contents, &v2))
      continue;
    for (st = stNone, y = 0; y < v2.multifieldValue->length; ++y)
      if (v2.multifieldValue->contents + y)
        switch (*((v2.multifieldValue->contents + y)->lexemeValue->contents + 1)) {
        case 'Y': /* SYMBOL */
          st |= stNil;
          break;
        case 'N': /* INTEGER */
          st |= stInteger;
          break;
        case 'L': /* FLOAT */
          st |= stFloat;
          break;
        case 'T': /* STRING */
          st |= stString;
          break;
        default:
          break;
        }
    if (!(st & (stInteger | stFloat | stString)))
      continue;
    if (!(t = sqlite3_realloc(v->s, (v->n + 1) * sizeof (*v->s)))) {
      clpDis(&v->v);
      return (SQLITE_NOMEM);
    }
    v->s = t;
    (v->s + v->n)->t = st;
    if (!(st & ~(stNil | stInteger))) {
      if (st & stNil)
        d = " INTEGER";
      else
        d = " INTEGER NOT NULL";
    } else if (!(st & ~(stNil | stFloat))) {
      if (st & stNil)
        d = " REAL";
      else
        d = " REAL NOT NULL";
    } else if (!(st & ~(stNil | stString))) {
      if (st & stNil)
        d = " TEXT";
      else
        d = " TEXT NOT NULL";
    } else if (!(st & stNil))
      d = " NOT NULL";
    else
      d = "";
    if (v->n)
      s = sqlite3_mprintf("%z,\"%s\"%s", s, p->lexemeValue->contents, d);
    else
      s = sqlite3_mprintf("%z\"%s\"%s", s, p->lexemeValue->contents, d);
    if (!s || !((v->s + v->n)->n = sqlite3_mprintf("%s", p->lexemeValue->contents))) {
      clpDis(&v->v);
      return (SQLITE_NOMEM);
    }
    ++v->n;
  }
  if (!(s = sqlite3_mprintf(/*(*/"%z)", s))) {
    clpDis(&v->v);
    return (SQLITE_NOMEM);
  }
  z = sqlite3_declare_vtab(v->d, s);
  sqlite3_free(s);
  if (z) {
    clpDis(&v->v);
    return (z);
  }
  sqlite3_vtab_config(v->d, SQLITE_VTAB_CONSTRAINT_SUPPORT, 1);
  *vt = &v->v;
  return (SQLITE_OK);
}

static int
clpCrt(
  sqlite3 *db
 ,void *ev
 ,int ac
 ,const char *const *av
 ,sqlite3_vtab **vt
 ,char **er
){
  return (clpCon(db, ev, ac, av, vt, er));
}

struct clpCsr {
  sqlite3_vtab_cursor c;
  struct clpVtb *t;
  Fact *f;
  Fact **a;
  unsigned long n;
  unsigned long o;
};

static int
clpCls(
  sqlite3_vtab_cursor *vc
){
#define V ((struct clpCsr *)vc)
  if (!V->a) {
    if (V->f)
      ReleaseFact(V->f);
  } else {
    for (V->o = 0; V->o < V->n; ++V->o)
      ReleaseFact(*(V->a + V->o));
    sqlite3_free(V->a);
  }
  sqlite3_free(V);
  return (SQLITE_OK);
#undef V
}

static int
clpOpn(
  sqlite3_vtab *vt
 ,sqlite3_vtab_cursor **vc
){
#define V ((struct clpVtb *)vt)
  struct clpCsr *c;

  if (!(c = sqlite3_malloc(sizeof (*c))))
    return (SQLITE_NOMEM);
  c->t = V; 
  c->f = 0;
  c->a = 0;
  c->n = c->o = 0;
  *vc = &c->c;
  return (SQLITE_OK);
#undef V
}

static int
clpBst(
  sqlite3_vtab *vt
 ,sqlite3_index_info *ii
){
#define V ((struct clpVtb *)vt)
  int i;
  char o;

  for (i = 0; i < ii->nConstraint; ++i) {
    if ((ii->aConstraint + i)->usable) {
      switch ((ii->aConstraint + i)->op) {
      case SQLITE_INDEX_CONSTRAINT_ISNULL:
        o = 'n';
        ii->estimatedCost /= 2;
        break;
      case SQLITE_INDEX_CONSTRAINT_ISNOTNULL:
        o = 'N';
        ii->estimatedCost /= 2;
        break;
      case SQLITE_INDEX_CONSTRAINT_IS:
        o = 'i';
        if ((ii->aConstraint + i)->iColumn < 0)
          ii->estimatedCost /= 4;
        else
          ii->estimatedCost /= 5;
        break;
      case SQLITE_INDEX_CONSTRAINT_ISNOT:
        o = 'I';
        if ((ii->aConstraint + i)->iColumn < 0)
          ii->estimatedCost /= 3;
        else
          ii->estimatedCost /= 4;
        break;
      case SQLITE_INDEX_CONSTRAINT_EQ:
        o = 'e';
        if ((ii->aConstraint + i)->iColumn < 0)
          ii->estimatedCost /= 4;
        else
          ii->estimatedCost /= 5;
        break;
      case SQLITE_INDEX_CONSTRAINT_NE:
        o = 'E';
        if ((ii->aConstraint + i)->iColumn < 0)
          ii->estimatedCost /= 3;
        else
          ii->estimatedCost /= 4;
        break;
      default:
        continue;
      }
      if (!(ii->idxStr = sqlite3_mprintf("%z%c%d", ii->idxStr, o, (ii->aConstraint + i)->iColumn)))
        return (SQLITE_NOMEM);
      ++ii->idxNum;
      (ii->aConstraintUsage + i)->argvIndex = ii->idxNum;
      (ii->aConstraintUsage + i)->omit = 1;
      if ((ii->aConstraint + i)->iColumn == -1)
        ii->idxFlags = SQLITE_INDEX_SCAN_UNIQUE;
    }
  }
  if (ii->idxNum)
    ii->needToFreeIdxStr = 1;
  return (SQLITE_OK);
  (void)vt;
#undef V
}

static int
clpFlt(
  sqlite3_vtab_cursor *vc
 ,int in
 ,const char *is
 ,int ac
 ,sqlite3_value **av
){
#define V ((struct clpCsr *)vc)
  char *s;
  CLIPSValue v;
  unsigned long j;
  int i;
  int c;
  char o;

  if (!in) {
    if ((V->f = GetNextFactInTemplate(V->t->t, V->f)))
      RetainFact(V->f);
    return (SQLITE_OK);
  }
  --in;
  if (!(s = sqlite3_mprintf("(find-all-facts((?f %s))%s"/*)*/, DeftemplateName(V->t->t), in ? "(and"/*)*/ : "")))
    return (SQLITE_NOMEM);
  for (i = 0; i < ac && (o = *is++); ++i) {
    if (*is == '-') {
      for (++is; *is >= '0' && *is <= '9'; ++is);
      c = -1;
    } else
      for (c = 0; *is >= '0' && *is <= '9'; ++is)
        c = c * 10 + (*is - '0');
    switch (o) {
    case 'n': /* SQLITE_INDEX_CONSTRAINT_ISNULL */
      if (c < 0)
        s = sqlite3_mprintf("%z(eq ?f nil)", s);
      else
        s = sqlite3_mprintf("%z(eq ?f:%s nil)", s, (V->t->s + c)->n);
      break;
    case 'N': /* SQLITE_INDEX_CONSTRAINT_ISNOTNULL */
      if (c < 0)
        s = sqlite3_mprintf("%z(neq ?f nil)", s);
      else
        s = sqlite3_mprintf("%z(neq ?f:%s nil)", s, (V->t->s + c)->n);
      break;
    case 'i': /* SQLITE_INDEX_CONSTRAINT_IS */
    case 'e': /* SQLITE_INDEX_CONSTRAINT_EQ */
      if (c < 0)
        s = sqlite3_mprintf("%z(eq(fact-index ?f)%s)", s, sqlite3_value_text(*(av + i)));
      else if (sqlite3_value_type(*(av + i)) == SQLITE_NULL)
        s = sqlite3_mprintf("%z(eq ?f:%s nil)", s, (V->t->s + c)->n);
      else if (sqlite3_value_type(*(av + i)) == SQLITE_TEXT)
        s = sqlite3_mprintf("%z(eq ?f:%s \"%s\")", s, (V->t->s + c)->n, sqlite3_value_text(*(av + i)));
      else
        s = sqlite3_mprintf("%z(eq ?f:%s %s)", s, (V->t->s + c)->n, sqlite3_value_text(*(av + i)));
      break;
    case 'I': /* SQLITE_INDEX_CONSTRAINT_ISNOT */
    case 'E': /* SQLITE_INDEX_CONSTRAINT_NE */
      if (c < 0)
        s = sqlite3_mprintf("%z(neq(fact-index ?f)%s)", s, sqlite3_value_text(*(av + i)));
      else if (sqlite3_value_type(*(av + i)) == SQLITE_NULL)
        s = sqlite3_mprintf("%z(neq ?f:%s nil)", s, (V->t->s + c)->n);
      else if (sqlite3_value_type(*(av + i)) == SQLITE_TEXT)
        s = sqlite3_mprintf("%z(neq ?f:%s \"%s\")", s, (V->t->s + c)->n, sqlite3_value_text(*(av + i)));
      else
        s = sqlite3_mprintf("%z(neq ?f:%s %s)", s, (V->t->s + c)->n, sqlite3_value_text(*(av + i)));
      break;
    default:
      return (SQLITE_ERROR);
    }
    if (!s)
      return (SQLITE_NOMEM);
  }
  if (!(s = sqlite3_mprintf(/*(*/"%z%s)", s, in ? /*(*/")" : "")))
    return (SQLITE_NOMEM);
  i = Eval(V->t->e, s, &v);
  sqlite3_free(s);
  if (i)
    return (SQLITE_ERROR);
  V->n = v.multifieldValue->length;
  if (!(V->a = sqlite3_malloc((V->n ? V->n : 1) * sizeof (*V->a))))
    return (SQLITE_NOMEM);
  for (j = 0; j < V->n; ++j)
    RetainFact((*(V->a + j) = (v.multifieldValue->contents + j)->factValue));
  return (SQLITE_OK);
#undef V
}

static int
clpNxt(
  sqlite3_vtab_cursor *vc
){
#define V ((struct clpCsr *)vc)
  if (!V->a) {
    if (V->f)
      ReleaseFact(V->f);
    if ((V->f = GetNextFactInTemplate(V->t->t, V->f)))
      RetainFact(V->f);
  } else
    ++V->o;
  return (SQLITE_OK);
#define V ((struct clpCsr *)vc)
}

static int
clpEof(
  sqlite3_vtab_cursor *vc
){
#define V ((struct clpCsr *)vc)
  if (V->f || (V->a && V->o < V->n))
    return (0);
  else
    return (1);
#define V ((struct clpCsr *)vc)
}

static int
clpRid(
  sqlite3_vtab_cursor *vc
 ,sqlite3_int64 *id
){
#define V ((struct clpCsr *)vc)
  if (!V->a)
    *id = FactIndex(V->f);
  else
    *id = FactIndex(*(V->a + V->o));
  return (SQLITE_OK);
#undef V
}

static int
clpClm(
  sqlite3_vtab_cursor *vc
 ,sqlite3_context *sc
 ,int cn
){
#define V ((struct clpCsr *)vc)
  CLIPSValue v;
  int i;

  if (sqlite3_vtab_nochange(sc))
    return (SQLITE_OK);
  if (!V->a)
    i = GetFactSlot(V->f, (V->t->s + cn)->n, &v);
  else
    i = GetFactSlot(*(V->a + V->o), (V->t->s + cn)->n, &v);
  if (i)
    return (SQLITE_OK);
  switch (v.header->type) {
  case INTEGER_TYPE:
    sqlite3_result_int64(sc, v.integerValue->contents);
    break;
  case FLOAT_TYPE:
    sqlite3_result_double(sc, v.floatValue->contents);
    break;
  case STRING_TYPE:
    sqlite3_result_text(sc, v.lexemeValue->contents, -1, SQLITE_TRANSIENT);
    break;
  default:
    break;
  }
  return (SQLITE_OK);
#undef V
}

static int
clpUpd(
  sqlite3_vtab *vt
 ,int ac
 ,sqlite3_value **av
 ,sqlite3_int64 *id
){
#define V ((struct clpVtb *)vt)
  char *s;
  Fact *f;
  CLIPSValue v;
  int i;
  int j;
  int k;

  if (ac == 1) { /* delete */
    if (!(s = sqlite3_mprintf("(find-fact((?f %s))(eq(fact-index ?f)%lld))", DeftemplateName(V->t), sqlite3_value_int64(*(av + 0)))))
      return (SQLITE_NOMEM);
    i = Eval(V->e, s, &v);
    sqlite3_free(s);
    if (!i && v.multifieldValue->length)
      Retract(v.multifieldValue->contents->factValue);
  } else {
    if (sqlite3_value_type(*(av + 0)) == SQLITE_NULL) { /* insert */
      FactBuilder *b;

      if (sqlite3_value_type(*(av + 1)) != SQLITE_NULL)
        return (SQLITE_CONSTRAINT);
      if (!(b = CreateFactBuilder(V->e, DeftemplateName(V->t))))
        return (SQLITE_NOMEM);
      for (j = 2, k = 0; j < ac; ++j, ++k) {
        if (sqlite3_value_type(*(av + j)) == SQLITE_NULL && (V->s + k)->t & stNil)
          i = FBPutSlotSymbol(b, (V->s + k)->n, "nil");
        else if (sqlite3_value_type(*(av + j)) == SQLITE_INTEGER && (V->s + k)->t & stInteger)
          i = FBPutSlotInteger(b, (V->s + k)->n, sqlite3_value_int64(*(av + j)));
        else if (sqlite3_value_type(*(av + j)) == SQLITE_FLOAT && (V->s + k)->t & stFloat)
          i = FBPutSlotFloat(b, (V->s + k)->n, sqlite3_value_double(*(av + j)));
        else if ((V->s + k)->t & stString)
          i = FBPutSlotString(b, (V->s + k)->n, (const char *)sqlite3_value_text(*(av + j)));
        else
          i = 1;
        if (i) {
          FBDispose(b);
          return (SQLITE_CONSTRAINT);
        }
      }
      f = FBAssert(b);
      FBDispose(b);
      if (!f)
        return (SQLITE_CONSTRAINT);
      *id = FactIndex(f);
    } else { /* update */
      FactModifier *m;

      if (sqlite3_value_int64(*(av + 0)) != sqlite3_value_int64(*(av + 1)))
        return (SQLITE_CONSTRAINT);
      if (!(s = sqlite3_mprintf("(find-fact((?f %s))(eq(fact-index ?f)%lld))", DeftemplateName(V->t), sqlite3_value_int64(*(av + 0)))))
        return (SQLITE_NOMEM);
      i = Eval(V->e, s, &v);
      sqlite3_free(s);
      if (i || !v.multifieldValue->length)
        return (SQLITE_NOTFOUND);
      if (!(m = CreateFactModifier(V->e, v.multifieldValue->contents->factValue)))
        return (SQLITE_NOMEM);
      for (j = 2, k = 0; j < ac; ++j, ++k) {
        if (sqlite3_value_nochange(*(av + j)))
          continue;
        if (sqlite3_value_type(*(av + j)) == SQLITE_NULL && (V->s + k)->t & stNil)
          i = FMPutSlotSymbol(m, (V->s + k)->n, "nil");
        else if (sqlite3_value_type(*(av + j)) == SQLITE_INTEGER && (V->s + k)->t & stInteger)
          i = FMPutSlotInteger(m, (V->s + k)->n, sqlite3_value_int64(*(av + j)));
        else if (sqlite3_value_type(*(av + j)) == SQLITE_FLOAT && (V->s + k)->t & stFloat)
          i = FMPutSlotFloat(m, (V->s + k)->n, sqlite3_value_double(*(av + j)));
        else if ((V->s + k)->t & stString)
          i = FMPutSlotString(m, (V->s + k)->n, (const char *)sqlite3_value_text(*(av + j)));
        else
          i = 1;
        if (i) {
          FMDispose(m);
          return (SQLITE_CONSTRAINT);
        }
      }
      f = FMModify(m);
      FMDispose(m);
      if (!f)
        return (SQLITE_CONSTRAINT);
      *id = FactIndex(f);
    }
  }
  return (SQLITE_OK);
#undef V
}

static sqlite3_module clpMod = {
  3,      /* iVersion */
  clpCrt, /* xCreate */
  clpCon, /* xConnect */
  clpBst, /* xBestIndex */
  clpDis, /* xDisconnect */
  clpDis, /* xDestroy */
  clpOpn, /* xOpen */
  clpCls, /* xClose */
  clpFlt, /* xFilter */
  clpNxt, /* xNext */
  clpEof, /* xEof */
  clpClm, /* xColumn */
  clpRid, /* xRowid */
  clpUpd, /* xUpdate */
  0,      /* xBegin */
  0,      /* xSync */
  0,      /* xCommit */
  0,      /* xRollback */
  0,      /* xFindFunction */
  0,      /* xRename */
/*iVersion=2*/
  0,      /* xSavepoint */
  0,      /* xRelease */
  0,      /* xRollbackTo */
/*iVersion=3*/
  0       /* xShadowName */
};

int
sqlite3_clips_init(
  sqlite3 *db
 ,Environment *ev
){
  return (sqlite3_create_module(db, "CLIPS", &clpMod, ev));
}
