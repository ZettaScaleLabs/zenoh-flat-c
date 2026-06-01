//
// zenoh-c-compatible types + functions, layered over zenoh-flat-c (`z_flat_*`).
//
// Header-only: `z_owned_X_t` wraps the flat heap handle `z_flat_X_t*`, a loaned
// type IS the flat handle, a moved type is a thin transfer wrapper. Closures are
// bridged from zenoh-c's single {call,drop} form to flat's (callback, on_close)
// pair. Errors map to `z_result_t`.
//
#ifndef ZENOH_COMPAT_COMMONS_H
#define ZENOH_COMPAT_COMMONS_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zenoh_constants.h"
#include "zenoh_flat.h"

// ── error mapping ──────────────────────────────────────────────────────────
// Any flat error (a non-NULL `message`) becomes a generic failure code; the
// message is released. Success is `Z_OK`.
static inline z_result_t _z_map(z_flat_error_t e) {
    if (e.message) {
        z_flat_free(e.message);
        return Z_EGENERIC;
    }
    return Z_OK;
}

// ── owned / loaned / moved handle families ─────────────────────────────────
// `zc` is the zenoh-c short name, `flat` the zenoh-flat base (they differ only
// for bytes → zbytes).
#define _Z_HANDLE(zc, flat)                                                                            \
    typedef struct z_owned_##zc##_t {                                                                  \
        z_flat_##flat##_t* _0;                                                                          \
    } z_owned_##zc##_t;                                                                                 \
    typedef z_flat_##flat##_t z_loaned_##zc##_t;                                                        \
    typedef struct z_moved_##zc##_t {                                                                   \
        z_owned_##zc##_t _this;                                                                         \
    } z_moved_##zc##_t;                                                                                 \
    static inline z_moved_##zc##_t* z_##zc##_move(z_owned_##zc##_t* x) { return (z_moved_##zc##_t*)x; } \
    static inline const z_loaned_##zc##_t* z_##zc##_loan(const z_owned_##zc##_t* x) {                   \
        return (const z_loaned_##zc##_t*)x->_0;                                                         \
    }                                                                                                  \
    static inline z_loaned_##zc##_t* z_##zc##_loan_mut(z_owned_##zc##_t* x) {                           \
        return (z_loaned_##zc##_t*)x->_0;                                                               \
    }                                                                                                  \
    static inline bool z_internal_##zc##_check(const z_owned_##zc##_t* x) { return x->_0 != NULL; }     \
    static inline void z_internal_##zc##_null(z_owned_##zc##_t* x) { x->_0 = NULL; }                    \
    static inline void z_##zc##_drop(z_moved_##zc##_t* m) {                                             \
        if (m && m->_this._0) {                                                                        \
            z_flat_##flat##_drop(m->_this._0);                                                          \
            m->_this._0 = NULL;                                                                         \
        }                                                                                              \
    }

_Z_HANDLE(session, session)
_Z_HANDLE(config, config)
_Z_HANDLE(keyexpr, keyexpr)
_Z_HANDLE(bytes, zbytes)
_Z_HANDLE(encoding, encoding)
_Z_HANDLE(subscriber, subscriber)
_Z_HANDLE(publisher, publisher)
_Z_HANDLE(queryable, queryable)
_Z_HANDLE(querier, querier)
_Z_HANDLE(sample, sample)
_Z_HANDLE(reply, reply)
_Z_HANDLE(query, query)
_Z_HANDLE(hello, hello)
_Z_HANDLE(scout, scout)
_Z_HANDLE(liveliness_token, liveliness_token)

// ── strings ────────────────────────────────────────────────────────────────
typedef struct z_loaned_string_t {
    const char* _val;
    size_t _len;
} z_loaned_string_t;
typedef struct z_owned_string_t {
    char* _val;  // owns a flat-malloc'd buffer (freed by z_flat_free)
    size_t _len;
} z_owned_string_t;
typedef struct z_moved_string_t {
    z_owned_string_t _this;
} z_moved_string_t;
typedef struct z_view_string_t {
    const char* _val;
    size_t _len;
} z_view_string_t;

static inline const z_loaned_string_t* z_string_loan(const z_owned_string_t* s) { return (const z_loaned_string_t*)s; }
static inline const z_loaned_string_t* z_view_string_loan(const z_view_string_t* s) {
    return (const z_loaned_string_t*)s;
}
static inline z_moved_string_t* z_string_move(z_owned_string_t* s) { return (z_moved_string_t*)s; }
static inline const char* z_string_data(const z_loaned_string_t* s) { return s->_val; }
static inline size_t z_string_len(const z_loaned_string_t* s) { return s->_len; }
static inline void z_string_drop(z_moved_string_t* m) {
    if (m && m->_this._val) {
        z_flat_free(m->_this._val);
        m->_this._val = NULL;
    }
    if (m) m->_this._len = 0;
}

// ── string arrays ──────────────────────────────────────────────────────────
typedef struct z_owned_string_array_t {
    z_loaned_string_t* _items;  // each ._val is a flat-malloc'd NUL string
    size_t _len;
} z_owned_string_array_t;
typedef struct z_loaned_string_array_t {
    const z_loaned_string_t* _items;
    size_t _len;
} z_loaned_string_array_t;
typedef struct z_moved_string_array_t {
    z_owned_string_array_t _this;
} z_moved_string_array_t;

