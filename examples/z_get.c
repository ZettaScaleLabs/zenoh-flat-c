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
// zenoh-flat exposes `get` in the async callback form (not zenoh-c's FIFO
// channel + blocking `z_recv` loop). Each reply is delivered to the reply
// closure; the `on_close` closure fires once when the reply stream ends
// (all queryables answered, or the timeout elapsed). We mirror zenoh-c's
// "block until done" semantics with a condition variable signalled by on_close.
//
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "parse_args.h"
#include "zenoh_flat.h"

#define DEFAULT_SELECTOR "demo/example/**"
#define DEFAULT_TIMEOUT_MS 10000

struct args_t {
    char* selector;           // -s, --selector
    char* value;              // -p, --payload
    z_query_target_t target;  // -t, --target
    uint64_t timeout_ms;      // -o, --timeout
};
struct args_t parse_args(int argc, char** argv, z_config_t** config);

// Shared completion state, passed as the closures' context.
typedef struct {
    pthread_mutex_t m;
    pthread_cond_t c;
    bool done;
} done_signal_t;

// The reply handler receives an OWNED `z_reply_t*` and must drop it.
void reply_handler(z_reply_t* reply, void* ctx) {
    (void)ctx;
    if (z_reply_is_ok(reply)) {
        const z_sample_t* sample = z_reply_sample(reply);  // borrowed
        char* keyexpr = z_keyexpr_to_string(z_sample_key_expr(sample));
        uintptr_t len = 0;
        uint8_t* payload = z_zbytes_to_bytes(z_sample_payload(sample), &len);
        printf(">> Received ('%s': '%.*s')\n", keyexpr, (int)len, payload);
        z_free(keyexpr);
        z_free(payload);
    } else {
        printf(">> Received an error\n");
    }
    z_reply_drop(reply);  // OWNED — release it
}

// Fired once when the reply stream ends (completion or timeout).
void on_close(void* ctx) {
    done_signal_t* sig = (done_signal_t*)ctx;
    pthread_mutex_lock(&sig->m);
    sig->done = true;
    pthread_cond_signal(&sig->c);
    pthread_mutex_unlock(&sig->m);
}

int main(int argc, char** argv) {
    z_init_zenoh_logs_from_env_or("error");

    z_config_t* config = NULL;
    struct args_t args = parse_args(argc, argv, &config);

    // Split the selector into the key-expression part and the parameters part.
    char* selector = args.selector;
    char* params = strchr(selector, '?');
    char* ke_str = selector;
    if (params != NULL) {
        *params = '\0';  // terminate the key-expr part
        params += 1;     // parameters follow the '?'
    }

    printf("Opening session...\n");
    z_session_t* s = z_open(config, NULL);  // consumes the config
    if (!s) {
        printf("Unable to open session!\n");
        return -1;
    }

    z_keyexpr_t* ke = z_keyexpr_try_from(ke_str, NULL);
    if (!ke) {
        printf("%s is not a valid key expression\n", ke_str);
        z_session_drop(s);
        return -1;
    }

    z_zbytes_t* payload = NULL;
    if (args.value != NULL) {
        payload = z_zbytes_from_slice((const uint8_t*)args.value, strlen(args.value));
    }

    done_signal_t sig;
    pthread_mutex_init(&sig.m, NULL);
    pthread_cond_init(&sig.c, NULL);
    sig.done = false;

    z_closure_reply_t callback = {&sig, reply_handler, NULL};
    z_closure_drop_t closer = {&sig, on_close, NULL};

    printf("Sending Query '%s'...\n", args.selector);
    // Borrows the key expression; consumes the payload; encoding is borrowed.
    bool ok = z_session_get(s, ke, params, (int64_t)args.timeout_ms, args.target, Auto, Any, Block, Data, false,
                            payload, z_encoding_zenoh_bytes(), NULL, callback, closer, NULL);
    z_keyexpr_drop(ke);  // get only borrowed it

    if (ok) {
        // Wait for `on_close`, bounded by the query timeout plus a margin so we
        // never hang if it doesn't fire.
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += (time_t)(args.timeout_ms / 1000) + 2;
        pthread_mutex_lock(&sig.m);
        while (!sig.done) {
            if (pthread_cond_timedwait(&sig.c, &sig.m, &deadline) != 0) {
                break;  // timed out waiting for completion
            }
        }
        pthread_mutex_unlock(&sig.m);
    } else {
        printf("Get failed...\n");
    }

    pthread_cond_destroy(&sig.c);
    pthread_mutex_destroy(&sig.m);
    z_session_drop(s);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_get [OPTIONS]\n\n\
    Options:\n\
        -s, --selector <SELECTOR> (optional, string, default='%s'): The selection of resources to query\n\
        -p, --payload <PAYLOAD> (optional, string): An optional value to put in the query\n\
        -t, --target <TARGET> (optional, BEST_MATCHING | ALL | ALL_COMPLETE): Query target\n\
        -o, --timeout <TIMEOUT_MS> (optional, number, default='%d'): Query timeout in milliseconds\n",
        DEFAULT_SELECTOR, DEFAULT_TIMEOUT_MS);
    printf(COMMON_HELP);
}

struct args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    struct args_t args;
    _Z_PARSE_ARG(args.selector, "s", "selector", (char*), (char*)DEFAULT_SELECTOR);
    _Z_PARSE_ARG(args.value, "p", "payload", (char*), NULL);
    _Z_PARSE_ARG(args.timeout_ms, "o", "timeout", atoi, DEFAULT_TIMEOUT_MS);
    _Z_PARSE_ARG(args.target, "t", "target", parse_query_target, BestMatching);

    parse_zenoh_common_args(argc, argv, config);
    const char* arg = check_unknown_opts(argc, argv);
    if (arg) {
        printf("Unknown option %s\n", arg);
        exit(-1);
    }
    char** pos_args = parse_pos_args(argc, argv, 1);
    if (!pos_args || pos_args[0]) {
        printf("Unexpected positional arguments\n");
        free(pos_args);
        exit(-1);
    }
    free(pos_args);
    return args;
}
