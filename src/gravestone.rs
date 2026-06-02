//! Hand-written [`prebindgen::Gravestone`] impls for the inline-by-value
//! (`value_opaque`) opaque counterpart types.
//!
//! The opaque struct itself (`crate::z_zbytes_t`, a `#[repr(C, align)]` byte
//! buffer of the same size/alignment as `zenoh_flat::ZZBytes`) is probe-generated
//! at build time and injected into the generated bindings. The gravestone — the
//! empty / moved-from state written back on consume so a later `_drop` is a no-op
//! — is type-specific knowledge, so it lives here in user code.
//!
//! For `ZZBytes`, an *empty* `ZBytes` is the gravestone (cheaply constructed and
//! safe to drop), mirroring zenoh-c. `transmute_copy` reinterprets its bytes as
//! the opaque buffer (sizes match — the `value_opaque` converters assert it
//! fail-closed); `is_gravestone` reinterprets the buffer back as a `ZBytes` and
//! checks emptiness. (The legality is orphan-rule safe because `z_zbytes_t` is a
//! local type.)

impl ::prebindgen::Gravestone for crate::z_zbytes_t {
    fn gravestone() -> Self {
        unsafe {
            ::core::mem::transmute_copy(&::core::mem::ManuallyDrop::new(
                <zenoh_flat::ZZBytes as ::core::default::Default>::default(),
            ))
        }
    }

    fn is_gravestone(&self) -> bool {
        unsafe { (*(self as *const Self as *const zenoh_flat::ZZBytes)).is_empty() }
    }
}
