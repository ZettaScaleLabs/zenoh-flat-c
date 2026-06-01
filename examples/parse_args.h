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
// CLI argument + configuration parsing helper for the zenoh-flat-c examples,
// written against the flat `z_*` API (heap-pointer config handle).
//

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zenoh_flat.h"

// Configuration keys (zenoh JSON5 paths).
#define Z_CONFIG_MODE_KEY "mode"
#define Z_CONFIG_CONNECT_KEY "connect/endpoints"
#define Z_CONFIG_LISTEN_KEY "listen/endpoints"
#define Z_CONFIG_MULTICAST_SCOUTING_KEY "scouting/multicast/enabled"

#define COMMON_HELP \
    "\
        -c, --config <CONFIG> (optional, string): The path to a configuration file for the session. If this option isn't passed, the default configuration will be used.\n\
        -m, --mode <MODE> (optional, string, default='peer'): The zenoh session mode. [possible values: peer, client, router]\n\
        -e, --connect <CONNECT> (optional, string): Endpoint to connect to. Repeat option to pass multiple endpoints. If none are given, endpoints will be discovered through multicast-scouting if it is enabled.\n\
            e.g.: '-e tcp/192.168.1.1:7447'\n\
        -l, --listen <LISTEN> (optional, string): Locator to listen on. Repeat option to pass multiple locators. If none are given, the default configuration will be used.\n\
            e.g.: '-l tcp/192.168.1.1:7447'\n\
        --no-multicast-scouting (optional): By default zenohd replies to multicast scouting messages for being discovered by peers and clients. This option disables this feature.\n\
        --cfg (optional, string): Allows arbitrary configuration changes as column-separated KEY:VALUE pairs. Where KEY must be a valid config path and VALUE must be a valid JSON5 string that can be deserialized to the expected type for the KEY field. Example: --cfg='transport/unicast/max_links:2'.\n\
        -h, --help: Print help\n\
"
#define _Z_PARSE_ARG(VALUE, ID_SHORT, ID_LONG, FUNC, DEFAULT_VALUE)  \
    do {                                                             \
        const char* arg_val = parse_opt(argc, argv, ID_SHORT, true); \
        if (!arg_val) {                                              \
            arg_val = parse_opt(argc, argv, ID_LONG, true);          \
        }                                                            \
        if (!arg_val) {                                              \
            VALUE = DEFAULT_VALUE;                                   \
        } else {                                                     \
            VALUE = FUNC(arg_val);                                   \
        }                                                            \
    } while (0)

#define _Z_PARSE_ARG_SINGLE_OPT(VALUE, ID, FUNC, DEFAULT_VALUE) \
    do {                                                        \
        const char* arg_val = parse_opt(argc, argv, ID, true);  \
        if (!arg_val) {                                         \
            VALUE = DEFAULT_VALUE;                              \
        } else {                                                \
            VALUE = FUNC(arg_val);                              \
        }                                                       \
    } while (0)

#define _Z_CHECK_HELP                                                                    \
    do {                                                                                 \
        if (parse_opt(argc, argv, "h", false) || parse_opt(argc, argv, "help", false)) { \
            print_help();                                                                \
            exit(1);                                                                     \
        }                                                                                \
    } while (0)

#define _Z_CHECK_FLAG(ID) (parse_opt(argc, argv, ID, false) != NULL)

/**
 * Parse an option of format `-f`, `--flag`, `-f <value>` or `--flag <value>` from `argv`. If found, the option and its
 * eventual value are each replaced by NULL in `argv`
 * @returns NULL if option was not found, else a non-null value depending on if `opt_has_value`.
 */
const char* parse_opt(int argc, char** argv, const char* opt, bool opt_has_value) {
    size_t optlen = strlen(opt);
    char* value = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        size_t len = strlen(argv[i]);
        if (len < 2) {
            continue;
        }
        if (optlen == 1) {
            if (argv[i][0] == '-' && argv[i][1] == opt[0] && argv[i][2] == 0) {
                argv[i] = NULL;
                if (!opt_has_value) {
                    return (char*)opt;
                } else if (i + 1 < argc && argv[i + 1]) {
                    value = argv[i + 1];
                    argv[i + 1] = NULL;
                    return value;
                } else {
                    printf("Option -%s given without a value\n", opt);
                    exit(-1);
                }
            }
        } else if (optlen > 1 && len > 3 && argv[i][0] == '-' && argv[i][1] == '-') {
            if (strncmp(argv[i] + 2, opt, optlen) == 0) {
                char* pos = strchr(argv[i], '=');
                if (!opt_has_value) {
                    argv[i] = NULL;
                    return (char*)opt;
                } else if (pos != NULL) {
                    value = pos + 1;
                    argv[i] = NULL;
                    return value;
                } else if (i + 1 < argc && argv[i + 1]) {
                    argv[i] = NULL;
                    value = argv[i + 1];
                    argv[i + 1] = NULL;
                    return value;
                } else {
                    printf("Option --%s given without a value\n", opt);
                    exit(-1);
                }
            }
        }
    }
    return NULL;
}

/**
 * Check if any options remains in `argv`. Must be called after all expected options are parsed.
 * @returns NULL if no option was found, else the first option string that was found
 */
const char* check_unknown_opts(int argc, char** const argv) {
    for (int i = 1; i < argc; i++) {
        if (argv[i] && argv[i][0] == '-') {
            return argv[i];
        }
    }
    return NULL;
}

/**
 * Parse positional arguments from `argv`. Returned pointer is dynamically allocated and must be freed.
 */
