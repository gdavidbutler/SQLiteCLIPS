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

int
main(
){
  extern int sqlite3_clips_init(sqlite3 *, Environment *);
  Environment *ev;
  sqlite3 *db;
  sqlite3_stmt *st;

  sqlite3_initialize();

  if (!(ev = CreateEnvironment())) {
    fprintf(stderr, "CreateEnvironment fail\n");
    return (-1);
  }
  SetConserveMemory(ev, 1);
  if (!LoadFromString(ev
  ,"(deftemplate MAIN::t1"
    "(slot s1 (type INTEGER))"       /* t1.s1 INTEGER NOT NULL */
    "(slot s2 (type SYMBOL STRING))" /* t1.s2 TEXT */
    "(slot s3)"                      /* t1.s3 */
   ")"
  ,SIZE_MAX)) {
    fprintf(stderr, "LoadFromString fail\n");
    return (-1);
  }
  Reset(ev);

  if (sqlite3_open(":memory:", &db)) {
    fprintf(stderr, "sqlite3_open fail\n");
    return (-1);
  }
  if (sqlite3_clips_init(db, ev)) {
    fprintf(stderr, "sqlite3_create_module fail\n");
    return (-1);
  }
  if (sqlite3_exec(db
  ,"CREATE VIRTUAL TABLE \"t1\" USING CLIPS(\"MAIN::t1\");"
  ,0,0,0)) {
    fprintf(stderr, "sqlite3_exec %s\n", sqlite3_errmsg(db));
    return (-1);
  }
  if (sqlite3_prepare(db, "SELECT ROWID,\"s1\",\"s2\",\"s3\" FROM \"t1\"", -1, &st, 0)) {
    fprintf(stderr, "sqlite3_prepare %s\n", sqlite3_errmsg(db));
    return (-1);
  }
  if (sqlite3_exec(db, "INSERT INTO \"t1\"(\"s1\",\"s2\")VALUES(1,CAST('a' AS BLOB)),(2,'b');", 0,0,0)) {
    fprintf(stderr, "sqlite3_exec %s\n", sqlite3_errmsg(db));
    return (-1);
  }
  while (sqlite3_step(st) == SQLITE_ROW)
    printf("%lld %s %s %s\n", sqlite3_column_int64(st, 0)
    ,sqlite3_column_type(st, 1) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 1)
    ,sqlite3_column_type(st, 2) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 2)
    ,sqlite3_column_type(st, 3) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 3)
    );
  sqlite3_reset(st);
  putchar('\n');
  if (sqlite3_exec(db, "UPDATE \"t1\" SET \"s3\"=1 WHERE \"s2\"='b';", 0,0,0)) {
    fprintf(stderr, "sqlite3_exec %s\n", sqlite3_errmsg(db));
    return (-1);
  }
  while (sqlite3_step(st) == SQLITE_ROW)
    printf("%lld %s %s %s\n", sqlite3_column_int64(st, 0)
    ,sqlite3_column_type(st, 1) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 1)
    ,sqlite3_column_type(st, 2) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 2)
    ,sqlite3_column_type(st, 3) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 3)
    );
  sqlite3_reset(st);
  putchar('\n');
  if (sqlite3_exec(db, "DELETE FROM \"t1\" WHERE \"s3\" IS NULL;", 0,0,0)) {
    fprintf(stderr, "sqlite3_exec %s\n", sqlite3_errmsg(db));
    return (-1);
  }
  while (sqlite3_step(st) == SQLITE_ROW)
    printf("%lld %s %s %s\n", sqlite3_column_int64(st, 0)
    ,sqlite3_column_type(st, 1) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 1)
    ,sqlite3_column_type(st, 2) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 2)
    ,sqlite3_column_type(st, 3) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 3)
    );
  sqlite3_reset(st);
  putchar('\n');
  if (sqlite3_exec(db, "UPDATE \"t1\" SET \"s3\"=NULL WHERE \"s1\"=2;", 0,0,0)) {
    fprintf(stderr, "sqlite3_exec %s\n", sqlite3_errmsg(db));
    return (-1);
  }
  while (sqlite3_step(st) == SQLITE_ROW)
    printf("%lld %s %s %s\n", sqlite3_column_int64(st, 0)
    ,sqlite3_column_type(st, 1) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 1)
    ,sqlite3_column_type(st, 2) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 2)
    ,sqlite3_column_type(st, 3) == SQLITE_NULL ? "NULL" : (const char *)sqlite3_column_text(st, 3)
    );
  sqlite3_finalize(st);

  if (sqlite3_close(db)) {
    fprintf(stderr, "sqlite3_close fail\n");
    return (-1);
  }
  if (!DestroyEnvironment(ev)) {
    fprintf(stderr, "DestroyEnvironment fail\n");
    return (-1);
  }
  return (0);
}
