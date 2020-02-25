qjs-sqlite3
===========

## What is it? ##
**qjs-sqlite3** is a simple interface to sqlite3 from quickjs
<https://bellard.org/quickjs/>.

This has only been run on Linux x86_64 (Fedora 31).

See test.js for simple example and testing.

## How to use it? ##
Use new sqlite3_db("db") to get a handle to the open db.

```
import { sqlite3_db } from "./sqlite3.so";

db = new sqlite3_db("db);
db.close(); // close db
db.errmsg(); // last error message from sqlite3
st = db.prepare("sql"); // prepare statement
db.exec("sql"); // non-parameter sql
db.last_insert_rowid(); // return last insert rowid

st.clear_bindings(); // clear bindings
st.reset(); // reset statement
st.finalize(); // finalize (free) statement
st.step(); // step statement (returns "row", "done", "busy", null)
st.bind_parameter_count(); // bind parameter count
st.bind_parameter_name(n); // name for parameter n (1..)
st.bind_parameter_index(name); // index for parameter name
st.bind(n, value); // bind parameter n to value
st.column_count(); // result column count
st.column_name(n); // column name for n (0..)
st.column_text(n); // column n as text
st.column_value(n); // column n value

```

## Installation ##
Installing qjs-sqlite3 is easy.

```
$ ./BUILD
```

## Available imports ##
```
  import { sqlite3_db } from "./sqlite3.so";
```

## Changes ##

* Wed Feb 12 15:51:56 EST 2020
* initial release


## Limitations ##

* Small subset of sqlite3 API supported.
* Synchronous interface

## TODO ##

* Needs testing
