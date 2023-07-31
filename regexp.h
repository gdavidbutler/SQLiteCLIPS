/*
 * SQLiteRegexp - a SQLite regexp library for generic C
 * Copyright (C) 2023 G. David Butler <gdb@dbSystems.com>
 *
 * This file is part of SQLiteRegexp
 *
 * SQLiteRegexp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SQLiteRegexp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
** 2012-11-13
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** The code in this file implements a compact but reasonably
** efficient regular-expression matcher for posix extended regular
** expressions against UTF8 text.
**
** This file is an SQLite extension.  It registers a single function
** named "regexp(A,B)" where A is the regular expression and B is the
** string to be matched.  By registering this function, SQLite will also
** then implement the "B regexp A" operator.  Note that with the function
** the regular expression comes first, but with the operator it comes
** second.
**
**  The following regular expression syntax is supported:
**
**     X*      zero or more occurrences of X
**     X+      one or more occurrences of X
**     X?      zero or one occurrences of X
**     X{p,q}  between p and q occurrences of X
**     (X)     match X
**     X|Y     X or Y
**     ^X      X occurring at the beginning of the string
**     X$      X occurring at the end of the string
**     .       Match any single character
**     \c      Character c where c is one of \{}()[]|*+?.
**     \c      C-language escapes for c in afnrtv.  ex: \t or \n
**     \uXXXX  Where XXXX is exactly 4 hex digits, unicode value XXXX
**     \xXX    Where XX is exactly 2 hex digits, unicode value XX
**     [abc]   Any single character from the set abc
**     [^abc]  Any single character not in the set abc
**     [a-z]   Any single character in the range a-z
**     [^a-z]  Any single character not in the range a-z
**     \b      Word boundary
**     \w      Word character.  [A-Za-z0-9_]
**     \W      Non-word character
**     \d      Digit
**     \D      Non-digit
**     \s      Whitespace character
**     \S      Non-whitespace character
**
** A nondeterministic finite automaton (NFA) is used for matching, so the
** performance is bounded by O(N*M) where N is the size of the regular
** expression and M is the size of the input string.  The matcher never
** exhibits exponential behavior.  Note that the X{p,q} operator expands
** to p copies of X following by q-p copies of X? and that the size of the
** regular expression in the O(N*M) performance bound is computed after
** this expansion.
*/

typedef struct sqlite3re_compiled sqlite3re_compiled;

/* duplicate a compiled RE */
sqlite3re_compiled *sqlite3re_dup(sqlite3re_compiled *pRe);

/* pRe can be 0 */
void sqlite3re_free(sqlite3re_compiled *pRe);

/* return 0 on success, else an error message and sets *ppRe to 0 */
const char *sqlite3re_compile(sqlite3re_compiled **ppRe, const char *zIn, int noCase);

/* return -1 on alloc error, 0 for no match, 1 for match */
int sqlite3re_match(sqlite3re_compiled *pRe, const unsigned char *zIn, int nIn);