static inline const z_loaned_string_array_t* z_string_array_loan(const z_owned_string_array_t* a) {
    return (const z_loaned_string_array_t*)a;
}
static inline z_moved_string_array_t* z_string_array_move(z_owned_string_array_t* a) {
    return (z_moved_string_array_t*)a;
}
static inline size_t z_string_array_len(const z_loaned_string_array_t* a) { return a->_len; }
static inline const z_loaned_string_t* z_string_array_get(const z_loaned_string_array_t* a, size_t i) {
    return &a->_items[i];
}
static inline void z_string_array_drop(z_moved_string_array_t* m) {
    if (!m) return;
    for (size_t i = 0; i < m->_this._len; i++) z_flat_free((void*)m->_this._items[i]._val);
    free(m->_this._items);
    m->_this._items = NULL;
    m->_this._len = 0;
}

// ── view keyexpr (owns a flat keyexpr; not dropped by callers — see README) ──
typedef struct z_view_keyexpr_t {
    z_flat_keyexpr_t* _0;
} z_view_keyexpr_t;
static inline z_result_t z_view_keyexpr_from_str(z_view_keyexpr_t* v, const char* s) {
    z_flat_error_t e = {0};
    v->_0 = z_flat_keyexpr_try_from(s, &e);
    return _z_map(e);
}
static inline z_result_t z_view_keyexpr_from_str_autocanonize(z_view_keyexpr_t* v, char* s) {
    z_flat_error_t e = {0};
    v->_0 = z_flat_keyexpr_autocanonize(s, &e);
    if (v->_0) {
        // zenoh-c canonicalizes the input buffer in place (canonical ≤ input).
        char* canon = z_flat_keyexpr_to_string(v->_0);
        if (canon) {
            size_t n = strlen(canon);
            memcpy(s, canon, n);
            s[n] = '\0';
            z_flat_free(canon);
        }
    }
    return _z_map(e);
}
static inline z_result_t z_view_keyexpr_from_substr_autocanonize(z_view_keyexpr_t* v, char* start, size_t* len) {
    char* tmp = (char*)malloc(*len + 1);
    memcpy(tmp, start, *len);
    tmp[*len] = '\0';
    z_flat_error_t e = {0};
    v->_0 = z_flat_keyexpr_autocanonize(tmp, &e);
    if (v->_0) {
        // Reflect the (possibly shorter) canonical form back into the caller's
        // buffer + length, matching zenoh-c's in-place substr autocanonize.
        char* canon = z_flat_keyexpr_to_string(v->_0);
        if (canon) {
            size_t n = strlen(canon);
            memcpy(start, canon, n);
            *len = n;
            z_flat_free(canon);
        }
    }
    free(tmp);
    return _z_map(e);
}
static inline bool z_view_keyexpr_is_empty(const z_view_keyexpr_t* v) { return v->_0 == NULL; }
static inline const z_loaned_keyexpr_t* z_view_keyexpr_loan(const z_view_keyexpr_t* v) {
    return (const z_loaned_keyexpr_t*)v->_0;
}

// In-place canonization (zenoh-c API), backed by flat's autocanonize: the
// canonical form is never longer than the input, so it fits the caller's buffer.
static inline z_result_t z_keyexpr_canonize(char* start, size_t* len) {
    char* tmp = (char*)malloc(*len + 1);
    memcpy(tmp, start, *len);
    tmp[*len] = '\0';
    z_flat_error_t e = {0};
    z_flat_keyexpr_t* ke = z_flat_keyexpr_autocanonize(tmp, &e);
    free(tmp);
    if (!ke) return _z_map(e);
    char* canon = z_flat_keyexpr_to_string(ke);
    z_flat_keyexpr_drop(ke);
    if (!canon) return Z_EGENERIC;
    size_t n = strlen(canon);
    memcpy(start, canon, n);
    start[n] = '\0';  // zenoh-c NUL-terminates at the (shorter) canonical length
    *len = n;
    z_flat_free(canon);
    return Z_OK;
}
static inline z_result_t z_keyexpr_canonize_null_terminated(char* start) {
    size_t len = strlen(start);
    z_result_t r = z_keyexpr_canonize(start, &len);
    if (r == Z_OK) start[len] = '\0';
    return r;
}

// ── id ──────────────────────────────────────────────────────────────────────
typedef struct z_id_t {
    uint8_t id[16];
} z_id_t;
static inline void _z_fill_id(z_id_t* out, z_flat_zenoh_id_t* h) {
    memset(out->id, 0, 16);
    if (h) {
        uintptr_t n = 0;
        uint8_t* b = z_flat_zenoh_id_to_bytes(h, &n);
        if (b) {
            if (n > 16) n = 16;
            memcpy(out->id, b, n);
            z_flat_free(b);
        }
        z_flat_zenoh_id_drop(h);
    }
}
static inline void z_id_to_string(const z_id_t* id, z_owned_string_t* out) {
    char* s = (char*)malloc(33);
    for (int i = 0; i < 16; i++) snprintf(s + 2 * i, 3, "%02x", id->id[i]);
    s[32] = '\0';
    out->_val = s;
    out->_len = 32;
}

