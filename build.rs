//! Build script for the zenoh-flat C layer.
//!
//! prebindgen part: read the `#[prebindgen]` items collected by `zenoh-flat`
//! and run them through the `lang::Cbindgen` adapter to produce a Rust file of
//! `extern "C"` wrappers. cbindgen part: turn that Rust file into a C header.
//!
//! Currently scaffolding: the adapter declares nothing, so the generated file
//! is empty and the produced library exports no symbols yet.

use std::path::PathBuf;

fn main() {
    let bindings_file = generate_flat_bindings();
    generate_c_headers(&bindings_file);
}

/// Generate the Rust FFI bindings file from `zenoh-flat`'s prebindgen output.
fn generate_flat_bindings() -> PathBuf {
    // Reader over the data emitted by zenoh-flat's `#[prebindgen]` macro.
    let source = prebindgen::Source::new(zenoh_flat::PREBINDGEN_OUT_DIR);

    // C / cbindgen adapter. First declared function: `z_keyexpr_try_from`.
    let cbindgen = prebindgen::lang::Cbindgen::new()
        .source_module(syn::parse_quote!(zenoh_flat))
        .opaque_named(syn::parse_quote!(ZKeyExpr), "z_keyexpr")
        .data_struct_named(syn::parse_quote!(Error), "z_error")
        .function(syn::parse_quote!(z_keyexpr_try_from));

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
