use anyhow::Result;
use embedded_svc::http::Method;
use embedded_svc::http::client::Client;
use embedded_svc::wifi::{AuthMethod, ClientConfiguration, Configuration as WifiConfiguration};
use esp_idf_svc::eventloop::EspSystemEventLoop;
use esp_idf_svc::hal::peripherals::Peripherals;
use esp_idf_svc::http::client::{Configuration as HttpConfig, EspHttpConnection};
use esp_idf_svc::nvs::EspDefaultNvsPartition;
use esp_idf_svc::wifi::{BlockingWifi, EspWifi};
use log::info;
use std::thread::sleep;
use std::time::Duration;

#[path = "../wifi.local.rs"]
mod wifi_local;

#[derive(Debug)]
struct WeatherSample {
    temp_f: f32,
    feels_like_f: f32,
    humidity: i32,
    wind_mph: f32,
    wind_deg: i32,
    pressure_hpa: f32,
    condition: String,
}

fn main() -> Result<()> {
    esp_idf_sys::link_patches();
    // Use ESP-IDF logger so output goes to UART reliably.
    esp_idf_svc::log::EspLogger::initialize_default();

    info!("BOOT OK (waveshare_s3_3p)");
    info!("Wi-Fi connect + weather fetch start");

    let peripherals = Peripherals::take()?;
    let sysloop = EspSystemEventLoop::take()?;
    let nvs = EspDefaultNvsPartition::take()?;

    let mut wifi = BlockingWifi::wrap(
        EspWifi::new(peripherals.modem, sysloop.clone(), Some(nvs))?,
        sysloop,
    )?;

    connect_wifi(&mut wifi)?;

    match fetch_weather() {
        Some(sample) => {
            info!(
                "WEATHER {} temp {:.1}F feels {:.1}F hum {}% wind {:.1} mph {}deg press {:.1}hPa",
                sample.condition,
                sample.temp_f,
                sample.feels_like_f,
                sample.humidity,
                sample.wind_mph,
                sample.wind_deg,
                sample.pressure_hpa
            );
        }
        None => info!("WEATHER fetch failed"),
    }

    loop {
        info!("alive");
        sleep(Duration::from_secs(30));
    }
}

fn connect_wifi(wifi: &mut BlockingWifi<EspWifi<'static>>) -> Result<()> {
    let wifi_config = WifiConfiguration::Client(ClientConfiguration {
        ssid: wifi_local::WIFI_SSID.try_into().unwrap(),
        password: wifi_local::WIFI_PASS.try_into().unwrap(),
        auth_method: AuthMethod::WPA2Personal,
        ..Default::default()
    });

    wifi.set_configuration(&wifi_config)?;
    wifi.start()?;
    info!("Wi-Fi started, connecting to {}", wifi_local::WIFI_SSID);
    wifi.connect()?;
    wifi.wait_netif_up()?;

    if let Ok(ip_info) = wifi.wifi().sta_netif().get_ip_info() {
        info!("Wi-Fi connected, IP info: {:?}", ip_info);
    }
    Ok(())
}

fn fetch_weather() -> Option<WeatherSample> {
    let url = format!(
        "http://api.openweathermap.org/data/2.5/weather?zip=63301,us&appid={}&units=imperial",
        wifi_local::OPENWEATHER_API_KEY
    );

    let conn = EspHttpConnection::new(&HttpConfig {
        timeout: Some(Duration::from_secs(8)),
        ..Default::default()
    })
    .ok()?;
    let mut client = Client::wrap(conn);

    let headers = [("accept", "application/json")];
    let request = client.request(Method::Get, &url, &headers).ok()?;
    let mut response = request.submit().ok()?;

    let status = response.status();
    if status < 200 || status >= 300 {
        info!("HTTP status {}", status);
        return None;
    }

    let mut body = Vec::new();
    let mut buf = [0u8; 512];
    loop {
        match response.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => body.extend_from_slice(&buf[..n]),
            Err(_) => break,
        }
    }

    let body = String::from_utf8(body).ok()?;
    parse_weather_response(&body)
}

fn parse_weather_response(body: &str) -> Option<WeatherSample> {
    let temp_f = parse_number_after(body, "\"temp\":")?;
    let feels_like_f = parse_number_after(body, "\"feels_like\":")?;
    let humidity = parse_number_after(body, "\"humidity\":")? as i32;
    let wind_mph = parse_number_after(body, "\"speed\":")?;
    let wind_deg = parse_number_after(body, "\"deg\":")? as i32;
    let pressure_hpa = parse_number_after(body, "\"pressure\":")?;
    let condition = parse_string_after(find_after(body, "\"weather\"")?, "\"main\":\"")?;

    Some(WeatherSample {
        temp_f,
        feels_like_f,
        humidity,
        wind_mph,
        wind_deg,
        pressure_hpa,
        condition,
    })
}

fn parse_number_after(text: &str, key: &str) -> Option<f32> {
    let rest = find_after(text, key)?;
    parse_number(rest)
}

fn parse_number(text: &str) -> Option<f32> {
    let mut end = 0;
    for (idx, ch) in text.char_indices() {
        if ch.is_ascii_digit() || ch == '-' || ch == '.' {
            end = idx + ch.len_utf8();
        } else if end > 0 {
            break;
        } else {
            break;
        }
    }
    if end == 0 {
        return None;
    }
    text[..end].parse::<f32>().ok()
}

fn parse_string_after(text: &str, key: &str) -> Option<String> {
    let rest = find_after(text, key)?;
    let end = rest.find('"')?;
    Some(rest[..end].to_string())
}

fn find_after<'a>(text: &'a str, key: &str) -> Option<&'a str> {
    text.find(key).map(|idx| &text[idx + key.len()..])
}
