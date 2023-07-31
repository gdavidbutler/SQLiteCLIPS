/*
 * SQLiteCLIPS - a SQLite virtual table for CLIPS template facts
 * Copyright (C) 2021-2023 G. David Butler <gdb@dbSystems.com>
 *
 * This file is part of SQLiteCLIPS
 *
 * SQLiteCLIPS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SQLiteCLIPS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#if defined(__linux__)
# include <netinet/ether.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <net/ethernet.h>
#endif
#include <arpa/inet.h>
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

/*
 * patterned from Oracle's INSTR
 * integer = INSTR(text,subText) (same as SQLite's built-in version)
 *  find position of first subText in text from beginning
 * integer = INSTR(text,subText,positionInteger)
 *  find position of first subText in text starting at +position from beginning or -position from end
 * integer = INSTR(text,subText,positionInteger,occurrenceInteger)
 *  find position of +occurrence of subText in text starting at +position from beginning or -position from end
 */
static void instrFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  const unsigned char *zHaystack;
  const unsigned char *zNeedle;
  int nHaystack;
  int nNeedle;
  int typeHaystack, typeNeedle;
  int N = 0;

  typeHaystack = sqlite3_value_type(argv[0]);
  typeNeedle = sqlite3_value_type(argv[1]);
  if( typeHaystack==SQLITE_NULL || typeNeedle==SQLITE_NULL ) return;
  nHaystack = sqlite3_value_bytes(argv[0]);
  nNeedle = sqlite3_value_bytes(argv[1]);
  if( nNeedle>0 ){
    if( typeHaystack==SQLITE_BLOB && typeNeedle==SQLITE_BLOB ){
      zHaystack = sqlite3_value_blob(argv[0]);
      zNeedle = sqlite3_value_blob(argv[1]);
    }else{
      zHaystack = sqlite3_value_text(argv[0]);
      zNeedle = sqlite3_value_text(argv[1]);
    }
    if( zNeedle==0 || (nHaystack && zHaystack==0) ) return;
    if( nNeedle <= nHaystack ){
      const unsigned char *p1;
      const unsigned char *p2;
      const unsigned char *zNeedleTail;
      int zNeedleTailOffset;
      int position;
      int occurrence;
      int j;

      if( argc<3 || SQLITE_NULL==sqlite3_value_type(argv[2]) || (position = sqlite3_value_int(argv[2]))==0 )
        position = 1;
      if( argc<4 || SQLITE_NULL==sqlite3_value_type(argv[3]) || (occurrence = sqlite3_value_int(argv[3]))<=0 )
        occurrence = 1;
      zNeedleTailOffset = nNeedle - 1;
      zNeedleTail = zNeedle + zNeedleTailOffset;
      if( position>0 ){
        for( N = position - 1, zHaystack += N; N < nHaystack; ++N, ++zHaystack ){
          if( *zNeedle!=*zHaystack ) continue;
          if( nHaystack - N<nNeedle ){
            N = nHaystack;
            break;
          }
          if( *(zHaystack + zNeedleTailOffset)!=*zNeedleTail ) continue;
          for( j = 1, p1 = zNeedle + 1, p2 = zHaystack + 1; j<zNeedleTailOffset && *p1==*p2; ++j, ++p1, ++p2 );
          if( j>=zNeedleTailOffset && !--occurrence ) break;
        }
        if( N==nHaystack ) N = 0;
        else ++N;
      }else{
        for( N = nHaystack + position, zHaystack += N; N>=0; --N, --zHaystack ){
          if( *zNeedleTail!=*zHaystack ) continue;
          if( N + 1<nNeedle ){
            N = -1;
            break;
          }
          if( *(zHaystack - zNeedleTailOffset)!=*zNeedle ) continue;
          for( j = zNeedleTailOffset - 1, p1 = zNeedleTail - 1, p2 = zHaystack - 1; j>=0 && *p1==*p2; --j, --p1, --p2 );
          if( j<=0 && !--occurrence ) break;
        }
        if( N<0 ) N = 0;
        else N -= zNeedleTailOffset - 1;
      }
    }
  }
  sqlite3_result_int(context, N);
}

