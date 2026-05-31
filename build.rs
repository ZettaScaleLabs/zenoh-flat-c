//! Build script for the zenoh-flat C layer.
//!
//! prebindgen part: read the `#[prebindgen]` items collected by `zenoh-flat`
//! and run them through the `lang::Cbindgen` adapter to produce a Rust file of
//! `extern "C"` wrappers. cbindgen part: turn that Rust file into a C header.

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
    let mut cbindgen = prebindgen::lang::Cbindgen::new()
        .source_module(pq!(zenoh_flat))
        // Single universal raw-memory freer for the `char*` data the layer hands
        // out (string returns + data-struct `String` fields). Opaque handles keep
        // their own typed `*_drop`.
        .free_memory_function("z_free");

    for (ty, name, drop_name) in [
        (pq!(ZKeyExpr), "z_keyexpr_t", "z_keyexpr_drop"),
        (pq!(ZConfig), "z_config_t", "z_config_drop"),
        (pq!(ZZenohId), "z_zenoh_id_t", "z_zenoh_id_drop"),
        (pq!(ZHello), "z_hello_t", "z_hello_drop"),
        (pq!(ZZBytes), "z_zbytes_t", "z_zbytes_drop"),
        (pq!(ZEncoding), "z_encoding_t", "z_encoding_drop"),
        (pq!(ZSession), "z_session_t", "z_session_drop"),
        (pq!(ZReply), "z_reply_t", "z_reply_drop"),
        (pq!(ZSample), "z_sample_t", "z_sample_drop"),
        (pq!(ZTimestamp), "z_timestamp_t", "z_timestamp_drop"),
        (pq!(ZPublisher), "z_publisher_t", "z_publisher_drop"),
        (pq!(ZQuerier), "z_querier_t", "z_querier_drop"),
        (pq!(ZQuery), "z_query_t", "z_query_drop"),
        (pq!(ZLivelinessToken), "z_liveliness_token_t", "z_liveliness_token_drop"),
    ] {
        cbindgen = cbindgen.ptr_struct(ty).name(name).destructor_name(drop_name);
    }

    cbindgen = cbindgen.data_struct(pq!(Error)).name("z_error_t").error();

    for (ty, name) in [
        (pq!(SetIntersectionLevel), "z_set_intersection_level_t"),
        (pq!(WhatAmI), "z_whatami_t"),
        (pq!(CongestionControl), "z_congestion_control_t"),
        (pq!(Priority), "z_priority_t"),
        (pq!(Reliability), "z_reliability_t"),
        (pq!(ConsolidationMode), "z_consolidation_mode_t"),
        (pq!(QueryTarget), "z_query_target_t"),
        (pq!(ReplyKeyExpr), "z_reply_key_expr_t"),
        (pq!(SampleKind), "z_sample_kind_t"),
    ] {
        cbindgen = cbindgen.enum_type(ty).name(name);
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
        pq!(zenoh_id_to_string),
    ] {
        cbindgen = cbindgen.ignore_function(function);
    }

    // The C layer intentionally stays on the simple callback-free Z-handle
    // surface. Callback APIs and the generic/value-layer `impl Into(...)`
    // wrappers remain out of scope for this step.
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
        // Explicit, callback-free operations whose concrete signatures need
        // neither `Option<T>` nor `Vec<u8>` on the wire (the two type classes
        // `lang::Cbindgen` does not yet marshal). All take concrete handles
        // (`&z_keyexpr_t`, `z_zbytes_t`, `&z_encoding_t`) — no `impl Into<…>`.
        // The remaining explicit functions (put/delete/reply_success/
        // reply_delete + the z_zbytes byte builders) are blocked on adapter
        // support for `Option<T>` and `Vec<u8>` and stay undeclared for now.
        pq!(z_session_declare_publisher),
        pq!(z_session_declare_querier),
        pq!(z_query_reply_error),
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
    ] {
        cbindgen = cbindgen.function(function).panic();
    }

    // Logger helpers. Keep the public C ABI on the existing `z_*` prefix.
    cbindgen = cbindgen
        .function(pq!(init_android_logs))
        .name("z_init_android_logs")
        .panic()
        .function(pq!(try_init_zenoh_logs_from_env))
        .name("z_try_init_zenoh_logs_from_env")
        .function(pq!(init_zenoh_logs_from_env_or))
        .name("z_init_zenoh_logs_from_env_or")
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
