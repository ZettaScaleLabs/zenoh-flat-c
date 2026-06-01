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

#define DEFAULT_KEYEXPR "demo/example/zenoh-flat-c-pub"
#define DEFAULT_VALUE "Pub from C!"

struct args_t {
    char* keyexpr;     // -k, --key
    char* value;       // -p, --payload
    char* attachment;  // -a, --attach
};
struct args_t parse_args(int argc, char** argv, z_config_t** config);

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

    printf("Declaring Publisher on '%s'...\n", args.keyexpr);
    z_keyexpr_t* ke = z_keyexpr_try_from(args.keyexpr, NULL);
    if (!ke) {
        printf("%s is not a valid key expression\n", args.keyexpr);
        z_session_drop(s);
        return -1;
    }
    // `declare_publisher` CONSUMES the key expression.
#if defined(ZENOH_FLAT_UNSTABLE_API)
    z_publisher_t* pub = z_session_declare_publisher(s, ke, Block, Data, false, Reliable, NULL);
#else
    z_publisher_t* pub = z_session_declare_publisher(s, ke, Block, Data, false, NULL);
#endif
    if (!pub) {
        printf("Unable to declare Publisher for key expression!\n");
        z_session_drop(s);
        return -1;
    }

    printf("Press CTRL-C to quit...\n");
    char buf[256] = {0};
    for (int idx = 0; 1; ++idx) {
        sleep(1);
        snprintf(buf, sizeof(buf), "[%4d] %s", idx, args.value);
        printf("Putting Data ('%s': '%s')...\n", args.keyexpr, buf);

        z_zbytes_t* payload = z_zbytes_from_slice((const uint8_t*)buf, strlen(buf));
        z_zbytes_t* attachment = NULL;
        if (args.attachment) {
            attachment = z_zbytes_from_slice((const uint8_t*)args.attachment, strlen(args.attachment));
        }
        // `publisher_put` consumes the payload + attachment.
        z_publisher_put(pub, payload, z_encoding_text_plain(), attachment, NULL);
    }

    z_publisher_drop(pub);
    z_session_drop(s);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_pub [OPTIONS]\n\n\
    Options:\n\
        -k, --key <KEYEXPR> (optional, string, default='%s'): The key expression to write to\n\
        -p, --payload <PAYLOAD> (optional, string, default='%s'): The value to write\n\
        -a, --attach <ATTACHMENT> (optional, string): The attachment to add to each put\n",
        DEFAULT_KEYEXPR, DEFAULT_VALUE);
    printf(COMMON_HELP);
}

struct args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    struct args_t args;
    _Z_PARSE_ARG(args.keyexpr, "k", "key", (char*), (char*)DEFAULT_KEYEXPR);
    _Z_PARSE_ARG(args.value, "p", "payload", (char*), (char*)DEFAULT_VALUE);
    _Z_PARSE_ARG(args.attachment, "a", "attach", (char*), NULL);

    parse_zenoh_common_args(argc, argv, config);
    const char* unknown_arg = check_unknown_opts(argc, argv);
    if (unknown_arg) {
        printf("Unknown option %s\n", unknown_arg);
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