/* blob = BLOB(hexText) is the inverse of SQLite's built-in hexText = HEX(blob) */
static void
blobFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  static unsigned char const hex[] = {
    16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   , 0,  1,  2,  3,   4,  5,  6,  7,   8,  9, 16, 16,  16, 16, 16, 16
   ,16, 10, 11, 12,  13, 14, 15, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 10, 11, 12,  13, 14, 15, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
   ,16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16,  16, 16, 16, 16
  };
  const unsigned char *t;
  unsigned char *b;
  int i;

  (void)argc;
  if ((t = sqlite3_value_text(argv[0]))
   && (i = sqlite3_value_bytes(argv[0]))
   && !(i & 1)
   && (i /= 2)
   && (b = sqlite3_malloc(i))) {
    unsigned char *p;

    p = b;
    for (p = b; i; --i, ++p) {
      unsigned char c1;
      unsigned char c2;

      if ((c1 = hex[*t++]) == 16)
        break;
      if ((c2 = hex[*t++]) == 16)
        break;
       *p = c1 << 4 | c2;
    }
    if (!i)
      sqlite3_result_blob(context, b, p - b, sqlite3_free);
    else
      sqlite3_free(b);
  }
}

/* blob = BLOB_NOT(blob) bitwise "not (ones' complement)" of a blob */
static void
notFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const unsigned char *b;
  unsigned char *r;
  unsigned int i;
  unsigned int j;

  (void)argc;
  if ((b = sqlite3_value_blob(argv[0]))
   && (i = sqlite3_value_bytes(argv[0]))
   && (r = sqlite3_malloc(i))) {
    j = i;
    while (j) {
      --j;
      r[j] = ~ b[j];
    }
    sqlite3_result_blob(context, r, i, sqlite3_free);
  }
}

/* blob = BLOB_XXX(blob1,blob2) bitwise "OP" of two blobs */
static void
xxxFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const unsigned char *b1;
  const unsigned char *b2;
  unsigned char *r;
  unsigned int i;
  unsigned int j;

  (void)argc;
  if ((b1 = sqlite3_value_blob(argv[0]))
   && (b2 = sqlite3_value_blob(argv[1]))
   && (i = sqlite3_value_bytes(argv[0]))
   && (j = sqlite3_value_bytes(argv[1]))
   && i == j
   && (r = sqlite3_malloc(i))) {
    switch ((long)sqlite3_user_data(context)) {
    case 0: /* and */
      while (j) {
        --j;
        r[j] = b1[j] & b2[j];
      }
      break;
    case 1: /* or */
      while (j) {
        --j;
        r[j] = b1[j] | b2[j];
      }
      break;
    case 2: /* xor */
      while (j) {
        --j;
        r[j] = b1[j] ^ b2[j];
      }
      break;
    default:
      return;
    }
    sqlite3_result_blob(context, r, i, sqlite3_free);
  }
}

struct xxxCtx {
  unsigned char *b;
  unsigned int l;
};

/* aggregate blob = BLOB_XXX(blob) bitwise "OP" of blobs */
static void
xxxStep(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  struct xxxCtx *c;

  (void)argc;
  if ((c = sqlite3_aggregate_context(context, sizeof(*c)))) {
    const unsigned char *b;
    unsigned int i;

    if ((b = sqlite3_value_blob(argv[0]))
     && (i = sqlite3_value_bytes(argv[0]))) {
      if (!c->l && !c->b && (c->b = sqlite3_malloc(i))) {
        c->l = i;
        while (i) {
          --i;
          c->b[i] = b[i];
        }
      } else if (c->l != i) {
        sqlite3_free(c->b);
        c->b = 0;
      } else switch ((long)sqlite3_user_data(context)) {
      case 0: /* and */
        while (i) {
          --i;
          c->b[i] &= b[i];
        }
        break;
      case 1: /* or */
        while (i) {
          --i;
          c->b[i] |= b[i];
        }
        break;
      case 2: /* xor */
        while (i) {
          --i;
          c->b[i] ^= b[i];
        }
        break;
      default:
        sqlite3_free(c->b);
        c->b = 0;
        break;
      }
    }
  }
}

static void
xxxFinalize(
  sqlite3_context *context
){
  struct xxxCtx *c;

  if (!(c = sqlite3_aggregate_context(context, 0))
   || !c->b)
    sqlite3_result_error(context,"blob size mis-match",-1);
  else
    sqlite3_result_blob(context, c->b, c->l, sqlite3_free);
}

