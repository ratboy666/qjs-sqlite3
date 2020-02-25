/**********************************************************************
 *                                                                    *
 * sqlite3.c                                                          *
 *                                                                    *
 * For sqlite3 function docs, see:                                    *
 * https://www.sqlite.org/c3ref/funclist.html                         *
 *                                                                    *
 **********************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <sqlite3.h>
#include <quickjs/quickjs.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static void debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static void fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(2);
}

typedef struct {
    char *name;
    sqlite3 *db;
} sqlite_db;

static JSClassID db_class_id;

typedef struct {
    sqlite3_stmt *st;
} sqlite_st;

static JSClassID st_class_id;

static void st_finalizer(JSRuntime *rt, JSValue val) {
    sqlite_st *s = JS_GetOpaque(val, st_class_id);
    if (s) {
	if (s->st)
	    sqlite3_finalize(s->st);
	js_free_rt(rt, s);
    }
}

static JSValue new_st(JSContext *ctx, sqlite3_stmt *st) {
    sqlite_st *s;
    JSValue obj;
    obj = JS_NewObjectClass(ctx, st_class_id);
    if (JS_IsException(obj))
	return obj;
    s = js_mallocz(ctx, sizeof(*s));
    if (!s) {
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
    }
    s->st = st;
    JS_SetOpaque(obj, s);
    return obj;
}

static void db_finalizer(JSRuntime *rt, JSValue val) {
    sqlite_db *s = JS_GetOpaque(val, db_class_id);
    if (s) {
	if (s->db)
	    sqlite3_close(s->db);
	js_free_rt(rt, s);
    }
}

static JSValue db_ctor(JSContext *ctx, JSValueConst new_target,
                       int argc, JSValueConst *argv) {
    sqlite_db *s;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    int r;
    const char *name;
    
    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;
    name = JS_ToCString(ctx, argv[0]);
    if (!name)
	goto fail;
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto fail;
    obj = JS_NewObjectProtoClass(ctx, proto, db_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto fail;
    JS_SetOpaque(obj, s);
    r = sqlite3_open(name, &(s->db));
    if (r != SQLITE_OK)
	goto fail;
    JS_FreeCString(ctx, name);
    return obj;
 fail:
    if (name)
	JS_FreeCString(ctx, name);
    js_free(ctx, s);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue db_errmsg(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    sqlite_db *s = JS_GetOpaque2(ctx, this_val, db_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (s->db)
	return JS_NewString(ctx, sqlite3_errmsg(s->db));
    else
	return JS_EXCEPTION;
}

static JSValue db_last_insert_rowid(JSContext *ctx,
                                    JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    sqlite_db *s = JS_GetOpaque2(ctx, this_val, db_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (s->db)
	return JS_NewInt32(ctx, sqlite3_last_insert_rowid(s->db));
    return JS_EXCEPTION;
}

static JSValue db_prepare(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    sqlite_db *s = JS_GetOpaque2(ctx, this_val, db_class_id);
    const char *sql;
    sqlite3_stmt *t;
    int r;
    if (!s)
        return JS_EXCEPTION;
    if (!s->db)
	return JS_EXCEPTION;
    sql = JS_ToCString(ctx, argv[0]);
    if (!sql)
	return JS_EXCEPTION;
    r = sqlite3_prepare_v2(s->db, sql, strlen(sql) + 1, &t, NULL);
    JS_FreeCString(ctx, sql);
    if (r != SQLITE_OK)
	return JS_NULL;
    if (t == NULL)
	return JS_NULL;
    return new_st(ctx, t);
}

static JSValue db_exec(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
    sqlite_db *s = JS_GetOpaque2(ctx, this_val, db_class_id);
    int r;
    const char *sql;
    if (!s)
        return JS_EXCEPTION;
    if (s->db == NULL)
	return JS_EXCEPTION;
    sql = JS_ToCString(ctx, argv[0]);
    if (!sql)
	return JS_EXCEPTION;
    r = sqlite3_exec(s->db, sql, NULL, NULL, NULL);
    JS_FreeCString(ctx, sql);
    if (r == SQLITE_OK)
	return JS_TRUE;
    return JS_FALSE;
}

static JSValue db_close(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    sqlite_db *s = JS_GetOpaque2(ctx, this_val, db_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (s->db) {
	sqlite3_close(s->db);
	s->db = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue st_finalize(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    sqlite3_stmt *st;
    if (!s)
        return JS_EXCEPTION;
    st = s->st;
    s->st = NULL;
    if (!st)
	return JS_TRUE;
    if (sqlite3_finalize(s->st) == SQLITE_OK)
	return JS_TRUE;
    return JS_FALSE;
}

static JSValue st_reset(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (sqlite3_reset(s->st) == SQLITE_OK)
	return JS_TRUE;
    return JS_FALSE;
}

static JSValue st_clear_bindings(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (sqlite3_clear_bindings(s->st) == SQLITE_OK)
	return JS_TRUE;
    return JS_FALSE;
}

static JSValue st_bind_parameter_count(JSContext *ctx,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    return JS_NewInt32(ctx, sqlite3_bind_parameter_count(s->st));
}

static JSValue st_column_text(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    int n;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &n, argv[0]))
	return JS_EXCEPTION;
    return JS_NewString(ctx, sqlite3_column_text(s->st, n));
}

static JSValue st_column_name(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    int n;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &n, argv[0]))
	return JS_EXCEPTION;
    return JS_NewString(ctx, sqlite3_column_name(s->st, n));
}

static JSValue st_column_count(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    return JS_NewInt32(ctx, sqlite3_column_count(s->st));
}

static JSValue st_bind_parameter_name(JSContext *ctx,
                                      JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    int n;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &n, argv[0]))
	return JS_EXCEPTION;
    return JS_NewString(ctx, sqlite3_bind_parameter_name(s->st, n));
}

static JSValue st_bind_parameter_index(JSContext *ctx,
                                       JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    const char *name;
    int n;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    name = JS_ToCString(ctx, argv[0]);
    if (!name)
	return JS_NULL;
    n = sqlite3_bind_parameter_index(s->st, name);
    JS_FreeCString(ctx, name);
    return JS_NewInt32(ctx, n);
}

static JSValue st_step(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    int r;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    r = sqlite3_step(s->st);
    switch (r) {
    case SQLITE_ROW:
	return JS_NewString(ctx, "row");
    case SQLITE_DONE:
	return JS_NewString(ctx, "done");
    case SQLITE_BUSY:
	return JS_NewString(ctx, "busy");
    default:
	return JS_NULL;
    }
}

static JSValue st_column_value(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    int n;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &n, argv[0]))
	return JS_EXCEPTION;
    switch (sqlite3_column_type(s->st, n)) {
    case SQLITE_INTEGER:
	return JS_NewInt64(ctx, sqlite3_column_int64(s->st, n));
    case SQLITE_FLOAT:
	return JS_NewFloat64(ctx, sqlite3_column_double(s->st, n));
    case SQLITE_BLOB:
	return JS_NewArrayBufferCopy(ctx,
	    (uint8_t *)sqlite3_column_blob(s->st, n),
	    (size_t)sqlite3_column_bytes(s->st, n));
    case SQLITE_NULL:
	return JS_NULL;
    case SQLITE3_TEXT:
    default:
	return JS_NewString(ctx, sqlite3_column_text(s->st, n));
    }
    return JS_EXCEPTION;
}

static inline JS_BOOL JS_IsInteger(JSValueConst v) {
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_INT || tag == JS_TAG_BIG_INT;
}

static JSValue st_bind(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
    sqlite_st *s = JS_GetOpaque2(ctx, this_val, st_class_id);
    JSValueConst a;
    int n, r;
    if (!s)
        return JS_EXCEPTION;
    if (!s->st)
	return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &n, argv[0]))
	return JS_EXCEPTION;
    a = argv[1];
    if (JS_IsNull(a)) {
	r = sqlite3_bind_null(s->st, n);
    } else if (JS_IsBool(a)) {
	int b = JS_ToBool(ctx, a);
	r = sqlite3_bind_int(s->st, n, b);
    } else if (JS_IsInteger(a)) {
	int64_t i64;
	int i;
	double d;
	if (!JS_ToInt32(ctx, &i, a)) {
	    r = sqlite3_bind_int(s->st, n, i);
	} if (!JS_ToInt64(ctx, &i64, a)) {
	    r = sqlite3_bind_int64(s->st, n, i64);
	} else if (JS_ToFloat64(ctx, &d, a)) {
	    return JS_EXCEPTION;
	} else {
	    r = sqlite3_bind_double(s->st, n, d);
	}
    } else if (JS_IsNumber(a)) {
	double d;
	if (JS_ToFloat64(ctx, &d, a))
	    return JS_EXCEPTION;
	r = sqlite3_bind_double(s->st, n, d);
    } else if (JS_IsString(a)) {
	const char *t;
	t = JS_ToCString(ctx, a);
	r = sqlite3_bind_text(s->st, n, t, strlen(t), SQLITE_TRANSIENT);
	JS_FreeCString(ctx, t);
    } else {
	uint8_t *buf;
	size_t size;
	buf = JS_GetArrayBuffer(ctx, &size, a);
	if (!buf)
	    return JS_EXCEPTION;
	r = sqlite3_bind_blob(s->st, n, buf, size, SQLITE_TRANSIENT);
    }
    return JS_NewInt32(ctx, r);
}

static JSClassDef db_class = {
    "sqlite3_db",
    .finalizer = db_finalizer,
}; 

static const JSCFunctionListEntry db_proto_funcs[] = {
    JS_CFUNC_DEF("exec", 1, db_exec),
    JS_CFUNC_DEF("prepare", 0, db_prepare),
    JS_CFUNC_DEF("last_insert_rowid", 0, db_last_insert_rowid),
    JS_CFUNC_DEF("errmsg", 0, db_errmsg),
    JS_CFUNC_DEF("close", 0, db_close),
};

static const JSCFunctionListEntry st_proto_funcs[] = {
    JS_CFUNC_DEF("column_value", 1, st_column_value),
    JS_CFUNC_DEF("column_text", 1, st_column_text),
    JS_CFUNC_DEF("column_name", 1, st_column_name),
    JS_CFUNC_DEF("column_count", 0, st_column_count),
    JS_CFUNC_DEF("bind", 2, st_bind),
    JS_CFUNC_DEF("bind_parameter_index", 1, st_bind_parameter_index),
    JS_CFUNC_DEF("bind_parameter_name", 1, st_bind_parameter_name),
    JS_CFUNC_DEF("bind_parameter_count", 0, st_bind_parameter_count),
    JS_CFUNC_DEF("step", 0, st_step),
    JS_CFUNC_DEF("finalize", 0, st_finalize),
    JS_CFUNC_DEF("reset", 0, st_reset),
    JS_CFUNC_DEF("clear_bindings", 0, st_clear_bindings),
};

static JSClassDef st_class = {
    "sqlite3_st",
    .finalizer = st_finalizer,
};

static int st_init(JSContext *ctx, JSModuleDef *m) {
    JSValue proto;
    JS_NewClassID(&st_class_id);
    JS_NewClass(JS_GetRuntime(ctx), st_class_id, &st_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, st_proto_funcs,
                               countof(st_proto_funcs));
    JS_SetClassProto(ctx, st_class_id, proto);
    return 0;
}

static int db_init(JSContext *ctx, JSModuleDef *m) {
    JSValue proto, class;
    st_init(ctx, m);
    JS_NewClassID(&db_class_id);
    JS_NewClass(JS_GetRuntime(ctx), db_class_id, &db_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, db_proto_funcs,
                               countof(db_proto_funcs));
    JS_SetClassProto(ctx, db_class_id, proto);
    class = JS_NewCFunction2(ctx, db_ctor, "sqlite3_db", 2,
                             JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, class, proto);
    JS_SetModuleExport(ctx, m, "sqlite3_db", class);
    return 0;
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_sqlite3
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name) {
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, db_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "sqlite3_db");
    return m;
}

/* ce: .mc; */
