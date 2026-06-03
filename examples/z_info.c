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

#include "parse_args.h"
#include "zenoh_flat.h"

void print_zid(const z_zenoh_id_t* zid) {
    char* s = z_zenoh_id_to_string(zid);
    printf("%s\n", s);
    z_free(s);
}

// Print + free an array of zenoh ids. `z_zenoh_id_t` is now an inline by-value
// plain-data struct, so the result is a single contiguous block of ids (not an
// array of owned handles): print each by reference, then free the one block.
void print_zid_array(const char* label, const z_zenoh_id_t* ids, uintptr_t len) {
    printf("%s:\n", label);
    for (uintptr_t i = 0; i < len; i++) {
        printf("  ");
        print_zid(&ids[i]);
    }
    z_free((void*)ids);
}

void parse_args(int argc, char** argv, z_config_t** config);

int main(int argc, char** argv) {
    z_init_zenoh_logs_from_env_or("error");

    z_config_t* config = NULL;
    parse_args(argc, argv, &config);

    printf("Opening session...\n");
    z_session_t* s = z_open(config, NULL);
    if (!s) {
        printf("Unable to open session!\n");
        return -1;
    }

    z_zenoh_id_t self_id = z_session_zid(s);  // by value (plain data)
    printf("own id: ");
    print_zid(&self_id);

    uintptr_t n = 0;
    z_zenoh_id_t* routers = z_session_routers_zid(s, &n);
    print_zid_array("routers ids", routers, n);

    n = 0;
    z_zenoh_id_t* peers = z_session_peers_zid(s, &n);
    print_zid_array("peers ids", peers, n);

    z_session_drop(s);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_info [OPTIONS]\n\n\
    Options:\n");
    printf(COMMON_HELP);
}

void parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
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
}