/* blob = BLOB_SHX(blob,integer) bitwise shift (shifting in zero bits) a blob */
static void
shFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const unsigned char *b;
  unsigned char *r;
  unsigned int i;
  unsigned int j;

  (void)argc;
  if ((b = sqlite3_value_blob(argv[0]))
   && (i = sqlite3_value_bytes(argv[0]))
   && (int)(j = sqlite3_value_int(argv[1])) >= 0
   && j <= i * 8
   && (r = sqlite3_malloc(i))) {
    unsigned int f; /* full bytes */
    unsigned int p; /* partial bits */
    unsigned int k;

    f = j / 8;
    p = j % 8;
    k = 0;
    switch ((long)sqlite3_user_data(context)) {
    case 0: /* right */
      while (k < f) {
        r[k] = 0x00;
        ++k;
      }
      if (!p)
        while (k < i) {
          r[k] = b[k - f];
          ++k;
        }
      else {
        unsigned char c; /* carry bits */

        c = 0x00;
        while (k < i) {
          r[k] = c | b[k - f] >> p;
          c = b[k - f] << (8 - p);
          ++k;
        }
      }
      break;
    case 1: /* left */
      while (k < f) {
        ++k;
        r[i - k] = 0x00;
      }
      if (!p)
        while (k < i) {
          ++k;
          r[i - k] = b[i - k + f];
        }
      else {
        unsigned char c; /* carry bits */

        c = 0x00;
        while (k < i) {
          ++k;
          r[i - k] = c | b[i - k + f] << p;
          c = b[i - k + f] >> (8 - p);
        }
      }
      break;
    default:
      return;
    }
    sqlite3_result_blob(context, r, i, sqlite3_free);
  }
}

/* integer = BLOB_CXZ(blob) bitwise count zeros of a blob */
static void
czFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const unsigned char *b;
  sqlite3_int64 z;
  unsigned int i;
  unsigned int c;

  (void)argc;
  if ((b = sqlite3_value_blob(argv[0]))
   && (i = sqlite3_value_bytes(argv[0]))) {
    z = 0;
    switch ((long)sqlite3_user_data(context)) {
    case 0: /* leading */
      for (; i && !*b; --i, ++b)
        z += 8;
      if (i)
        for (i = 8, c = *b; i && !(c & 0x80); --i, c <<= 1)
          ++z;
      break;
    case 1: /* trailing */
      for (b += i - 1; i && !*b; --i, --b)
        z += 8;
      if (i)
        for (i = 8, c = *b; i && !(c & 0x01); --i, c >>= 1)
          ++z;
      break;
    default:
      return;
    }
    sqlite3_result_int64(context, z);
  }
}

/* SELECT rowid,value FROM BLOB_BIT(blob) */
static int
bitCon(
  sqlite3 *d
 ,void *u1
 ,int u2
 ,const char *const *u3
 ,sqlite3_vtab **v
 ,char **u4
){
  int i;

  if ((i = sqlite3_declare_vtab(d, "CREATE TABLE x(value INTEGER, blob BLOB HIDDEN)")))
    return (i);
  if (!(*v = sqlite3_malloc(sizeof (**v))))
    return (SQLITE_NOMEM);
  memset(*v, 0, sizeof (**v));
  sqlite3_vtab_config(d, SQLITE_VTAB_INNOCUOUS);
  return (SQLITE_OK);
  (void)u1;
  (void)u2;
  (void)u3;
  (void)u4;
}

static int
bitDis(
  sqlite3_vtab *v
){
  sqlite3_free(v);
  return (SQLITE_OK);
}

struct bitCsr {
  sqlite3_vtab_cursor c;
  unsigned char *b;
  sqlite3_int64 i;
  unsigned int l;
};

static int
bitOpn(
  sqlite3_vtab *u1
 ,sqlite3_vtab_cursor **c
){
  struct bitCsr *xc;

  if (!(xc = sqlite3_malloc(sizeof (*xc))))
    return (SQLITE_NOMEM);
  memset(xc, 0, sizeof (*xc));
  *c = &xc->c;
  return (SQLITE_OK);
  (void)u1;
}

static int
bitCls(
  sqlite3_vtab_cursor *c
){
  sqlite3_free(((struct bitCsr *)c)->b);
  sqlite3_free(c);
  return (SQLITE_OK);
}

static int
bitBst(
  sqlite3_vtab *u1,
  sqlite3_index_info *i
){
  const struct sqlite3_index_constraint *c;
  int j;
  
  i->estimatedCost = 1000000000.0;
  i->estimatedRows = 1000000000;
  for (j = 0, c = i->aConstraint; j < i->nConstraint; ++j, ++c) {
    if (c->iColumn != 1
     || c->op != SQLITE_INDEX_CONSTRAINT_EQ
     || !c->usable)
      continue;
    i->aConstraintUsage[j].argvIndex = 1;
    i->aConstraintUsage[j].omit = 1;
    i->estimatedCost = 10.0;
    i->estimatedRows = 10;
    break;
  }
  return (SQLITE_OK);
  (void)u1;
}

