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
#include "zenoh_flat.h"

#undef NDEBUG
#include <assert.h>

// Open and drop a session built from the default configuration. `z_open`
// consumes the config; `z_session_drop` performs the real close.
void test_open_drop(void) {
    z_config_t* config = z_config_default();
    assert(config != NULL);
    z_session_t* s = z_open(config, NULL);  // consumes config
    assert(s != NULL);
    z_session_drop(s);
}

int main(void) {
    z_init_zenoh_logs_from_env_or("error");
    test_open_drop();
    test_open_drop();  // a second session opens cleanly after the first is dropped
    return 0;
}
