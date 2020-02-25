/* test.js
 */

import * as std from "std";
import * as os from "os";
import { sqlite3_db } from "./sqlite3.so";

console.log("sqlite3 test");

var db = new sqlite3_db("testdb");

if (!db.exec("create table t (a, b, c);"))
  console.log(db.errmsg());

var st = db.prepare("insert into t (a, b, c) values (1, 2, 3);");
console.log("bind parameter count = " + st.bind_parameter_count());
var s;
s = st.step();
console.log("step result = " + s); // "row" "done" "busy" null
st.reset();
st.clear_bindings();
st.finalize();
console.log("last insert rowid = " + db.last_insert_rowid());

st = db.prepare("insert into t (a, b, c) values (?1, ?2, ?3);");
console.log("bind parameter count = " + st.bind_parameter_count());
/* distinctly 1 based */
console.log(st.bind_parameter_name(1));
console.log(st.bind_parameter_index("?2"));
console.log(st.column_count());
st.bind(1, null);
st.bind(2, null);
st.bind(3, null);
st.finalize();

console.log("select");
st = db.prepare("select * from t;");
st.step();
console.log(st.column_count());
var i;
console.log("begin column names...");
/* zero based - and see above bind parameter indices */
for (i = 0; i < st.column_count(); ++i) {
  console.log(st.column_name(i));
}
console.log("... end column names");
st.reset();
while ((s = st.step()) == "row") {
  console.log("row", st.column_value(0), st.column_value(1),
    st.column_value(2));
}
console.log(s);
st.finalize();

db.close();
