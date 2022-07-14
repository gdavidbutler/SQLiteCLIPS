## SQLiteCLIPS
SQLite virtual table for CLIPS template facts

See [SQLite](https://sqlite.org) and [CLIPS](https://clipsrules.net)

Synopsis: CREATE VIRTUAL TABLE "name" USING CLIPS("templateName");

* Columns are CLIPS' templates' "single" slots that allow SYMBOL, INTEGER, FLOAT and / or STRING types
* Column ROWID (fact index) can not be set on INSERT nor changed on UPDATE
* Fact duplicates are controlled by CLIPS' setting "set-fact-duplication"
* Otherwise use EXISTS

See example.c
