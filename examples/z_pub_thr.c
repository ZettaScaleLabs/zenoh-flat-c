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
// Throughput publisher: declare a publisher and put a fixed-size payload in a
// tight loop. The master payload is built once, then cheaply cloned each
// iteration (`z_zbytes_clone` bumps a refcount — no per-message copy), since
// `z_publisher_put` consumes the payload it is given.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_args.h"
#include "zenoh_flat.h"

#define DEFAULT_PRIORITY Data

struct args_t {
    unsigned int size;      // positional_1
    z_priority_t priority;  // -p, --priority
    bool express;           // --express
};
struct args_t parse_args(int argc, char** argv, z_config_t** config);

int main(int argc, char** argv) {
    const char* keyexpr = "test/thr";

    z_init_zenoh_logs_from_env_or("error");

    z_config_t* config = NULL;
    struct args_t args = parse_args(argc, argv, &config);

    uint8_t* value = (uint8_t*)malloc(args.size);
    for (size_t i = 0; i < args.size; ++i) {
        value[i] = i % 10;
    }

    z_session_t* s = z_open(config, NULL);  // consumes the config
    if (!s) {
        printf("Unable to open session!\n");
        free(value);
        return -1;
    }

    z_keyexpr_t* ke = z_keyexpr_try_from(keyexpr, NULL);
    // `declare_publisher` CONSUMES the key expression.
    z_congestion_control_t congestion = Block;
    bool express = args.express;
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_publisher_t* pub = z_session_declare_publisher(s, ke, &congestion, &args.priority, &express, NULL, NULL);
#else
    z_publisher_t* pub = z_session_declare_publisher(s, ke, &congestion, &args.priority, &express, NULL);
#endif
    if (!pub) {
        printf("Unable to declare publisher for key expression!\n");
        z_session_drop(s);
        free(value);
        return -1;
    }

    // Master payload, built once (held by value, inline — no heap handle). Each
    // `z_publisher_put` consumes its payload, so we hand it a cheap clone every
    // iteration: `z_zbytes_clone` bumps a refcount (no per-message copy) and the
    // value lives on the stack (no per-message malloc/free).
    z_zbytes_t payload = z_zbytes_from_slice(value, args.size);

    printf("Press CTRL-C to quit...\n");
    while (1) {
        z_zbytes_t to_send = z_zbytes_clone(&payload);
        z_publisher_put(pub, &to_send, NULL, NULL, NULL);
    }

    z_zbytes_drop(&payload);
    z_publisher_drop(pub);
    z_session_drop(s);
    free(value);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_pub_thr [OPTIONS] <PAYLOAD_SIZE>\n\n\
    Arguments:\n\
        <PAYLOAD_SIZE> (required, number): Size of the payload to publish\n\n\
    Options:\n\
        -p, --priority <PRIORITY> (optional, number [%d - %d], default='%d'): Priority for sending data\n\
        --express (optional): Batch messages.\n",
        RealTime, Background, DEFAULT_PRIORITY);
    printf(COMMON_HELP);
}

struct args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    struct args_t args;
    _Z_PARSE_ARG(args.priority, "p", "priority", parse_priority, DEFAULT_PRIORITY);
    args.express = _Z_CHECK_FLAG("express");

    parse_zenoh_common_args(argc, argv, config);
    const char* arg = check_unknown_opts(argc, argv);
    if (arg) {
        printf("Unknown option %s\n", arg);
        exit(-1);
    }
    char** pos_args = parse_pos_args(argc, argv, 1);
    if (!pos_args) {
        printf("Unexpected additional positional arguments\n");
        exit(-1);
    }
    if (!pos_args[0]) {
        printf("<PAYLOAD_SIZE> argument is required\n");
        free(pos_args);
        exit(-1);
    }
    args.size = atoi(pos_args[0]);
    free(pos_args);
    return args;
}
