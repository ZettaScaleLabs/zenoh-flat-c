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
#include <unistd.h>

#include "zenoh_flat.h"

const char* whatami_to_str(z_whatami_t w) {
    switch (w) {
        case Router:
            return "Router";
        case Peer:
            return "Peer";
        case Client:
            return "Client";
        default:
            return "Other";
    }
}

// The hello handler receives an owned `z_hello_t*`.
void hello_handler(z_hello_t* hello, void* context) {
    (*(int*)context)++;

    z_zenoh_id_t zid = z_hello_zid(hello);  // by value (plain data)
    char* zid_str = z_zenoh_id_to_string(&zid);

    printf("Hello { zid: %s, whatami: %s, locators: [", zid_str, whatami_to_str(z_hello_whatami(hello)));

    uintptr_t n = 0;
    char** locators = z_hello_locators(hello, &n);  // array of owned strings
    for (uintptr_t i = 0; i < n; i++) {
        printf("%s%s", i ? ", " : "", locators[i]);
    }
    printf("] }\n");

    z_free_array(locators, n, z_free);  // free each locator string + the block
    z_free(zid_str);
    z_hello_drop(hello);
}

void on_close(void* context) {
    int count = *(int*)context;
    if (!count) {
        printf("Did not find any zenoh process.\n");
    }
}

int main(void) {
    z_init_zenoh_logs_from_env_or("error");

    int count = 0;
    z_config_t* config = z_config_default();

    z_closure_hello_t callback = {&count, hello_handler, NULL};
    z_closure_drop_t closer = {&count, on_close, NULL};

    printf("Scouting...\n");
    // whatami bitfield 7 = Router | Peer | Client. The config is BORROWED (scout
    // clones it internally), so it can be dropped right after.
    z_scout_t* scout = z_scout(Router | Peer | Client, config, callback, closer, NULL);
    z_config_drop(config);
    if (!scout) {
        printf("Unable to start scouting.\n");
        return -1;
    }

    sleep(1);
    z_scout_drop(scout);  // stops scouting and fires `on_close`
    return 0;
}
