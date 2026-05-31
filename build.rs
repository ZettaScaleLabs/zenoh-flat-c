//! Build script for the zenoh-flat C layer.
//!
//! prebindgen part: read the `#[prebindgen]` items collected by `zenoh-flat`
//! and run them through the `lang::Cbindgen` adapter to produce a Rust file of
//! `extern "C"` wrappers. cbindgen part: turn that Rust file into a C header.
//!
//! Currently scaffolding: the adapter declares nothing, so the generated file
//! is empty and the produced library exports no symbols yet.

use std::path::PathBuf;
use syn::parse_quote as pq;

fn main() {
    let bindings_file = generate_flat_bindings();
    generate_c_headers(&bindings_file);
}

/// Generate the Rust FFI bindings file from `zenoh-flat`'s prebindgen output.
fn generate_flat_bindings() -> PathBuf {
    // Reader over the data emitted by zenoh-flat's `#[prebindgen]` macro.
    let source = prebindgen::Source::new(zenoh_flat::PREBINDGEN_OUT_DIR);

    // C / cbindgen adapter.
    //
    // We wrap only the `z_keyexpr_*` family — the low-level functions that
    // operate directly on the native `ZKeyExpr` handle and already have plain,
    // FFI-able signatures (`&ZKeyExpr`, `String`, `bool`, `Result<ZKeyExpr,
    // Error>`, `SetIntersectionLevel`). For C this is the right granularity: a
    // C caller holds the opaque `z_keyexpr_t` and crossing the boundary is cheap.
    //
    // We deliberately do NOT wrap the high-level `KeyExpr` struct or its
    // `keyexpr_*` functions. Those use `impl Into<KeyExpr>` generics (not
    // `extern "C"`-able) and a string-caching struct designed for managed
    // languages like Java/Kotlin, where every native-boundary crossing is
    // expensive: there the key expression is held string-side and the native
    // `ZKeyExpr` is materialized lazily. That layer belongs to the JNI adapter,
    // not the C one.
    let cbindgen = prebindgen::lang::Cbindgen::new()
        .source_module(pq!(zenoh_flat))
        .ptr_struct(pq!(ZKeyExpr))
        .name("z_keyexpr_t")
        .destructor_name("z_keyexpr_drop")
        .data_struct(pq!(Error))
        .name("z_error_t")
        .error()
        .enum_type(pq!(SetIntersectionLevel)).name("z_set_intersection_level_t")
        // Fallible constructors: `Result<ZKeyExpr, Error>`.
        .function(pq!(z_keyexpr_try_from))
        .function(pq!(z_keyexpr_autocanonize))
        // Predicates over borrowed handles returning `bool`. They have fallible
        // (null-checked) borrow inputs but no `Result` channel, so `.panic()`
        // lets the wrapper abort on a null handle.
        .function(pq!(z_keyexpr_intersects)).panic()
        .function(pq!(z_keyexpr_includes)).panic()
        // Borrowed-handle relation returning the `SetIntersectionLevel` enum —
        // also `.panic()` (fallible borrows, no `Result`).
        .function(pq!(z_keyexpr_relation_to)).panic()
        // Builders combining a borrowed handle with a `String` → `Result`.
        .function(pq!(z_keyexpr_join))
        .function(pq!(z_keyexpr_concat));

    let mut registry =
        prebindgen::core::Registry::from_items(source.items_all()).expect("scan prebindgen items");

    let bindings_file = registry
        .write_rust(&cbindgen, "zenoh_flat.rs")
        .expect("write generated bindings");

    println!(
        "cargo:warning=Generated bindings at: {}",
        bindings_file.display()
    );

    bindings_file
}

/// Generate the C header from the prebindgen-generated Rust file via cbindgen.
fn generate_c_headers(bindings_file: &PathBuf) {
    let crate_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let config = cbindgen::Config::from_root_or_default(&crate_dir);

    let header_path = PathBuf::from(&out_dir).join("zenoh_flat.h");

    match cbindgen::Builder::new()
        .with_config(config)
        .with_crate(&crate_dir)
        .with_src(bindings_file)
        .generate()
    {
        Ok(bindings) => {
            bindings.write_to_file(&header_path);
            println!(
                "cargo:warning=Generated C headers at: {}",
                header_path.display()
            );
        }
        Err(e) => {
            println!("cargo:warning=Failed to generate C headers: {e:?}");
        }
    }
}