// ── closures ────────────────────────────────────────────────────────────────
#define _Z_CLOSURE(zc, ltype)                                                       \
    typedef struct z_owned_closure_##zc##_t {                                        \
        void* context;                                                              \
        void (*call)(ltype*, void*);                                                \
        void (*drop)(void*);                                                        \
    } z_owned_closure_##zc##_t;                                                      \
    typedef struct z_moved_closure_##zc##_t {                                        \
        z_owned_closure_##zc##_t _this;                                             \
    } z_moved_closure_##zc##_t;                                                      \
    static inline z_moved_closure_##zc##_t* z_closure_##zc##_move(z_owned_closure_##zc##_t* c) {       \
        return (z_moved_closure_##zc##_t*)c;                                         \
    }                                                                               \
    static inline void z_closure_##zc(z_owned_closure_##zc##_t* c, void (*call)(ltype*, void*),        \
                                      void (*drop)(void*), void* ctx) {              \
        c->context = ctx;                                                           \
        c->call = call;                                                            \
        c->drop = drop;                                                            \
    }

_Z_CLOSURE(sample, z_loaned_sample_t)
_Z_CLOSURE(query, z_loaned_query_t)
_Z_CLOSURE(reply, z_loaned_reply_t)
_Z_CLOSURE(hello, z_loaned_hello_t)

// zid closure delivers a by-value id (const pointer)
typedef struct z_owned_closure_zid_t {
    void* context;
    void (*call)(const z_id_t*, void*);
    void (*drop)(void*);
} z_owned_closure_zid_t;
typedef struct z_moved_closure_zid_t {
    z_owned_closure_zid_t _this;
} z_moved_closure_zid_t;
static inline z_moved_closure_zid_t* z_closure_zid_move(z_owned_closure_zid_t* c) {
    return (z_moved_closure_zid_t*)c;
}
static inline void z_closure_zid(z_owned_closure_zid_t* c, void (*call)(const z_id_t*, void*), void (*drop)(void*),
                                 void* ctx) {
    c->context = ctx;
    c->call = call;
    c->drop = drop;
}

// ── closure bridge: zenoh-c {context,call,drop} → flat (callback, on_close) ──
// Allocated on the heap, shared by the flat sample/query/... closure and the
// flat on_close closure. The per-message trampoline hands the zenoh-c callback a
// *loaned* handle (cast) then drops the *owned* flat handle it received. The
// on_close trampoline runs the zenoh-c drop once and frees the bridge.
typedef struct _z_bridge_t {
    void* context;
    void* call;  // typed fn ptr, cast per trampoline
    void (*drop)(void*);
} _z_bridge_t;

static inline _z_bridge_t* _z_bridge_new(void* ctx, void* call, void (*drop)(void*)) {
    _z_bridge_t* b = (_z_bridge_t*)malloc(sizeof(_z_bridge_t));
    b->context = ctx;
    b->call = call;
    b->drop = drop;
    return b;
}
static inline void _z_bridge_close(void* p) {
    _z_bridge_t* b = (_z_bridge_t*)p;
    if (b->drop) b->drop(b->context);
    free(b);
}
static inline void _z_call_sample(z_flat_sample_t* s, void* p) {
    _z_bridge_t* b = (_z_bridge_t*)p;
    ((void (*)(z_loaned_sample_t*, void*))b->call)((z_loaned_sample_t*)s, b->context);
    z_flat_sample_drop(s);
}
static inline void _z_call_query(z_flat_query_t* q, void* p) {
    _z_bridge_t* b = (_z_bridge_t*)p;
    ((void (*)(z_loaned_query_t*, void*))b->call)((z_loaned_query_t*)q, b->context);
    z_flat_query_drop(q);
}
static inline void _z_call_hello(z_flat_hello_t* h, void* p) {
    _z_bridge_t* b = (_z_bridge_t*)p;
    ((void (*)(z_loaned_hello_t*, void*))b->call)((z_loaned_hello_t*)h, b->context);
    z_flat_hello_drop(h);
}
static inline void _z_call_reply(z_flat_reply_t* r, void* p) {
    _z_bridge_t* b = (_z_bridge_t*)p;
    ((void (*)(z_loaned_reply_t*, void*))b->call)((z_loaned_reply_t*)r, b->context);
    z_flat_reply_drop(r);
}

// ── logging / runtime ────────────────────────────────────────────────────────
static inline void zc_init_log_from_env_or(const char* fallback) { z_flat_init_zenoh_logs_from_env_or(fallback); }
static inline void zc_try_init_log_from_env(void) { z_flat_try_init_zenoh_logs_from_env(); }
// zenoh-flat manages its async runtime internally and has no explicit shutdown;
// kept as a no-op for API compatibility.
static inline void zc_stop_z_runtime(void) {}

// ── config ────────────────────────────────────────────────────────────────────
typedef struct z_open_options_t {
    uint8_t _dummy;
} z_open_options_t;
static inline void z_open_options_default(z_open_options_t* o) { o->_dummy = 0; }

static inline z_result_t z_config_default(z_owned_config_t* c) {
    c->_0 = z_flat_config_default();
    return c->_0 ? Z_OK : Z_EGENERIC;
}
static inline z_result_t zc_config_from_file(z_owned_config_t* c, const char* path) {
    z_flat_error_t e = {0};
    c->_0 = z_flat_config_from_file(path, &e);
    return _z_map(e);
}
static inline z_result_t zc_config_from_str(z_owned_config_t* c, const char* s) {
    z_flat_error_t e = {0};
    c->_0 = z_flat_config_from_json5(s, &e);
    return _z_map(e);
}
static inline z_result_t zc_config_insert_json5(z_loaned_config_t* c, const char* key, const char* value) {
    z_flat_error_t e = {0};
    z_flat_config_insert_json5((z_flat_config_t*)c, key, value, &e);
    return _z_map(e);
}

