//! Build script for the zenoh-flat C layer.
//!
//! prebindgen part: read the `#[prebindgen]` items collected by `zenoh-flat`
//! and run them through the `lang::Cbindgen` adapter to produce a Rust file of
//! `extern "C"` wrappers. cbindgen part: turn that Rust file into a C header.

use prebindgen::lang::snake_case;
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
    // We wrap the C-facing low-level APIs that already have clean ownership and
    // error semantics for a native caller:
    // - key expressions via the `ZKeyExpr` opaque handle
    // - configs via the `ZConfig` opaque handle
    // - logger initialization helpers
    //
    // We deliberately do NOT wrap the high-level `KeyExpr` struct or its
    // `keyexpr_*` functions. Those use `impl Into<KeyExpr>` generics (not
    // `extern "C"`-able) and a string-caching struct designed for managed
    // languages like Java/Kotlin, where every native-boundary crossing is
    // expensive: there the key expression is held string-side and the native
    // `ZKeyExpr` is materialized lazily. That layer belongs to the JNI adapter,
    // not the C one.
    // Name-mangling rules: every C-facing name (types, destructors, callback
    // structs, function symbols) is generated from the Rust type/function, so the
    // declarations below carry no per-item `.name(...)`. The zenoh `z_…_t` / `z_…`
    // convention lives here, in this consumer — `lang::Cbindgen` stays universal.
    let mut cbindgen = prebindgen::lang::Cbindgen::new()
        .source_module(pq!(zenoh_flat))
        // Single universal raw-memory freer for the `char*` data the layer hands
        // out (string returns + data-struct `String` fields). Opaque handles keep
        // their own typed `*_drop`.
        .free_memory_function("z_flat_free")
        // Base: strip the `Z` opaque-handle prefix, then `snake_case`, with the
        // few irregulars fixed in this one place (`KeyExpr`→`keyexpr` etc.).
        .mangle_rust_type(|short| {
            let s = short.strip_prefix('Z').unwrap_or(short);
            match s {
                "KeyExpr" => "keyexpr".to_string(),
                "ZBytes" => "zbytes".to_string(),
                "WhatAmI" => "whatami".to_string(),
                other => snake_case(other),
            }
        })
        // All C-facing names carry the `z_flat_` prefix so the plain `z_` namespace
        // is free for a separate zenoh-c-compatible wrapper layered on top.
        .mangle_type_name(|base| format!("z_flat_{base}_t"))
        .mangle_destructor(|base| format!("z_flat_{base}_drop"))
        .mangle_callback(|bases| {
            if bases.is_empty() {
                "z_flat_closure_drop_t".to_string()
            } else {
                format!("z_flat_closure_{}_t", bases.join("_"))
            }
        })
        // Strip any leading `z_` then prefix `z_flat_` (`z_open`→`z_flat_open`,
        // `init_android_logs`→`z_flat_init_android_logs`).
        .mangle_function(|n| {
            let rest = n.strip_prefix("z_").unwrap_or(n);
            format!("z_flat_{rest}")
        });

    for ty in [
        pq!(ZKeyExpr),
        pq!(ZConfig),
        pq!(ZZenohId),
        pq!(ZHello),
        pq!(ZZBytes),
        pq!(ZEncoding),
        pq!(ZSession),
        pq!(ZReply),
        pq!(ZSample),
        pq!(ZTimestamp),
        pq!(ZPublisher),
        pq!(ZQuerier),
        pq!(ZQuery),
        pq!(ZLivelinessToken),
        // Returned by the callback-declaring APIs (subscriber/queryable/scout).
        pq!(ZSubscriber),
        pq!(ZQueryable),
        pq!(ZScout),
    ] {
        cbindgen = cbindgen.ptr_struct(ty);
    }

    cbindgen = cbindgen.data_struct(pq!(Error)).error();

    for ty in [
        pq!(SetIntersectionLevel),
        pq!(WhatAmI),
        pq!(CongestionControl),
        pq!(Priority),
        pq!(Reliability),
        pq!(ConsolidationMode),
        pq!(QueryTarget),
        pq!(ReplyKeyExpr),
        pq!(SampleKind),
    ] {
        cbindgen = cbindgen.enum_type(ty);
    }

    // Callback signatures of the `z_*` opaque-handle tier. Each `impl Fn(handle)`
    // (plus the zero-arg `on_close`) emits a by-value `#[repr(C)]` closure struct
    // `{ context; call; drop }`; the C `call` receives an OWNED handle it must
    // `z_<x>_drop`. Struct names come from the callback mangler (`z_closure_*_t`).
    for ty in [
        pq!(impl Fn(ZSample) + Send + Sync + 'static),
        pq!(impl Fn(ZReply) + Send + Sync + 'static),
        pq!(impl Fn(ZQuery) + Send + Sync + 'static),
        pq!(impl Fn(ZHello) + Send + Sync + 'static),
        pq!(impl Fn() + Send + Sync + 'static),
    ] {
        cbindgen = cbindgen.callback(ty);
    }

    for ty in [
        pq!(Encoding),
        pq!(Hello),
        pq!(KeyExpr),
        pq!(Query),
        pq!(Reply),
        pq!(Sample),
        pq!(Timestamp),
        pq!(ZBytes),
        pq!(ZenohId),
    ] {
        cbindgen = cbindgen.ignore_type(ty);
    }

    for function in [
        pq!(encoding_application_cbor),
        pq!(encoding_application_cdr),
        pq!(encoding_application_coap_payload),
        pq!(encoding_application_java_serialized_object),
        pq!(encoding_application_json),
        pq!(encoding_application_json_patch_json),
        pq!(encoding_application_json_seq),
        pq!(encoding_application_jsonpath),
        pq!(encoding_application_jwt),
        pq!(encoding_application_mp4),
        pq!(encoding_application_octet_stream),
        pq!(encoding_application_openmetrics_text),
        pq!(encoding_application_protobuf),
        pq!(encoding_application_python_serialized_object),
        pq!(encoding_application_soap_xml),
        pq!(encoding_application_sql),
        pq!(encoding_application_x_www_form_urlencoded),
        pq!(encoding_application_xml),
        pq!(encoding_application_yaml),
        pq!(encoding_application_yang),
        pq!(encoding_audio_aac),
        pq!(encoding_audio_flac),
        pq!(encoding_audio_mp4),
        pq!(encoding_audio_ogg),
        pq!(encoding_audio_vorbis),
        pq!(encoding_from_string),
        pq!(encoding_image_bmp),
        pq!(encoding_image_gif),
        pq!(encoding_image_jpeg),
        pq!(encoding_image_png),
        pq!(encoding_image_webp),
        pq!(encoding_text_css),
        pq!(encoding_text_csv),
        pq!(encoding_text_html),
        pq!(encoding_text_javascript),
        pq!(encoding_text_json),
        pq!(encoding_text_json5),
        pq!(encoding_text_markdown),
        pq!(encoding_text_plain),
        pq!(encoding_text_xml),
        pq!(encoding_text_yaml),
        pq!(encoding_to_string),
        pq!(encoding_video_h261),
        pq!(encoding_video_h263),
        pq!(encoding_video_h264),
        pq!(encoding_video_h265),
        pq!(encoding_video_h266),
        pq!(encoding_video_mp4),
        pq!(encoding_video_ogg),
        pq!(encoding_video_raw),
        pq!(encoding_video_vp8),
        pq!(encoding_video_vp9),
        pq!(encoding_zenoh_bytes),
        pq!(encoding_zenoh_serialized),
        pq!(encoding_zenoh_string),
        pq!(keyexpr_autocanonize),
        pq!(keyexpr_concat),
        pq!(keyexpr_includes),
        pq!(keyexpr_intersects),
        pq!(keyexpr_join),
        pq!(keyexpr_relation_to),
        pq!(keyexpr_try_from),
        pq!(liveliness_declare_subscriber),
        pq!(liveliness_declare_token),
        pq!(liveliness_get),
        pq!(publisher_delete),
        pq!(publisher_put),
        pq!(querier_get),
        pq!(query_reply_delete),
        pq!(query_reply_error),
        pq!(query_reply_success),
        pq!(scout),
        pq!(session_declare_publisher),
        pq!(session_declare_querier),
        pq!(session_declare_queryable),
        pq!(session_declare_subscriber),
        pq!(session_delete),
        pq!(session_get),
        pq!(session_put),
        // Value-class zid accessors (return the `ZenohId` data twin) — JNI layer.
        pq!(session_zid),
        pq!(session_peers_zid),
        pq!(session_routers_zid),
        pq!(z_query_expand),
        pq!(z_reply_expand),
        pq!(z_sample_expand),
        pq!(z_timestamp_expand),
        pq!(zenoh_id_to_string),
        pq!(z_zbytes_from_vec),
    ] {
        cbindgen = cbindgen.ignore_function(function);
    }

    // The C layer wraps the `z_*` opaque-handle tier, now including its callback
    // APIs (closure structs). The generic/value-layer `impl Into(...)` wrappers
    // (and value-struct returns) remain out of scope — they belong to the JNI
    // adapter.
    for function in [
        pq!(z_keyexpr_try_from),
        pq!(z_keyexpr_autocanonize),
        pq!(z_keyexpr_join),
        pq!(z_keyexpr_concat),
        pq!(z_config_default),
        pq!(z_config_from_file),
        pq!(z_config_from_json),
        pq!(z_config_from_json5),
        pq!(z_config_from_yaml),
        pq!(z_config_get_json),
        pq!(z_config_insert_json5),
        pq!(z_open),
        pq!(z_session_declare_keyexpr),
        pq!(z_session_undeclare_keyexpr),
        // Explicit, callback-free operations taking concrete handles
        // (`&z_keyexpr_t`, `z_zbytes_t`, `&z_encoding_t`) — no `impl Into<…>`.
        // All return `Result<(), Error>`, so fallible inputs (including the
        // `Option<…>` attachments) route through the error out-param — no
        // `.panic()`. Only the callback APIs and the value-struct/`Vec<ZenohId>`
        // returns remain undeclared.
        pq!(z_zbytes_from_slice),
        pq!(z_session_declare_publisher),
        pq!(z_session_declare_querier),
        pq!(z_query_reply_error),
        // Callback APIs of the `z_*` tier (deliver opaque handles to a C closure
        // struct + zero-arg on-close). All return `Result<_, Error>`, so fallible
        // borrow/`Option` inputs route through the error out-param — no `.panic()`.
        pq!(z_session_declare_subscriber),
        pq!(z_session_declare_queryable),
        pq!(z_session_get),
        pq!(z_scout),
        pq!(z_liveliness_get),
        pq!(z_liveliness_declare_subscriber),
        pq!(z_querier_get),
        // Put/delete/reply ops, unblocked by `Option<T>` input support.
        pq!(z_publisher_put),
        pq!(z_publisher_delete),
        pq!(z_session_put),
        pq!(z_session_delete),
        pq!(z_query_reply_success),
        pq!(z_query_reply_delete),
        pq!(z_liveliness_declare_token),
        pq!(z_encoding_zenoh_bytes),
        pq!(z_encoding_zenoh_string),
        pq!(z_encoding_zenoh_serialized),
        pq!(z_encoding_application_octet_stream),
        pq!(z_encoding_text_plain),
        pq!(z_encoding_application_json),
        pq!(z_encoding_text_json),
        pq!(z_encoding_application_cdr),
        pq!(z_encoding_application_cbor),
        pq!(z_encoding_application_yaml),
        pq!(z_encoding_text_yaml),
        pq!(z_encoding_text_json5),
        pq!(z_encoding_application_python_serialized_object),
        pq!(z_encoding_application_protobuf),
        pq!(z_encoding_application_java_serialized_object),
        pq!(z_encoding_application_openmetrics_text),
        pq!(z_encoding_image_png),
        pq!(z_encoding_image_jpeg),
        pq!(z_encoding_image_gif),
        pq!(z_encoding_image_bmp),
        pq!(z_encoding_image_webp),
        pq!(z_encoding_application_xml),
        pq!(z_encoding_application_x_www_form_urlencoded),
        pq!(z_encoding_text_html),
        pq!(z_encoding_text_xml),
        pq!(z_encoding_text_css),
        pq!(z_encoding_text_javascript),
        pq!(z_encoding_text_markdown),
        pq!(z_encoding_text_csv),
        pq!(z_encoding_application_sql),
        pq!(z_encoding_application_coap_payload),
        pq!(z_encoding_application_json_patch_json),
        pq!(z_encoding_application_json_seq),
        pq!(z_encoding_application_jsonpath),
        pq!(z_encoding_application_jwt),
        pq!(z_encoding_application_mp4),
        pq!(z_encoding_application_soap_xml),
        pq!(z_encoding_application_yang),
        pq!(z_encoding_audio_aac),
        pq!(z_encoding_audio_flac),
        pq!(z_encoding_audio_mp4),
        pq!(z_encoding_audio_ogg),
        pq!(z_encoding_audio_vorbis),
        pq!(z_encoding_video_h261),
        pq!(z_encoding_video_h263),
        pq!(z_encoding_video_h264),
        pq!(z_encoding_video_h265),
        pq!(z_encoding_video_h266),
        pq!(z_encoding_video_mp4),
        pq!(z_encoding_video_ogg),
        pq!(z_encoding_video_raw),
        pq!(z_encoding_video_vp8),
        pq!(z_encoding_video_vp9),
    ] {
        cbindgen = cbindgen.function(function);
    }

    // Predicates over borrowed handles returning `bool`. They have fallible
    // (null-checked) borrow inputs but no `Result` channel, so `.panic()`
    // lets the wrapper abort on a null handle.
    cbindgen = cbindgen.function(pq!(z_keyexpr_intersects)).panic();
    cbindgen = cbindgen.function(pq!(z_keyexpr_includes)).panic();

    // Borrowed-handle relation returning the `SetIntersectionLevel` enum —
    // also `.panic()` (fallible borrows, no `Result`).
    cbindgen = cbindgen.function(pq!(z_keyexpr_relation_to)).panic();

    for function in [
        pq!(z_keyexpr_clone),
        pq!(z_config_clone),
        pq!(z_zenoh_id_to_string),
        pq!(z_hello_whatami),
        pq!(z_hello_zid),
        pq!(z_encoding_id),
        pq!(z_encoding_to_string),
        pq!(z_encoding_from_string),
        pq!(z_timestamp_ntp64),
        pq!(z_reply_replier_eid),
        pq!(z_reply_is_ok),
        pq!(z_sample_key_expr),
        pq!(z_sample_payload),
        pq!(z_sample_encoding),
        pq!(z_sample_kind),
        pq!(z_sample_express),
        pq!(z_sample_priority),
        pq!(z_sample_congestion_control),
        pq!(z_query_keyexpr),     // &ZKeyExpr borrow
        pq!(z_query_parameters),  // owned String → char*
        pq!(z_keyexpr_to_string), // owned String → char*
        pq!(z_encoding_clone),    // owned ZEncoding handle
    ] {
        cbindgen = cbindgen.function(function).panic();
    }

    // `Option<T>` getters: NULL encodes `None` on the wire, so NULL can no
    // longer double as an error channel — their null-checked borrow inputs
    // need `.panic()`. All inner `T` lower to a pointer wire (`String` →
    // `char*`, the rest to declared opaque handles).
    for function in [
        pq!(z_encoding_schema),      // Option<String>
        pq!(z_sample_timestamp),     // Option<ZTimestamp>
        pq!(z_sample_attachment),    // Option<ZZBytes>
        pq!(z_reply_replier_zid),    // Option<ZZenohId>
        pq!(z_reply_sample),         // Option<ZSample>
        pq!(z_reply_error_payload),  // Option<ZZBytes>
        pq!(z_reply_error_encoding), // Option<ZEncoding>
        pq!(z_query_payload),        // Option<&ZZBytes> borrow
        pq!(z_query_encoding),       // Option<&ZEncoding> borrow
    ] {
        cbindgen = cbindgen.function(function).panic();
    }

    // `Vec<T>` getters: lower to a malloc'd array (`T* + size_t`) freed C-side
    // via the `z_free_array` macro. Their null-checked `&handle` borrow inputs
    // have no `Result` channel, so `.panic()`. (`z_zbytes_from_slice`, a `&[u8]`
    // slice input, is infallible and declared in the plain `.function` loop.)
    for function in [
        pq!(z_hello_locators),       // Vec<String>
        pq!(z_zbytes_to_bytes),      // Vec<u8>
        pq!(z_zenoh_id_to_bytes),    // Vec<u8>
        pq!(z_timestamp_id),         // Vec<u8>
        pq!(z_session_peers_zid),    // Vec<ZZenohId>
        pq!(z_session_routers_zid),  // Vec<ZZenohId>
    ] {
        cbindgen = cbindgen.function(function).panic();
    }

    // Opaque-handle zid accessor: returns the `ZZenohId` handle (`&ZSession`
    // borrow is fallible, no `Result`, so `.panic()`).
    cbindgen = cbindgen.function(pq!(z_session_zid)).panic();

    // Logger helpers. The function mangler adds the `z_` prefix (these are the
    // only declared functions not already `z_*`).
    cbindgen = cbindgen
        .function(pq!(init_android_logs))
        .panic()
        .function(pq!(try_init_zenoh_logs_from_env))
        .function(pq!(init_zenoh_logs_from_env_or))
        .panic();

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
            // Also publish the flat header to the in-tree, deterministic
            // `include/` dir so C consumers (and the CMake build) have a stable
            // path. `include/` is the single dual-API directory: this generated
            // `zenoh_flat.h` is the flat API; the committed `zenoh.h` family is
            // the zenoh-c-compatible API layered on top.
            let stable = PathBuf::from(&crate_dir).join("include").join("zenoh_flat.h");
            if let Some(parent) = stable.parent() {
                let _ = std::fs::create_dir_all(parent);
            }
            if let Err(e) = std::fs::copy(&header_path, &stable) {
                println!("cargo:warning=Failed to copy header to include/: {e}");
            }
            println!(
                "cargo:warning=Generated C headers at: {} (and {})",
                header_path.display(),
                stable.display()
            );
        }
        Err(e) => {
            println!("cargo:warning=Failed to generate C headers: {e:?}");
        }
    }
}
