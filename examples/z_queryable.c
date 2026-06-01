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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "parse_args.h"
#include "zenoh_flat.h"

#define DEFAULT_KEYEXPR "demo/example/zenoh-c-queryable"
#define DEFAULT_VALUE "Queryable from C!"

struct args_t {
    char* keyexpr;  // -k, --key
    char* value;    // -p, --payload
    bool complete;  // --complete
};
struct args_t parse_args(int argc, char** argv, z_config_t** config);

// The query handler receives an owned `z_query_t*`. The `context` carries the
// reply value string.
void query_handler(z_query_t* query, void* context) {
    const char* value = (const char*)context;

    char* keyexpr = z_keyexpr_to_string(z_query_keyexpr(query));
    char* params = z_query_parameters(query);  // owned string

    const z_zbytes_t* payload = z_query_payload(query);  // borrowed, NULL if none
    if (payload != NULL) {
        uintptr_t plen = 0;
        uint8_t* pbuf = z_zbytes_to_bytes(payload, &plen);
        printf(">> [Queryable] Received Query '%s?%s' with value '%.*s'\n", keyexpr, params, (int)plen, pbuf);
        z_free(pbuf);
    } else {
        printf(">> [Queryable] Received Query '%s?%s'\n", keyexpr, params);
    }

    printf(">> [Queryable] Responding ('%s': '%s')\n", keyexpr, value);
    const z_keyexpr_t* reply_ke = z_query_keyexpr(query);  // borrowed; reply borrows it too
    z_zbytes_t* reply_payload = z_zbytes_from_slice((const uint8_t*)value, strlen(value));
    // `z_query_reply_success` borrows the query + keyexpr and consumes the payload.
    z_query_reply_success(query, reply_ke, reply_payload, NULL, NULL, NULL, NULL, NULL);

    z_free(keyexpr);
    z_free(params);
    z_query_drop(query);
}

void on_close(void* arg) { (void)arg; }

int main(int argc, char** argv) {
    z_init_zenoh_logs_from_env_or("error");

    z_config_t* config = NULL;
    struct args_t args = parse_args(argc, argv, &config);

    printf("Opening session...\n");
    z_session_t* s = z_open(config, NULL);
    if (!s) {
        printf("Unable to open session!\n");
        return -1;
    }

    z_keyexpr_t* ke = z_keyexpr_try_from(args.keyexpr, NULL);
    if (!ke) {
        printf("%s is not a valid key expression\n", args.keyexpr);
        z_session_drop(s);
        return -1;
    }

    printf("Declaring Queryable on '%s'...\n", args.keyexpr);
    z_closure_query_t callback = {(void*)args.value, query_handler, NULL};
    z_closure_drop_t closer = {NULL, on_close, NULL};
    bool complete = args.complete;
    // `declare_queryable` CONSUMES the key expression.
    z_queryable_t* qable = z_session_declare_queryable(s, ke, &complete, callback, closer, NULL);
    if (!qable) {
        printf("Unable to create queryable.\n");
        z_session_drop(s);
        return -1;
    }

    printf("Press CTRL-C to quit...\n");
    while (1) {
        sleep(1);
    }

    z_queryable_drop(qable);
    z_session_drop(s);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_queryable [OPTIONS]\n\n\
    Options:\n\
        -k, --key <KEYEXPR> (optional, string, default='%s'): The key expression matching queries to reply to\n\
        -p, --payload <PAYLOAD> (optional, string, default='%s'): The value to reply with\n\
        --complete (optional): Whether the queryable is complete\n",
        DEFAULT_KEYEXPR, DEFAULT_VALUE);
    printf(COMMON_HELP);
}

struct args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    struct args_t args;
    _Z_PARSE_ARG(args.keyexpr, "k", "key", (char*), (char*)DEFAULT_KEYEXPR);
    _Z_PARSE_ARG(args.value, "p", "payload", (char*), (char*)DEFAULT_VALUE);
    args.complete = _Z_CHECK_FLAG("complete");

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