// ── session ───────────────────────────────────────────────────────────────────
static inline z_result_t z_open(z_owned_session_t* s, z_moved_config_t* cfg, const z_open_options_t* opts) {
    (void)opts;
    z_flat_error_t e = {0};
    s->_0 = z_flat_open(cfg->_this._0, &e);
    cfg->_this._0 = NULL;  // consumed
    return _z_map(e);
}

// zenoh-flat has no explicit session close — the real close happens when the
// session handle is dropped. `z_close` is a no-op kept for API compatibility
// (the simple path; the concurrent-close branch is ZENOH_FLAT_UNSTABLE_API-gated).
typedef struct z_close_options_t {
    uint8_t _dummy;
} z_close_options_t;
static inline void z_close_options_default(z_close_options_t* o) { o->_dummy = 0; }
static inline z_result_t z_close(z_loaned_session_t* s, z_close_options_t* opts) {
    (void)s;
    (void)opts;
    return Z_OK;
}

// Declare a key expression on the session (gets a session-local id). Backed by
// flat's z_flat_session_declare_keyexpr (takes the keyexpr string).
static inline z_result_t z_declare_keyexpr(const z_loaned_session_t* s, z_owned_keyexpr_t* out,
                                           const z_loaned_keyexpr_t* ke) {
    char* str = z_flat_keyexpr_to_string((const z_flat_keyexpr_t*)ke);
    if (!str) {
        out->_0 = NULL;
        return Z_EGENERIC;
    }
    z_flat_error_t e = {0};
    out->_0 = z_flat_session_declare_keyexpr((const z_flat_session_t*)s, str, &e);
    z_flat_free(str);
    return _z_map(e);
}
static inline z_result_t z_undeclare_keyexpr(const z_loaned_session_t* s, z_moved_keyexpr_t* m) {
    z_flat_error_t e = {0};
    if (m->_this._0) {
        z_flat_session_undeclare_keyexpr((const z_flat_session_t*)s, m->_this._0, &e);
        m->_this._0 = NULL;  // consumed
    }
    return _z_map(e);
}

// ── keyexpr ─────────────────────────────────────────────────────────────────
static inline z_result_t z_keyexpr_from_str(z_owned_keyexpr_t* k, const char* s) {
    z_flat_error_t e = {0};
    k->_0 = z_flat_keyexpr_try_from(s, &e);
    return _z_map(e);
}
static inline void z_keyexpr_as_view_string(const z_loaned_keyexpr_t* ke, z_view_string_t* out) {
    char* s = z_flat_keyexpr_to_string((const z_flat_keyexpr_t*)ke);  // owned; view leaks (see README)
    out->_val = s;
    out->_len = s ? strlen(s) : 0;
}
static inline bool z_keyexpr_intersects(const z_loaned_keyexpr_t* a, const z_loaned_keyexpr_t* b) {
    return z_flat_keyexpr_intersects((const z_flat_keyexpr_t*)a, (const z_flat_keyexpr_t*)b);
}
static inline bool z_keyexpr_includes(const z_loaned_keyexpr_t* a, const z_loaned_keyexpr_t* b) {
    return z_flat_keyexpr_includes((const z_flat_keyexpr_t*)a, (const z_flat_keyexpr_t*)b);
}
static inline bool z_keyexpr_equals(const z_loaned_keyexpr_t* a, const z_loaned_keyexpr_t* b) {
    // Mutual inclusion ⇔ equality. (Avoids the unstable `relation_to`/`Equals`.)
    return z_flat_keyexpr_includes((const z_flat_keyexpr_t*)a, (const z_flat_keyexpr_t*)b) &&
           z_flat_keyexpr_includes((const z_flat_keyexpr_t*)b, (const z_flat_keyexpr_t*)a);
}

// ── bytes / payload ────────────────────────────────────────────────────────────
static inline z_result_t z_bytes_copy_from_str(z_owned_bytes_t* b, const char* s) {
    b->_0 = z_flat_zbytes_from_slice((const uint8_t*)s, strlen(s));
    return b->_0 ? Z_OK : Z_EGENERIC;
}
static inline z_result_t z_bytes_from_static_str(z_owned_bytes_t* b, const char* s) {
    return z_bytes_copy_from_str(b, s);
}
static inline size_t z_bytes_len(const z_loaned_bytes_t* b) {
    uintptr_t n = 0;
    uint8_t* p = z_flat_zbytes_to_bytes((const z_flat_zbytes_t*)b, &n);
    if (p) z_flat_free(p);
    return n;
}
static inline z_result_t z_bytes_to_string(const z_loaned_bytes_t* b, z_owned_string_t* out) {
    uintptr_t n = 0;
    uint8_t* p = z_flat_zbytes_to_bytes((const z_flat_zbytes_t*)b, &n);
    out->_val = (char*)p;  // flat-malloc'd; freed by z_string_drop
    out->_len = n;
    return Z_OK;
}