static int
bitFlt(
  sqlite3_vtab_cursor *c
 ,int u1
 ,const char *u2
 ,int n
 ,sqlite3_value **a
){
  const unsigned char *b;

  sqlite3_free(((struct bitCsr *)c)->b);
  if (n
   && (b = sqlite3_value_blob(*a))
   && (((struct bitCsr *)c)->l = sqlite3_value_bytes(*a))
   && (((struct bitCsr *)c)->b = sqlite3_malloc(((struct bitCsr *)c)->l)))
    memcpy(((struct bitCsr *)c)->b, b, ((struct bitCsr *)c)->l);
  else
    ((struct bitCsr *)c)->b = 0;
  ((struct bitCsr *)c)->i = 0;
  return (SQLITE_OK);
  (void)u1;
  (void)u2;
}

static int
bitNxt(
  sqlite3_vtab_cursor *c
){
  ++((struct bitCsr *)c)->i;
  return (SQLITE_OK);
}

static int
bitEof(
  sqlite3_vtab_cursor *c
){
  return (((struct bitCsr *)c)->i >= ((struct bitCsr *)c)->l * 8); 
}

static int
bitRid(
  sqlite3_vtab_cursor *c
 ,sqlite3_int64 *i
){
  *i = ((struct bitCsr *)c)->i;
  return (SQLITE_OK);
}

static int
bitClm(
  sqlite3_vtab_cursor *c
 ,sqlite3_context *x
 ,int i
){
  if (((struct bitCsr *)c)->b) {
    if (!i)
      sqlite3_result_int(x, (*(((struct bitCsr *)c)->b + ((((struct bitCsr *)c)->l - 1) - ((struct bitCsr *)c)->i / 8)) >> (((struct bitCsr *)c)->i % 8)) & 0x01);
    else
      sqlite3_result_blob(x, ((struct bitCsr *)c)->b, ((struct bitCsr *)c)->l, 0);
  }
  return (SQLITE_OK);
}

static sqlite3_module bitMod = {
  0,      /* iVersion */
  0,      /* xCreate */
  bitCon, /* xConnect */
  bitBst, /* xBestIndex */
  bitDis, /* xDisconnect */
  0,      /* xDestroy */
  bitOpn, /* xOpen - open a cursor */
  bitCls, /* xClose - close a cursor */
  bitFlt, /* xFilter - configure scan constraints */
  bitNxt, /* xNext - advance a cursor */
  bitEof, /* xEof - check for end of scan */
  bitClm, /* xColumn - read data */
  bitRid, /* xRowid - read data */
  0,      /* xUpdate */
  0,      /* xBegin */
  0,      /* xSync */
  0,      /* xCommit */
  0,      /* xRollback */
  0,      /* xFindMethod */
  0,      /* xRename */
  0,      /* xSavepoint */
  0,      /* xRelease */
  0,      /* xRollbackTo */
  0       /* xShadowName */
};

/* integer = STRTOLL(numberText,baseInteger) is C library's strtoll(text,base), base 0,2-36 */
static void
strtollFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const char *t;
  int b;

  (void)argc;
  if ((t = (const char *)sqlite3_value_text(argv[0]))) {
    if ((b = sqlite3_value_int(argv[1])) < 2)
      b = 0;
    else if (b > 36)
      b = 36;
    sqlite3_result_int64(context, strtoll(t, 0, b));
  }
}

/* macText = ETHER_NTOA(blob(6)) is C library's ether_ntoa(addr,buf) */
static void
ntoaFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const unsigned char *b;
  char *r;

  (void)argc;
  if ((b = sqlite3_value_blob(argv[0]))
   && sqlite3_value_bytes(argv[0]) == 6
   && (r = sqlite3_malloc(18))) {
    if (ether_ntoa_r((struct ether_addr *)b, r)) {
      r[17] = '\0'; /* strlen safety */
      sqlite3_result_text(context, r, strlen(r), sqlite3_free);
    } else
      sqlite3_free(r);
  }
}

/* blob(6) = ETHER_ATON(macText) is C library's ether_aton(addr,buf) */
static void
atonFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  const char *t;
  unsigned char *b;

  (void)argc;
  if ((t = (const char *)sqlite3_value_text(argv[0]))
   && (b = sqlite3_malloc(6))) {
    if (ether_aton_r(t, (struct ether_addr *)b))
      sqlite3_result_blob(context, b, 6, sqlite3_free);
    else
      sqlite3_free(b);
  }
}

