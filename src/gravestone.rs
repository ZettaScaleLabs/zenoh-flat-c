//! Hand-written [`prebindgen::Gravestone`] *logic* for the inline-by-value
//! (`value_opaque`) opaque counterpart types.
//!
//! Only the type-specific logic lives here: what an empty value is, and how to
//! detect it. The opaque struct (`crate::z_zbytes_t`) and its
//! [`prebindgen::Transmute`] impl (the unsafe rust<->opaque glue) are generated
//! into the bindings; the opaque-side `gravestone()`/`is_gravestone()` are
//! autoprovided via `Transmute`. So no `unsafe`/transmute is needed here.
//!
//! For `ZZBytes`, an *empty* `ZBytes` is the gravestone — cheaply constructed and
//! safe to drop — mirroring zenoh-c.

impl ::prebindgen::Gravestone for crate::z_zbytes_t {
    fn rust_gravestone() -> zenoh_flat::ZZBytes {
        zenoh_flat::ZZBytes::default()
    }

    fn rust_is_gravestone(rust: &zenoh_flat::ZZBytes) -> bool {
        rust.is_empty()
    }
}
