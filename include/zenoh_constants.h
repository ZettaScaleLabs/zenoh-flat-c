//
// zenoh-c-compatible constants, layered over zenoh-flat-c (`z_flat_*`).
//
#ifndef ZENOH_COMPAT_CONSTANTS_H
#define ZENOH_COMPAT_CONSTANTS_H

#include "zenoh_flat.h"

// ── z_result_t codes ───────────────────────────────────────────────────────
typedef int8_t z_result_t;
#define Z_OK 0
#define Z_EINVAL -1
#define Z_EPARSE -2
#define Z_EIO -3
#define Z_ENETWORK -4
#define Z_ENULL -5
#define Z_EUNAVAILABLE -6
#define Z_EDESERIALIZE -7
#define Z_ESESSION_CLOSED -8
#define Z_EUTF8 -9
#define Z_EGENERIC ((z_result_t)-128)

// ── Enum aliases + value names ─────────────────────────────────────────────
// The underlying enums come from `zenoh_flat.h`; here we only add the
// `Z_*`-style value spellings the zenoh-c examples use.
typedef z_flat_sample_kind_t z_sample_kind_t;
#define Z_SAMPLE_KIND_PUT Put
#define Z_SAMPLE_KIND_DELETE Delete

typedef z_flat_priority_t z_priority_t;
#define Z_PRIORITY_REAL_TIME RealTime
#define Z_PRIORITY_INTERACTIVE_HIGH InteractiveHigh
#define Z_PRIORITY_INTERACTIVE_LOW InteractiveLow
#define Z_PRIORITY_DATA_HIGH DataHigh
#define Z_PRIORITY_DATA Data
#define Z_PRIORITY_DATA_LOW DataLow
#define Z_PRIORITY_BACKGROUND Background

typedef z_flat_congestion_control_t z_congestion_control_t;
#define Z_CONGESTION_CONTROL_DROP Drop
#define Z_CONGESTION_CONTROL_BLOCK Block

typedef z_flat_reliability_t z_reliability_t;
#define Z_RELIABILITY_BEST_EFFORT BestEffort
#define Z_RELIABILITY_RELIABLE Reliable

typedef z_flat_query_target_t z_query_target_t;
#define Z_QUERY_TARGET_BEST_MATCHING BestMatching
#define Z_QUERY_TARGET_ALL All
#define Z_QUERY_TARGET_ALL_COMPLETE AllComplete

typedef z_flat_consolidation_mode_t z_consolidation_mode_t;
typedef z_flat_reply_key_expr_t z_reply_keyexpr_t;
typedef z_flat_whatami_t z_whatami_t;
#define Z_WHATAMI_ROUTER Router
#define Z_WHATAMI_PEER Peer
#define Z_WHATAMI_CLIENT Client

// ── Config keys (zenoh JSON5 paths) ────────────────────────────────────────
#define Z_CONFIG_MODE_KEY "mode"
#define Z_CONFIG_CONNECT_KEY "connect/endpoints"
#define Z_CONFIG_LISTEN_KEY "listen/endpoints"
#define Z_CONFIG_MULTICAST_SCOUTING_KEY "scouting/multicast/enabled"

#endif  // ZENOH_COMPAT_CONSTANTS_H
