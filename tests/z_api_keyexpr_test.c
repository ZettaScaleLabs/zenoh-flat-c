//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>
//
#include <string.h>

#include "zenoh_flat.h"

#undef NDEBUG
#include <assert.h>

void test_create(void) {
    // Valid key expressions parse; invalid ones return NULL (error reported via
    // the return value — `e` is passed NULL).
    z_keyexpr_t* ke = z_keyexpr_try_from("a/b/c", NULL);
    assert(ke != NULL);
    z_keyexpr_drop(ke);

    z_keyexpr_t* bad = z_keyexpr_try_from("a/**/**", NULL);  // ** adjacent → not canon
    assert(bad == NULL);
}

void test_to_string(void) {
    z_keyexpr_t* ke = z_keyexpr_try_from("a/b/c", NULL);
    assert(ke != NULL);
    char* s = z_keyexpr_to_string(ke);
    assert(s != NULL && strcmp(s, "a/b/c") == 0);
    z_free(s);
    z_keyexpr_drop(ke);
}

void test_relations(void) {
    z_keyexpr_t* pattern = z_keyexpr_try_from("a/*/c", NULL);
    z_keyexpr_t* exact = z_keyexpr_try_from("a/b/c", NULL);
    assert(pattern && exact);

    assert(z_keyexpr_intersects(pattern, exact));
    assert(z_keyexpr_includes(pattern, exact));
    assert(!z_keyexpr_includes(exact, pattern));

#if defined(ZENOH_FLAT_UNSTABLE_API)
    assert(z_keyexpr_relation_to(exact, exact) == Equals);
    assert(z_keyexpr_relation_to(pattern, exact) == Includes);
#endif

    z_keyexpr_drop(pattern);
    z_keyexpr_drop(exact);
}

void test_autocanonize(void) {
    // The canonical form of "a/**/**/c" is "a/**/c".
    z_keyexpr_t* ke = z_keyexpr_autocanonize("a/**/**/c", NULL);
    assert(ke != NULL);
    char* s = z_keyexpr_to_string(ke);
    assert(s != NULL && strcmp(s, "a/**/c") == 0);
    z_free(s);
    z_keyexpr_drop(ke);
}

void test_concat_join(void) {
    z_keyexpr_t* a = z_keyexpr_try_from("a/b", NULL);
    z_keyexpr_t* joined = z_keyexpr_join(a, "c/d", NULL);
    assert(joined != NULL);
    char* s = z_keyexpr_to_string(joined);
    assert(s != NULL && strcmp(s, "a/b/c/d") == 0);
    z_free(s);
    z_keyexpr_drop(joined);
    z_keyexpr_drop(a);
}

int main(void) {
    test_create();
    test_to_string();
    test_relations();
    test_autocanonize();
    test_concat_join();
    return 0;
}
