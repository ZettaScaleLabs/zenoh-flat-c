//! Auto-generated C layer for `zenoh-flat`.
//!
//! The body of this crate is the Rust file produced at build time by the
//! prebindgen `lang::Cbindgen` adapter (see `build.rs`). cbindgen turns those
//! `extern "C"` wrappers into a C header.

include!(concat!(env!("OUT_DIR"), "/zenoh_flat.rs"));

// Hand-written `Gravestone` impls for the inline-by-value opaque types defined
// (probe-sized) in the generated file above.
mod gravestone;