// ── encoding ────────────────────────────────────────────────────────────────
static inline const z_loaned_encoding_t* z_encoding_zenoh_string(void) {
    return (const z_loaned_encoding_t*)z_flat_encoding_zenoh_string();
}
static inline const z_loaned_encoding_t* z_encoding_zenoh_bytes(void) {
    return (const z_loaned_encoding_t*)z_flat_encoding_zenoh_bytes();
}
static inline const z_loaned_encoding_t* z_encoding_text_plain(void) {
    return (const z_loaned_encoding_t*)z_flat_encoding_text_plain();
}
static inline void z_encoding_clone(z_owned_encoding_t* dst, const z_loaned_encoding_t* src) {
    dst->_0 = z_flat_encoding_clone((const z_flat_encoding_t*)src);
}
static inline z_result_t z_encoding_to_string(const z_loaned_encoding_t* e, z_owned_string_t* out) {
    char* s = z_flat_encoding_to_string((const z_flat_encoding_t*)e);
    out->_val = s;
    out->_len = s ? strlen(s) : 0;
    return Z_OK;
}

// ── put / delete / get options ───────────────────────────────────────────────
// The `reliability` option is unstable in zenoh (and in zenoh-flat's C ABI);
// the field and its plumbing are gated by `ZENOH_FLAT_UNSTABLE_API`.
typedef struct z_put_options_t {
    z_moved_encoding_t* encoding;
    z_congestion_control_t congestion_control;
    z_priority_t priority;
    bool is_express;
    const void* timestamp;  // unsupported: ignored
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_reliability_t reliability;
#endif
    z_moved_bytes_t* attachment;
} z_put_options_t;
static inline void z_put_options_default(z_put_options_t* o) {
    o->encoding = NULL;
    o->congestion_control = Block;
    o->priority = Data;
    o->is_express = false;
    o->timestamp = NULL;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    o->reliability = Reliable;
#endif
    o->attachment = NULL;
}
typedef struct z_delete_options_t {
    z_congestion_control_t congestion_control;
    z_priority_t priority;
    bool is_express;
    const void* timestamp;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_reliability_t reliability;
#endif
} z_delete_options_t;
static inline void z_delete_options_default(z_delete_options_t* o) {
    o->congestion_control = Block;
    o->priority = Data;
    o->is_express = false;
    o->timestamp = NULL;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    o->reliability = Reliable;
#endif
}

static inline z_result_t z_put(const z_loaned_session_t* s, const z_loaned_keyexpr_t* ke, z_moved_bytes_t* payload,
                               z_put_options_t* opts) {
    z_flat_encoding_t* enc_owned = NULL;
    z_congestion_control_t cc = Block;
    z_priority_t pr = Data;
    bool ex = false;
    z_flat_zbytes_t* att = NULL;
    if (opts) {
        if (opts->encoding) {
            enc_owned = opts->encoding->_this._0;
            opts->encoding->_this._0 = NULL;
        }
        cc = opts->congestion_control;
        pr = opts->priority;
        ex = opts->is_express;
        if (opts->attachment) {
            att = opts->attachment->_this._0;
            opts->attachment->_this._0 = NULL;
        }
    }
    const z_flat_encoding_t* enc = enc_owned ? enc_owned : z_flat_encoding_zenoh_bytes();
    z_flat_error_t e = {0};
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_flat_session_put((const z_flat_session_t*)s, (const z_flat_keyexpr_t*)ke, payload->_this._0, enc, cc, pr, ex, att,
                       opts ? opts->reliability : Reliable, &e);
#else
    z_flat_session_put((const z_flat_session_t*)s, (const z_flat_keyexpr_t*)ke, payload->_this._0, enc, cc, pr, ex, att,
                       &e);
#endif
    payload->_this._0 = NULL;
    if (enc_owned) z_flat_encoding_drop(enc_owned);
    return _z_map(e);
}

static inline z_result_t z_delete(const z_loaned_session_t* s, const z_loaned_keyexpr_t* ke, z_delete_options_t* opts) {
    z_congestion_control_t cc = Block;
    z_priority_t pr = Data;
    bool ex = false;
    if (opts) {
        cc = opts->congestion_control;
        pr = opts->priority;
        ex = opts->is_express;
    }
    z_flat_error_t e = {0};
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_flat_session_delete((const z_flat_session_t*)s, (const z_flat_keyexpr_t*)ke, cc, pr, ex, NULL,
                          opts ? opts->reliability : Reliable, &e);
#else
    z_flat_session_delete((const z_flat_session_t*)s, (const z_flat_keyexpr_t*)ke, cc, pr, ex, NULL, &e);
#endif
    return _z_map(e);
}

// ── publisher ────────────────────────────────────────────────────────────────
typedef struct z_publisher_options_t {
    z_moved_encoding_t* encoding;
    z_congestion_control_t congestion_control;
    z_priority_t priority;
    bool is_express;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_reliability_t reliability;
#endif
} z_publisher_options_t;
static inline void z_publisher_options_default(z_publisher_options_t* o) {
    o->encoding = NULL;
    o->congestion_control = Block;
    o->priority = Data;
    o->is_express = false;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    o->reliability = Reliable;
#endif
}
typedef struct z_publisher_put_options_t {
    z_moved_encoding_t* encoding;
    z_congestion_control_t congestion_control;
    z_priority_t priority;
    bool is_express;
    const void* timestamp;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_reliability_t reliability;
#endif
    z_moved_bytes_t* attachment;
} z_publisher_put_options_t;
static inline void z_publisher_put_options_default(z_publisher_put_options_t* o) {
    o->encoding = NULL;
    o->congestion_control = Block;
    o->priority = Data;
    o->is_express = false;
    o->timestamp = NULL;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    o->reliability = Reliable;
#endif
    o->attachment = NULL;
}

