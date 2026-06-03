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
// Throughput subscriber: count samples in fixed-size rounds and print msg/s for
// each. zenoh-flat has no `z_clock_*` helpers, so we time with the libc
// monotonic clock. The flat sample closure carries both the per-sample `call`
// and a `drop` (used here for `drop_stats`), so a single closure suffices.
//
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "parse_args.h"
#include "zenoh_flat.h"

#define DEFAULT_MEASUREMENTS 10
#define DEFAULT_MESSAGES 1000000

typedef struct {
    unsigned long samples;       // -s, --samples
    unsigned long num_messages;  // -n, --number
} args_t;

args_t parse_args(int argc, char** argv, z_config_t** config);

typedef struct {
    volatile unsigned long count;
    volatile unsigned long finished_rounds;
    struct timespec start;
    struct timespec first_start;
    bool started;
    unsigned long max_rounds;
    unsigned long messages_per_round;
} z_stats_t;

static double elapsed_ms(const struct timespec* from) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - from->tv_sec) * 1000.0 + (now.tv_nsec - from->tv_nsec) / 1.0e6;
}
static double elapsed_s(const struct timespec* from) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - from->tv_sec) + (now.tv_nsec - from->tv_nsec) / 1.0e9;
}

z_stats_t* z_stats_make(unsigned long max_rounds, unsigned long messages_per_round) {
    z_stats_t* stats = (z_stats_t*)malloc(sizeof(z_stats_t));
    stats->count = 0;
    stats->finished_rounds = 0;
    stats->started = false;
    stats->max_rounds = max_rounds;
    stats->messages_per_round = messages_per_round;
    return stats;
}

// The sample handler receives an owned `z_sample_t*`; the caller drops it after
// this returns (no `z_sample_drop` needed here).
void on_sample(z_sample_t* sample, void* context) {
    (void)sample;
    z_stats_t* stats = (z_stats_t*)context;
    if (stats->count == 0) {
        clock_gettime(CLOCK_MONOTONIC, &stats->start);
        if (!stats->started) {
            stats->first_start = stats->start;
            stats->started = true;
        }
        stats->count++;
    } else if (stats->count < stats->messages_per_round) {
        stats->count++;
    } else {
        stats->finished_rounds++;
        printf("%f msg/s\n", 1000.0 * stats->messages_per_round / elapsed_ms(&stats->start));
        stats->count = 0;
        if (stats->finished_rounds > stats->max_rounds) {
            exit(0);
        }
    }
    // No z_sample_drop: the caller drops the sample after this returns.
}

// Fired when the subscriber is dropped (clean shutdown).
void drop_stats(void* context) {
    const z_stats_t* stats = (z_stats_t*)context;
    const unsigned long received = stats->messages_per_round * stats->finished_rounds + stats->count;
    double secs = elapsed_s(&stats->first_start);
    printf("Stats being dropped after unsubscribing: received %ld messages over %f seconds (%f msg/s)\n", received,
           secs, secs > 0 ? (double)received / secs : 0.0);
    free(context);
}

int main(int argc, char** argv) {
    z_init_zenoh_logs_from_env_or("error");

    z_config_t* config = NULL;
    args_t args = parse_args(argc, argv, &config);

    z_session_t* s = z_open(config, NULL);  // consumes the config
    if (!s) {
        printf("Unable to open session!\n");
        return -1;
    }

    z_keyexpr_t* ke = z_keyexpr_try_from("test/thr", NULL);

    z_stats_t* context = z_stats_make(args.samples, args.num_messages);
    // The flat sample closure carries the per-sample `call` AND a `drop`
    // (invoked when the subscriber is dropped) — so on_sample + drop_stats fit
    // in one closure. `on_close` is unused here.
    z_closure_sample_t callback = {context, on_sample, drop_stats};
    z_closure_drop_t closer = {NULL, NULL, NULL};
    // `declare_subscriber` CONSUMES the key expression.
    z_subscriber_t* sub = z_session_declare_subscriber(s, ke, callback, closer, NULL);
    if (!sub) {
        printf("Unable to create subscriber.\n");
        z_session_drop(s);
        return -1;
    }

    printf("Press CTRL-C to quit...\n");
    while (1) {
        sleep(1);
    }

    z_subscriber_drop(sub);  // fires drop_stats
    z_session_drop(s);
    return 0;
}

void print_help() {
    printf(
        "\
    Usage: z_sub_thr [OPTIONS]\n\n\
    Options:\n\
        -s, --samples <MEASUREMENTS> (optional, number, default='%d'): Number of throughput measurements.\n\
        -n, --number <NUM_MESSAGES> (optional, number, default='%d'): Number of messages in each measurement.\n",
        DEFAULT_MEASUREMENTS, DEFAULT_MESSAGES);
    printf(COMMON_HELP);
}

args_t parse_args(int argc, char** argv, z_config_t** config) {
    _Z_CHECK_HELP;
    args_t args;
    _Z_PARSE_ARG(args.samples, "s", "samples", atoi, DEFAULT_MEASUREMENTS);
    _Z_PARSE_ARG(args.num_messages, "n", "number", atoi, DEFAULT_MESSAGES);

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
