use embedded_graphics::{image::Image, pixelcolor::Rgb565, prelude::*};
use tinybmp::Bmp;

use crate::framebuffer::Framebuffer;

/// Weather icon identifiers mapped to OpenWeatherMap condition codes.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum WeatherIcon {
    #[default]
    Clear = 0,
    FewClouds = 1,
    ScatteredClouds = 2,
    BrokenClouds = 3,
    Overcast = 4,
    ShowerRain = 5,
    Rain = 6,
    Drizzle = 7,
    Thunderstorm = 8,
    Snow = 9,
    Atmosphere = 10,
    Mist = 11,
    Fog = 12,
}

// ── 80x80 icons (NOW view) ──────────────────────────────────────────

static ICON_CLEAR_80: &[u8] = include_bytes!("icons/clear_80.bmp");
static ICON_FEW_CLOUDS_80: &[u8] = include_bytes!("icons/few_clouds_80.bmp");
static ICON_SCATTERED_CLOUDS_80: &[u8] = include_bytes!("icons/scattered_clouds_80.bmp");
static ICON_BROKEN_CLOUDS_80: &[u8] = include_bytes!("icons/broken_clouds_80.bmp");
static ICON_OVERCAST_80: &[u8] = include_bytes!("icons/overcast_80.bmp");
static ICON_SHOWER_RAIN_80: &[u8] = include_bytes!("icons/shower_rain_80.bmp");
static ICON_RAIN_80: &[u8] = include_bytes!("icons/rain_80.bmp");
static ICON_DRIZZLE_80: &[u8] = include_bytes!("icons/drizzle_80.bmp");
static ICON_THUNDERSTORM_80: &[u8] = include_bytes!("icons/thunderstorm_80.bmp");
static ICON_SNOW_80: &[u8] = include_bytes!("icons/snow_80.bmp");
static ICON_ATMOSPHERE_80: &[u8] = include_bytes!("icons/atmosphere_80.bmp");
static ICON_MIST_80: &[u8] = include_bytes!("icons/mist_80.bmp");
static ICON_FOG_80: &[u8] = include_bytes!("icons/fog_80.bmp");

// ── 36x36 icons (forecast rows) ─────────────────────────────────────

static ICON_CLEAR_36: &[u8] = include_bytes!("icons/clear_36.bmp");
static ICON_FEW_CLOUDS_36: &[u8] = include_bytes!("icons/few_clouds_36.bmp");
static ICON_SCATTERED_CLOUDS_36: &[u8] = include_bytes!("icons/scattered_clouds_36.bmp");
static ICON_BROKEN_CLOUDS_36: &[u8] = include_bytes!("icons/broken_clouds_36.bmp");
static ICON_OVERCAST_36: &[u8] = include_bytes!("icons/overcast_36.bmp");
static ICON_SHOWER_RAIN_36: &[u8] = include_bytes!("icons/shower_rain_36.bmp");
static ICON_RAIN_36: &[u8] = include_bytes!("icons/rain_36.bmp");
static ICON_DRIZZLE_36: &[u8] = include_bytes!("icons/drizzle_36.bmp");
static ICON_THUNDERSTORM_36: &[u8] = include_bytes!("icons/thunderstorm_36.bmp");
static ICON_SNOW_36: &[u8] = include_bytes!("icons/snow_36.bmp");
static ICON_ATMOSPHERE_36: &[u8] = include_bytes!("icons/atmosphere_36.bmp");
static ICON_MIST_36: &[u8] = include_bytes!("icons/mist_36.bmp");
static ICON_FOG_36: &[u8] = include_bytes!("icons/fog_36.bmp");

impl WeatherIcon {
    /// Short text label for fallback display.
    pub fn label(self) -> &'static str {
        match self {
            Self::Clear => "Clear",
            Self::FewClouds => "Few Clouds",
            Self::ScatteredClouds => "Scattered",
            Self::BrokenClouds => "Cloudy",
            Self::Overcast => "Overcast",
            Self::ShowerRain => "Showers",
            Self::Rain => "Rain",
            Self::Drizzle => "Drizzle",
            Self::Thunderstorm => "Storm",
            Self::Snow => "Snow",
            Self::Atmosphere => "Haze",
            Self::Mist => "Mist",
            Self::Fog => "Fog",
        }
    }

    fn bmp_data_80(self) -> &'static [u8] {
        match self {
            Self::Clear => ICON_CLEAR_80,
            Self::FewClouds => ICON_FEW_CLOUDS_80,
            Self::ScatteredClouds => ICON_SCATTERED_CLOUDS_80,
            Self::BrokenClouds => ICON_BROKEN_CLOUDS_80,
            Self::Overcast => ICON_OVERCAST_80,
            Self::ShowerRain => ICON_SHOWER_RAIN_80,
            Self::Rain => ICON_RAIN_80,
            Self::Drizzle => ICON_DRIZZLE_80,
            Self::Thunderstorm => ICON_THUNDERSTORM_80,
            Self::Snow => ICON_SNOW_80,
            Self::Atmosphere => ICON_ATMOSPHERE_80,
            Self::Mist => ICON_MIST_80,
            Self::Fog => ICON_FOG_80,
        }
    }

    fn bmp_data_36(self) -> &'static [u8] {
        match self {
            Self::Clear => ICON_CLEAR_36,
            Self::FewClouds => ICON_FEW_CLOUDS_36,
            Self::ScatteredClouds => ICON_SCATTERED_CLOUDS_36,
            Self::BrokenClouds => ICON_BROKEN_CLOUDS_36,
            Self::Overcast => ICON_OVERCAST_36,
            Self::ShowerRain => ICON_SHOWER_RAIN_36,
            Self::Rain => ICON_RAIN_36,
            Self::Drizzle => ICON_DRIZZLE_36,
            Self::Thunderstorm => ICON_THUNDERSTORM_36,
            Self::Snow => ICON_SNOW_36,
            Self::Atmosphere => ICON_ATMOSPHERE_36,
            Self::Mist => ICON_MIST_36,
            Self::Fog => ICON_FOG_36,
        }
    }

    /// Draw the 80x80 icon at the given position.
    pub fn draw_80(self, fb: &mut Framebuffer, x: i32, y: i32) {
        if let Ok(bmp) = Bmp::<Rgb565>::from_slice(self.bmp_data_80()) {
            Image::new(&bmp, Point::new(x, y)).draw(fb).ok();
        }
    }

    /// Draw the 36x36 icon at the given position.
    pub fn draw_36(self, fb: &mut Framebuffer, x: i32, y: i32) {
        if let Ok(bmp) = Bmp::<Rgb565>::from_slice(self.bmp_data_36()) {
            Image::new(&bmp, Point::new(x, y)).draw(fb).ok();
        }
    }
}