static inline z_result_t z_declare_publisher(const z_loaned_session_t* s, z_owned_publisher_t* p,
                                             const z_loaned_keyexpr_t* ke, z_publisher_options_t* opts) {
    z_congestion_control_t cc = Block;
    z_priority_t pr = Data;
    bool ex = false;
    if (opts) {
        cc = opts->congestion_control;
        pr = opts->priority;
        ex = opts->is_express;
    }
    z_flat_keyexpr_t* ke_owned = z_flat_keyexpr_clone((const z_flat_keyexpr_t*)ke);  // flat consumes
    z_flat_error_t e = {0};
#if defined(ZENOH_FLAT_UNSTABLE_API)
    p->_0 = z_flat_session_declare_publisher((const z_flat_session_t*)s, ke_owned, cc, pr, ex,
                                             opts ? opts->reliability : Reliable, &e);
#else
    p->_0 = z_flat_session_declare_publisher((const z_flat_session_t*)s, ke_owned, cc, pr, ex, &e);
#endif
    return _z_map(e);
}
static inline z_result_t z_publisher_put(const z_loaned_publisher_t* pub, z_moved_bytes_t* payload,
                                         z_publisher_put_options_t* opts) {
    z_flat_encoding_t* enc_owned = NULL;
    z_flat_zbytes_t* att = NULL;
    if (opts) {
        if (opts->encoding) {
            enc_owned = opts->encoding->_this._0;
            opts->encoding->_this._0 = NULL;
        }
        if (opts->attachment) {
            att = opts->attachment->_this._0;
            opts->attachment->_this._0 = NULL;
        }
    }
    const z_flat_encoding_t* enc = enc_owned ? enc_owned : z_flat_encoding_zenoh_bytes();
    z_flat_error_t e = {0};
    z_flat_publisher_put((const z_flat_publisher_t*)pub, payload->_this._0, enc, att, &e);
    payload->_this._0 = NULL;
    if (enc_owned) z_flat_encoding_drop(enc_owned);
    return _z_map(e);
}
static inline z_result_t z_publisher_delete(const z_loaned_publisher_t* pub, void* opts) {
    (void)opts;
    z_flat_error_t e = {0};
    z_flat_publisher_delete((const z_flat_publisher_t*)pub, NULL, &e);
    return _z_map(e);
}

// Matching-status listeners are not provided by zenoh-flat — stubbed as a no-op
// so examples that optionally use them still compile (the listener simply never
// fires). The closure is dropped immediately.
typedef struct z_matching_status_t {
    bool matching;
} z_matching_status_t;
typedef struct z_owned_closure_matching_status_t {
    void* context;
    void (*call)(const z_matching_status_t*, void*);
    void (*drop)(void*);
} z_owned_closure_matching_status_t;
typedef struct z_moved_closure_matching_status_t {
    z_owned_closure_matching_status_t _this;
} z_moved_closure_matching_status_t;
static inline z_moved_closure_matching_status_t* z_closure_matching_status_move(
    z_owned_closure_matching_status_t* c) {
    return (z_moved_closure_matching_status_t*)c;
}
static inline void z_closure_matching_status(z_owned_closure_matching_status_t* c,
                                             void (*call)(const z_matching_status_t*, void*), void (*drop)(void*),
                                             void* ctx) {
    c->context = ctx;
    c->call = call;
    c->drop = drop;
}
static inline z_result_t z_publisher_declare_background_matching_listener(const z_loaned_publisher_t* pub,
                                                                          z_moved_closure_matching_status_t* cb) {
    (void)pub;
    if (cb->_this.drop) cb->_this.drop(cb->_this.context);  // not supported: release immediately
    return Z_OK;
}

// ── subscriber ───────────────────────────────────────────────────────────────
typedef struct z_subscriber_options_t {
    uint8_t _dummy;
} z_subscriber_options_t;
static inline void z_subscriber_options_default(z_subscriber_options_t* o) { o->_dummy = 0; }

static inline z_result_t z_declare_subscriber(const z_loaned_session_t* s, z_owned_subscriber_t* sub,
                                              const z_loaned_keyexpr_t* ke, z_moved_closure_sample_t* cb,
                                              const z_subscriber_options_t* opts) {
    (void)opts;
    _z_bridge_t* b = _z_bridge_new(cb->_this.context, (void*)cb->_this.call, cb->_this.drop);
    z_flat_closure_sample_t fcb = {b, _z_call_sample, NULL};
    z_flat_closure_drop_t fclose = {b, _z_bridge_close, NULL};
    z_flat_keyexpr_t* ke_owned = z_flat_keyexpr_clone((const z_flat_keyexpr_t*)ke);
    z_flat_error_t e = {0};
    sub->_0 = z_flat_session_declare_subscriber((const z_flat_session_t*)s, ke_owned, fcb, fclose, &e);
    if (!sub->_0) free(b);  // declare failed: on_close won't fire
    return _z_map(e);
}
static inline z_result_t z_undeclare_subscriber(z_moved_subscriber_t* m) {
    z_subscriber_drop(m);
    return Z_OK;
}