/*
 * ipText = INET_NTOP(blob(4)) is C library's inet_ntop(AF_INET, src, dst)
 * ipText = INET_NTOP(blob(16)) is C library's inet_ntop(AF_INET6, src, dst)
 * ipText is an IPv4 format for an IPv6 encoding of an IPv4 address
 */
static void
ntopFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  static unsigned char i4[] = {0x00,0x00,0x00,0x00
                              ,0x00,0x00,0x00,0x00
                              ,0x00,0x00,0xff,0xff};
  const unsigned char *b;
  char *r;
  int i;

  (void)argc;
  if ((b = sqlite3_value_blob(argv[0]))
   && ((i = sqlite3_value_bytes(argv[0])) == 4 || i == 16)
   && (r = sqlite3_malloc(40))) {
    if (i == 4)
      b = (const unsigned char *)inet_ntop(AF_INET, b, r, 40);
    else if (!memcmp(b, i4, sizeof(i4)))
      b = (const unsigned char *)inet_ntop(AF_INET, b + sizeof(i4), r, 40);
    else
      b = (const unsigned char *)inet_ntop(AF_INET6, b, r, 40);
    if (b) {
      r[39] = '\0'; /* strlen safety */
      sqlite3_result_text(context, r, strlen(r), sqlite3_free);
    } else
      sqlite3_free(r);
  }
}

/*
 * blob(4) = INET_PTON(ipText) is C library's inet_pton(AF_INET, src, dst)
 * blob(16) = INET_PTON(ipText) is C library's inet_pton(AF_INET6, src, dst)
 * blob(4) is returned for an IPv6 encoding of an IPv4 address
 */
static void
ptonFunc(
  sqlite3_context *context
 ,int argc
 ,sqlite3_value **argv
){
  static unsigned char i4[] = {0x00,0x00,0x00,0x00
                              ,0x00,0x00,0x00,0x00
                              ,0x00,0x00,0xff,0xff};
  const char *t;
  unsigned char *b;

  (void)argc;
  if ((t = (const char *)sqlite3_value_text(argv[0]))
   && (b = sqlite3_malloc(16))) {
    if (inet_pton(AF_INET, t, b) == 1)
      sqlite3_result_blob(context, b, 4, sqlite3_free);
    else if (inet_pton(AF_INET6, t, b) == 1) {
      if (memcmp(b, i4, sizeof(i4)))
        sqlite3_result_blob(context, b, 16, sqlite3_free);
      else {
        memmove(b, b + sizeof(i4), 4);
        sqlite3_result_blob(context, b, 4, sqlite3_free);
      }
    } else
      sqlite3_free(b);
  }
}

int
sqliteLocal(
  sqlite3 *db
 ,char **e
 ,const struct sqlite3_api_routines *a
){
  int rc;

  (void)e;
  SQLITE_EXTENSION_INIT2(a);
  if (!(rc = sqlite3_create_function(db, "instr", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, instrFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "instr", 3, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, instrFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "instr", 4, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, instrFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, blobFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_not", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, notFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_and", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)0, xxxFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_and", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)0, 0, xxxStep, xxxFinalize))
   && !(rc = sqlite3_create_function(db, "blob_or", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)1, xxxFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_or", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)1, 0, xxxStep, xxxFinalize))
   && !(rc = sqlite3_create_function(db, "blob_xor", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)2, xxxFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_xor", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)2, 0, xxxStep, xxxFinalize))
   && !(rc = sqlite3_create_function(db, "blob_shr", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)0, shFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_shl", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)1, shFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_clz", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)0, czFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "blob_ctz", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, (void*)1, czFunc, 0, 0))
   && !(rc =   sqlite3_create_module(db, "blob_bit", &bitMod, 0))
   && !(rc = sqlite3_create_function(db, "strtoll", 2, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, strtollFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "ether_ntoa", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, ntoaFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "ether_aton", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, atonFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "inet_ntop", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, ntopFunc, 0, 0))
   && !(rc = sqlite3_create_function(db, "inet_pton", 1, SQLITE_UTF8|SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS, 0, ptonFunc, 0, 0))
  )
    return rc;
  return rc;
}

void
shellInitProc(
  void
){
  sqlite3_initialize();
  sqlite3_auto_extension((void(*)(void))sqliteLocal);
}
