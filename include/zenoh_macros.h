//
// zenoh-c-compatible generic macros (`z_loan`, `z_drop`, `z_move`, `z_closure`,
// …) dispatched with C11 `_Generic` over the wrapper's covered types.
//
#ifndef ZENOH_COMPAT_MACROS_H
#define ZENOH_COMPAT_MACROS_H

#include "zenoh_commons.h"

// z_loan(x): owned/view value → const z_loaned_*_t* (borrow).
#define z_loan(this_)                                  \
    _Generic((this_),                                  \
        z_owned_session_t: z_session_loan,             \
        z_owned_config_t: z_config_loan,               \
        z_owned_keyexpr_t: z_keyexpr_loan,             \
        z_owned_bytes_t: z_bytes_loan,                 \
        z_owned_encoding_t: z_encoding_loan,           \
        z_owned_subscriber_t: z_subscriber_loan,       \
        z_owned_publisher_t: z_publisher_loan,         \
        z_owned_queryable_t: z_queryable_loan,         \
        z_owned_querier_t: z_querier_loan,             \
        z_owned_sample_t: z_sample_loan,               \
        z_owned_reply_t: z_reply_loan,                 \
        z_owned_query_t: z_query_loan,                 \
        z_owned_hello_t: z_hello_loan,                 \
        z_owned_string_t: z_string_loan,               \
        z_owned_string_array_t: z_string_array_loan,   \
        z_view_keyexpr_t: z_view_keyexpr_loan,         \
        z_view_string_t: z_view_string_loan)(&(this_))

// z_loan_mut(x): owned value → z_loaned_*_t* (mutable borrow).
#define z_loan_mut(this_) \
    _Generic((this_), z_owned_config_t: z_config_loan_mut, z_owned_session_t: z_session_loan_mut)(&(this_))

// z_move(x): owned value → z_moved_*_t* (transfer).
#define z_move(this_)                                                \
    _Generic((this_),                                                \
        z_owned_session_t: z_session_move,                           \
        z_owned_config_t: z_config_move,                             \
        z_owned_keyexpr_t: z_keyexpr_move,                           \
        z_owned_bytes_t: z_bytes_move,                               \
        z_owned_encoding_t: z_encoding_move,                         \
        z_owned_subscriber_t: z_subscriber_move,                     \
        z_owned_publisher_t: z_publisher_move,                       \
        z_owned_queryable_t: z_queryable_move,                       \
        z_owned_querier_t: z_querier_move,                           \
        z_owned_string_t: z_string_move,                             \
        z_owned_string_array_t: z_string_array_move,                 \
        z_owned_closure_sample_t: z_closure_sample_move,             \
        z_owned_closure_query_t: z_closure_query_move,               \
        z_owned_closure_reply_t: z_closure_reply_move,               \
        z_owned_closure_hello_t: z_closure_hello_move,               \
        z_owned_closure_zid_t: z_closure_zid_move,                   \
        z_owned_closure_matching_status_t: z_closure_matching_status_move)(&(this_))

// z_drop(m): z_moved_*_t* → release.
#define z_drop(this_)                                       \
    _Generic((this_),                                       \
        z_moved_session_t*: z_session_drop,                 \
        z_moved_config_t*: z_config_drop,                   \
        z_moved_keyexpr_t*: z_keyexpr_drop,                 \
        z_moved_bytes_t*: z_bytes_drop,                     \
        z_moved_encoding_t*: z_encoding_drop,               \
        z_moved_subscriber_t*: z_subscriber_drop,           \
        z_moved_publisher_t*: z_publisher_drop,             \
        z_moved_queryable_t*: z_queryable_drop,             \
        z_moved_querier_t*: z_querier_drop,                 \
        z_moved_string_t*: z_string_drop,                   \
        z_moved_string_array_t*: z_string_array_drop)(this_)

// z_closure(&c, call, drop, ctx): initialize a closure.
#define z_closure(this_, call, drop, context)                 \
    _Generic((this_),                                         \
        z_owned_closure_sample_t*: z_closure_sample,          \
        z_owned_closure_query_t*: z_closure_query,            \
        z_owned_closure_reply_t*: z_closure_reply,            \
        z_owned_closure_hello_t*: z_closure_hello,            \
        z_owned_closure_zid_t*: z_closure_zid,                \
        z_owned_closure_matching_status_t*: z_closure_matching_status)(this_, call, drop, context)

#endif  // ZENOH_COMPAT_MACROS_H