// ── queryable ────────────────────────────────────────────────────────────────
typedef struct z_queryable_options_t {
    bool complete;
} z_queryable_options_t;
static inline void z_queryable_options_default(z_queryable_options_t* o) { o->complete = false; }

static inline z_result_t z_declare_queryable(const z_loaned_session_t* s, z_owned_queryable_t* q,
                                             const z_loaned_keyexpr_t* ke, z_moved_closure_query_t* cb,
                                             const z_queryable_options_t* opts) {
    bool complete = opts ? opts->complete : false;
    _z_bridge_t* b = _z_bridge_new(cb->_this.context, (void*)cb->_this.call, cb->_this.drop);
    z_flat_closure_query_t fcb = {b, _z_call_query, NULL};
    z_flat_closure_drop_t fclose = {b, _z_bridge_close, NULL};
    z_flat_keyexpr_t* ke_owned = z_flat_keyexpr_clone((const z_flat_keyexpr_t*)ke);
    z_flat_error_t e = {0};
    q->_0 = z_flat_session_declare_queryable((const z_flat_session_t*)s, ke_owned, complete, fcb, fclose, &e);
    if (!q->_0) free(b);
    return _z_map(e);
}
static inline z_result_t z_undeclare_queryable(z_moved_queryable_t* m) {
    z_queryable_drop(m);
    return Z_OK;
}

// ── query accessors + reply ──────────────────────────────────────────────────
static inline const z_loaned_keyexpr_t* z_query_keyexpr(const z_loaned_query_t* q) {
    return (const z_loaned_keyexpr_t*)z_flat_query_keyexpr((const z_flat_query_t*)q);
}
static inline void z_query_parameters(const z_loaned_query_t* q, z_view_string_t* out) {
    char* s = z_flat_query_parameters((const z_flat_query_t*)q);  // owned; view leaks (see README)
    out->_val = s;
    out->_len = s ? strlen(s) : 0;
}
static inline const z_loaned_bytes_t* z_query_payload(const z_loaned_query_t* q) {
    return (const z_loaned_bytes_t*)z_flat_query_payload((const z_flat_query_t*)q);
}
typedef struct z_query_reply_options_t {
    z_moved_encoding_t* encoding;
    z_moved_bytes_t* attachment;
} z_query_reply_options_t;
static inline void z_query_reply_options_default(z_query_reply_options_t* o) {
    o->encoding = NULL;
    o->attachment = NULL;
}
static inline z_result_t z_query_reply(const z_loaned_query_t* q, const z_loaned_keyexpr_t* ke,
                                       z_moved_bytes_t* payload, z_query_reply_options_t* opts) {
    z_flat_encoding_t* enc_owned = NULL;
    z_flat_zbytes_t* att = NULL;
    if (opts) {
        if (opts->encoding) {
            enc_owned = opts->encoding->_this._0;
            opts->encoding->_this._0 = NULL;
        }
        if (opts->attachment) {
            att = opts->attachment->_this._0;
            opts->attachment->_this._0 = NULL;
        }
    }
    const z_flat_encoding_t* enc = enc_owned ? enc_owned : z_flat_encoding_zenoh_bytes();
    z_flat_error_t e = {0};
    z_flat_query_reply_success((const z_flat_query_t*)q, (const z_flat_keyexpr_t*)ke, payload->_this._0, enc, NULL, att,
                               false, &e);
    payload->_this._0 = NULL;
    if (enc_owned) z_flat_encoding_drop(enc_owned);
    return _z_map(e);
}

// ── sample accessors ─────────────────────────────────────────────────────────
static inline const z_loaned_keyexpr_t* z_sample_keyexpr(const z_loaned_sample_t* s) {
    return (const z_loaned_keyexpr_t*)z_flat_sample_key_expr((const z_flat_sample_t*)s);
}
static inline const z_loaned_bytes_t* z_sample_payload(const z_loaned_sample_t* s) {
    return (const z_loaned_bytes_t*)z_flat_sample_payload((const z_flat_sample_t*)s);
}
static inline const z_loaned_encoding_t* z_sample_encoding(const z_loaned_sample_t* s) {
    return (const z_loaned_encoding_t*)z_flat_sample_encoding((const z_flat_sample_t*)s);
}
static inline const z_loaned_bytes_t* z_sample_attachment(const z_loaned_sample_t* s) {
    return (const z_loaned_bytes_t*)z_flat_sample_attachment((const z_flat_sample_t*)s);
}
static inline z_sample_kind_t z_sample_kind(const z_loaned_sample_t* s) {
    return z_flat_sample_kind((const z_flat_sample_t*)s);
}
static inline z_priority_t z_sample_priority(const z_loaned_sample_t* s) {
    return z_flat_sample_priority((const z_flat_sample_t*)s);
}
static inline bool z_sample_express(const z_loaned_sample_t* s) {
    return z_flat_sample_express((const z_flat_sample_t*)s);
}

