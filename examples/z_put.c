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

#include "parse_args.h"
#include "zenoh_flat.h"

#define DEFAULT_KEYEXPR "demo/example/zenoh-flat-c-put"
#define DEFAULT_VALUE "Put from C!"

struct args_t {
    char* keyexpr;  // -k, --key
    char* value;    // -p, --payload
};
struct args_t parse_args(int argc, char** argv, z_config_t** config);

int main(int argc, char** argv) {
    z_init_zenoh_logs_from_env_or("error");

    z_config_t* config = NULL;
    struct args_t args = parse_args(argc, argv, &config);

    printf("Opening session...\n");
    z_session_t* s = z_open(config, NULL);  // consumes the config
    if (!s) {
        printf("Unable to open session!\n");
        return -1;
    }

    printf("Putting Data ('%s': '%s')...\n", args.keyexpr, args.value);
    z_keyexpr_t* ke = z_keyexpr_try_from(args.keyexpr, NULL);
    if (!ke) {
        printf("%s is not a valid key expression\n", args.keyexpr);
        z_session_drop(s);
        return -1;
    }
    z_zbytes_t* payload = z_zbytes_from_slice((const uint8_t*)args.value, strlen(args.value));

    // `z_session_put` borrows the key expression and CONSUMES the payload.
#if defined(ZENOH_FLAT_UNSTABLE_API)
    bool ok = z_session_put(s, ke, payload, z_encoding_zenoh_bytes(), Block, Data, false, NULL, Reliable, NULL);
#else
    bool ok = z_session_put(s, ke, payload, z_encoding_zenoh_bytes(), Block, Data, false, NULL, NULL);
#endif
    if (!ok) {
        printf("Put failed...\n");
    }

    z_keyexpr_drop(ke);  // put only borrowed it
    z_session_drop(s);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_put [OPTIONS]\n\n\
    Options:\n\
        -k, --key <KEYEXPR> (optional, string, default='%s'): The key expression to write to\n\
        -p, --payload <PAYLOAD> (optional, string, default='%s'): The value to write\n",
        DEFAULT_KEYEXPR, DEFAULT_VALUE);
    printf(COMMON_HELP);
}

struct args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    struct args_t args;
    _Z_PARSE_ARG(args.keyexpr, "k", "key", (char*), (char*)DEFAULT_KEYEXPR);
    _Z_PARSE_ARG(args.value, "p", "payload", (char*), (char*)DEFAULT_VALUE);

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
