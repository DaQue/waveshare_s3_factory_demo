fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=wifi.local.rs");
    emit_local_secrets_from_wifi_local();
    embuild::espidf::sysenv::output();
}

fn emit_local_secrets_from_wifi_local() {
    let path = std::path::Path::new("wifi.local.rs");
    let Ok(src) = std::fs::read_to_string(path) else {
        return;
    };

    if let Some(v) = extract_rust_str_const(&src, "WIFI_SSID") {
        println!("cargo:rustc-env=LOCAL_WIFI_SSID={}", v);
    }
    if let Some(v) = extract_rust_str_const(&src, "WIFI_PASS") {
        println!("cargo:rustc-env=LOCAL_WIFI_PASS={}", v);
    }
    if let Some(v) = extract_rust_str_const(&src, "OPENWEATHER_API_KEY") {
        println!("cargo:rustc-env=LOCAL_OPENWEATHER_API_KEY={}", v);
    }
}

fn extract_rust_str_const(src: &str, name: &str) -> Option<String> {
    for line in src.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("//") {
            continue;
        }
        let needle = format!("pub const {}", name);
        if !trimmed.starts_with(&needle) {
            continue;
        }
        let start = trimmed.find('"')?;
        let end = trimmed[start + 1..].find('"')? + start + 1;
        return Some(trimmed[start + 1..end].to_string());
    }
    None
}
