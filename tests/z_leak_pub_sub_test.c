//
// Copyright (c) 2024 ZettaScale Technology
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
// In-process publisher + subscriber over the flat API. Verifies a sample is
// delivered through the callback handler (run under valgrind for a leak check).
//
// NOTE: the subscriber callback receives the sample by TAKEABLE pointer — the
// trampoline drops it after the callback returns. So the handler must only READ
// it (as below); calling `z_sample_drop` here would double-free the sample's
// payload buffer. To keep the sample, move it out with `z_sample_take` instead.
//
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "zenoh_flat.h"

#undef NDEBUG
#include <assert.h>

#define KEYEXPR "demo/example/zenoh-flat-c-leak-test"
#define VALUE "leak-test-payload"
#define N_MSGS 5

static int g_received = 0;

void data_handler(z_sample_t* sample, void* arg) {
    (void)arg;
    char* ke = z_keyexpr_to_string(z_sample_key_expr(sample));
    uintptr_t len = 0;
    uint8_t* payload = z_zbytes_to_bytes(z_sample_payload(sample), &len);
    assert(ke != NULL);
    assert(strncmp((const char*)payload, VALUE, strlen(VALUE)) == 0);
    g_received++;
    z_free(ke);
    z_free(payload);
}

void on_close(void* arg) { (void)arg; }

int main(void) {
    z_init_zenoh_logs_from_env_or("error");

    z_session_t* pub_session = z_open(z_config_default(), NULL);
    z_session_t* sub_session = z_open(z_config_default(), NULL);
    assert(pub_session && sub_session);

    z_keyexpr_t* sub_ke = z_keyexpr_try_from(KEYEXPR, NULL);
    z_closure_sample_t callback = {NULL, data_handler, NULL};
    z_closure_drop_t closer = {NULL, on_close, NULL};
    z_subscriber_t* sub = z_session_declare_subscriber(sub_session, sub_ke, callback, closer, NULL);  // consumes sub_ke
    assert(sub != NULL);

    z_keyexpr_t* pub_ke = z_keyexpr_try_from(KEYEXPR, NULL);
    z_congestion_control_t congestion = Block;
    z_priority_t priority = Data;
    bool express = false;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_reliability_t reliability = Reliable;
    z_publisher_t* pub =
        z_session_declare_publisher(pub_session, pub_ke, &congestion, &priority, &express, &reliability, NULL);
#else
    z_publisher_t* pub =
        z_session_declare_publisher(pub_session, pub_ke, &congestion, &priority, &express, NULL);  // consumes pub_ke
#endif
    assert(pub != NULL);

    sleep(1);  // let the subscriber connect
    for (int i = 0; i < N_MSGS; i++) {
        z_zbytes_t payload = z_zbytes_from_slice((const uint8_t*)VALUE, strlen(VALUE));
        bool ok = z_publisher_put(pub, &payload, z_encoding_zenoh_bytes(), NULL, NULL);
        assert(ok);
    }
    sleep(1);  // let samples arrive

    z_publisher_drop(pub);
    z_subscriber_drop(sub);
    z_session_drop(pub_session);
    z_session_drop(sub_session);

    printf("received %d sample(s)\n", g_received);
    return g_received > 0 ? 0 : 1;
}