// ── info ──────────────────────────────────────────────────────────────────────
static inline z_id_t z_info_zid(const z_loaned_session_t* s) {
    z_id_t id;
    _z_fill_id(&id, z_flat_session_zid((const z_flat_session_t*)s));
    return id;
}
static inline void _z_zid_array_each(z_flat_zenoh_id_t** arr, uintptr_t n, z_owned_closure_zid_t* cb) {
    for (uintptr_t i = 0; i < n; i++) {
        z_id_t id;
        memset(id.id, 0, 16);
        uintptr_t m = 0;
        uint8_t* b = z_flat_zenoh_id_to_bytes(arr[i], &m);
        if (b) {
            if (m > 16) m = 16;
            memcpy(id.id, b, m);
            z_flat_free(b);
        }
        if (cb->call) cb->call(&id, cb->context);
    }
    if (cb->drop) cb->drop(cb->context);
    z_flat_free_array(arr, n, z_flat_zenoh_id_drop);
}
static inline z_result_t z_info_routers_zid(const z_loaned_session_t* s, z_moved_closure_zid_t* cb) {
    uintptr_t n = 0;
    z_flat_zenoh_id_t** arr = z_flat_session_routers_zid((const z_flat_session_t*)s, &n);
    _z_zid_array_each(arr, n, &cb->_this);
    return Z_OK;
}
static inline z_result_t z_info_peers_zid(const z_loaned_session_t* s, z_moved_closure_zid_t* cb) {
    uintptr_t n = 0;
    z_flat_zenoh_id_t** arr = z_flat_session_peers_zid((const z_flat_session_t*)s, &n);
    _z_zid_array_each(arr, n, &cb->_this);
    return Z_OK;
}

// ── hello / scouting ─────────────────────────────────────────────────────────
static inline z_id_t z_hello_zid(const z_loaned_hello_t* h) {
    z_id_t id;
    _z_fill_id(&id, z_flat_hello_zid((const z_flat_hello_t*)h));
    return id;
}
static inline z_whatami_t z_hello_whatami(const z_loaned_hello_t* h) {
    return z_flat_hello_whatami((const z_flat_hello_t*)h);
}
static inline void z_whatami_to_view_string(z_whatami_t w, z_view_string_t* out) {
    const char* s = "other";
    if (w == Router)
        s = "router";
    else if (w == Peer)
        s = "peer";
    else if (w == Client)
        s = "client";
    out->_val = s;  // static storage; no free
    out->_len = strlen(s);
}
static inline z_result_t z_hello_locators(const z_loaned_hello_t* h, z_owned_string_array_t* out) {
    uintptr_t n = 0;
    char** arr = z_flat_hello_locators((const z_flat_hello_t*)h, &n);
    out->_items = (z_loaned_string_t*)malloc(sizeof(z_loaned_string_t) * (n ? n : 1));
    out->_len = n;
    for (uintptr_t i = 0; i < n; i++) {
        out->_items[i]._val = arr[i];  // moved (string ownership transferred)
        out->_items[i]._len = arr[i] ? strlen(arr[i]) : 0;
    }
    if (arr) z_flat_free(arr);  // free only the outer block; strings live in _items
    return Z_OK;
}

typedef struct z_scout_options_t {
    uint32_t timeout_ms;
} z_scout_options_t;
static inline void z_scout_options_default(z_scout_options_t* o) { o->timeout_ms = 1000; }

// zenoh-c `z_scout` is non-blocking and self-terminates after `timeout_ms`, then
// runs the closure's drop. flat's scout instead runs until its handle is
// dropped, so a detached timer thread drops it after the timeout — that drop
// fires flat's `on_close`, i.e. `_z_bridge_close` (zenoh-c drop + free bridge).
struct _z_scout_timer_arg {
    z_flat_scout_t* sc;
    uint32_t timeout_ms;
};
static inline void* _z_scout_timer(void* a) {
    struct _z_scout_timer_arg* arg = (struct _z_scout_timer_arg*)a;
    usleep((useconds_t)arg->timeout_ms * 1000);
    z_flat_scout_drop(arg->sc);  // → flat on_close → _z_bridge_close
    free(arg);
    return NULL;
}
static inline z_result_t z_scout(z_moved_config_t* cfg, z_moved_closure_hello_t* cb, const z_scout_options_t* opts) {
    uint32_t timeout = opts ? opts->timeout_ms : 1000;
    _z_bridge_t* b = _z_bridge_new(cb->_this.context, (void*)cb->_this.call, cb->_this.drop);
    z_flat_closure_hello_t fcb = {b, _z_call_hello, NULL};
    z_flat_closure_drop_t fclose = {b, _z_bridge_close, NULL};
    z_flat_error_t e = {0};
    z_flat_scout_t* sc = z_flat_scout(7 /* Router|Peer|Client */, cfg->_this._0, fcb, fclose, &e);
    if (cfg->_this._0) {  // zenoh-c consumes the config; flat only borrowed it
        z_flat_config_drop(cfg->_this._0);
        cfg->_this._0 = NULL;
    }
    if (!sc) {
        free(b);
        return _z_map(e);
    }
    struct _z_scout_timer_arg* arg = (struct _z_scout_timer_arg*)malloc(sizeof(*arg));
    arg->sc = sc;
    arg->timeout_ms = timeout;
    pthread_t t;
    pthread_create(&t, NULL, _z_scout_timer, arg);
    pthread_detach(t);
    return Z_OK;
}

#endif  // ZENOH_COMPAT_COMMONS_H