char** parse_pos_args(const int argc, char** argv, const size_t nb_args) {
    char** pos_argv = (char**)calloc(nb_args, sizeof(char*));
    size_t pos_argc = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i]) {
            pos_argc++;
            if (pos_argc > nb_args) {
                free(pos_argv);
                return NULL;
            }
            pos_argv[pos_argc - 1] = argv[i];
        }
    }
    return pos_argv;
}

/**
 * Parse a repeated endpoint option (-e/-l) into a JSON list and insert it under `config_key`.
 */
void parse_zenoh_json_list_config(int argc, char** argv, const char* opt_short, const char* opt_long,
                                  const char* config_key, z_config_t* config) {
    char* buf = (char*)calloc(1, sizeof(char));
    const char* value;
    _Z_PARSE_ARG(value, opt_short, opt_long, (const char*), NULL);
    while (value) {
        size_t len_newbuf = strlen(buf) + strlen(value) + 4;  // value + quotes + comma + nullbyte
        char* newbuf = (char*)malloc(len_newbuf);
        snprintf(newbuf, len_newbuf, "%s'%s',", buf, value);
        free(buf);
        buf = newbuf;
        _Z_PARSE_ARG(value, opt_short, opt_long, (const char*), NULL);
    }
    size_t buflen = strlen(buf);
    if (buflen > 0) {
        buf[buflen - 1] = '\0';  // remove trailing comma
        buflen--;
        size_t json_list_len = buflen + 3;  // buf + brackets + nullbyte
        char* json_list = (char*)malloc(json_list_len);
        snprintf(json_list, json_list_len, "[%s]", buf);
        if (!z_config_insert_json5(config, config_key, json_list, NULL)) {
            printf("Couldn't insert value `%s` in configuration at `%s`\n", json_list, config_key);
            free(json_list);
            exit(-1);
        }
        free(json_list);
    }
    free(buf);
}

void parse_zenoh_json_list_cfg(int argc, char** argv, const char* opt_long, z_config_t* config) {
    char* buf = (char*)calloc(1, sizeof(char));
    const char* value;
    _Z_PARSE_ARG_SINGLE_OPT(value, opt_long, (char*), NULL);
    while (value) {
        char* pos = strchr(value, ':');
        if (pos == NULL) {
            printf("--cfg` argument: expected KEY:VALUE pair, got %s ", value);
            exit(-1);
        }
        *pos = 0;
        const char* key = value;
        const char* val = pos + 1;
        if (!z_config_insert_json5(config, key, val, NULL)) {
            printf("Couldn't insert value `%s` in configuration at `%s`\n", val, key);
            exit(-1);
        }
        _Z_PARSE_ARG_SINGLE_OPT(value, opt_long, (char*), NULL);
    }
}

/**
 * Parse the options common to all examples (-c, -m, -e, -l, --cfg, --no-multicast-scouting) and
 * build the session configuration. On return, `*config` owns a `z_config_t*` (drop with
 * `z_config_drop`, or pass to `z_open` which consumes it).
 */
void parse_zenoh_common_args(const int argc, char** argv, z_config_t** config) {
    // -c, --config: A configuration file.
    const char* config_file;
    _Z_PARSE_ARG(config_file, "c", "config", (const char*), NULL);
    if (config_file) {
        *config = z_config_from_file(config_file, NULL);
        if (!*config) {
            printf("Couldn't read configuration from file `%s`\n", config_file);
            exit(-1);
        }
    } else {
        *config = z_config_default();
    }
    // -m: The Zenoh session mode [default: peer].
    const char* mode;
    _Z_PARSE_ARG(mode, "m", "mode", (const char*), NULL);
    if (mode) {
        size_t buflen = strlen(mode) + 3;  // mode + quotes + nullbyte
        char* buf = (char*)malloc(buflen);
        snprintf(buf, buflen, "'%s'", mode);
        if (!z_config_insert_json5(*config, Z_CONFIG_MODE_KEY, buf, NULL)) {
            printf(
                "Couldn't insert value `%s` at `%s`. Value must be one of: 'client', 'peer' or 'router'\n",
                mode, Z_CONFIG_MODE_KEY);
            free(buf);
            exit(-1);
        }
        free(buf);
    }
    parse_zenoh_json_list_config(argc, argv, "e", "connect", Z_CONFIG_CONNECT_KEY, *config);
    parse_zenoh_json_list_config(argc, argv, "l", "listen", Z_CONFIG_LISTEN_KEY, *config);
    parse_zenoh_json_list_cfg(argc, argv, "cfg", *config);
    bool no_multicast_scouting = _Z_CHECK_FLAG("no-multicast-scouting");
    if (no_multicast_scouting && !z_config_insert_json5(*config, Z_CONFIG_MULTICAST_SCOUTING_KEY, "false", NULL)) {
        printf("Couldn't disable multicast-scouting.\n");
        exit(-1);
    }
}

z_query_target_t parse_query_target(const char* arg) {
    if (strcmp(arg, "BEST_MATCHING") == 0) {
        return BestMatching;
    } else if (strcmp(arg, "ALL") == 0) {
        return All;
    } else if (strcmp(arg, "ALL_COMPLETE") == 0) {
        return AllComplete;
    } else {
        printf("Unsupported query target value [%s]\n", arg);
        exit(-1);
    }
}

z_priority_t parse_priority(const char* arg) {
    int p = atoi(arg);
    if (p < RealTime || p > Background) {
        printf("Unsupported priority value [%s]\n", arg);
        exit(-1);
    }
    return (z_priority_t)p;
}
