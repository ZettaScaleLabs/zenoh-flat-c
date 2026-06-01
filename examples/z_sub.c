//
// Copyright (c) 2023 ZettaScale Technology
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
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "parse_args.h"
#include "zenoh_flat.h"

#define DEFAULT_KEYEXPR "demo/example/**"

struct args_t {
    char* keyexpr;  // -k, --key
};
struct args_t parse_args(int argc, char** argv, z_config_t** config);
const char* kind_to_str(z_sample_kind_t kind);

// The sample handler receives a LOANED `z_sample_t*` valid only for the call. The
// accessors (`z_sample_key_expr`, `z_sample_payload`, ...) return BORROWS valid
// only while the sample is alive; the strings they materialize are owned and
// must be `z_free`d.
void data_handler(z_sample_t* sample, void* arg) {
    (void)arg;
    char* keyexpr = z_keyexpr_to_string(z_sample_key_expr(sample));

    uintptr_t payload_len = 0;
    uint8_t* payload = z_zbytes_to_bytes(z_sample_payload(sample), &payload_len);

    printf(">> [Subscriber] Received %s ('%s': '%.*s')", kind_to_str(z_sample_kind(sample)), keyexpr,
           (int)payload_len, payload);

    const z_zbytes_t* attachment = z_sample_attachment(sample);  // borrowed, NULL if none
    if (attachment != NULL) {
        uintptr_t att_len = 0;
        uint8_t* att = z_zbytes_to_bytes(attachment, &att_len);
        printf(" (%.*s)", (int)att_len, att);
        z_free(att);
    }
    printf("\n");

    z_free(keyexpr);
    z_free(payload);
    z_sample_drop(sample);
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

    printf("Declaring Subscriber on '%s'...\n", args.keyexpr);
    z_closure_sample_t callback = {NULL, data_handler, NULL};
    z_closure_drop_t closer = {NULL, on_close, NULL};
    // `declare_subscriber` CONSUMES the key expression.
    z_subscriber_t* sub = z_session_declare_subscriber(s, ke, callback, closer, NULL);
    if (!sub) {
        printf("Unable to declare subscriber.\n");
        z_session_drop(s);
        return -1;
    }

    printf("Press CTRL-C to quit...\n");
    while (1) {
        sleep(1);
    }

    z_subscriber_drop(sub);
    z_session_drop(s);
    return 0;
}

const char* kind_to_str(z_sample_kind_t kind) {
    switch (kind) {
        case Put:
            return "PUT";
        case Delete:
            return "DELETE";
        default:
            return "UNKNOWN";
    }
}

void print_help() {
    printf(
        "\
    Usage: z_sub [OPTIONS]\n\n\
    Options:\n\
        -k, --key <KEYEXPR> (optional, string, default='%s'): The key expression to subscribe to\n",
        DEFAULT_KEYEXPR);
    printf(COMMON_HELP);
}

struct args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    struct args_t args;
    _Z_PARSE_ARG(args.keyexpr, "k", "key", (char*), (char*)DEFAULT_KEYEXPR);

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
