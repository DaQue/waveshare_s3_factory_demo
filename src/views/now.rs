use embedded_graphics::{
    mono_font::MonoTextStyle,
    prelude::*,
    text::{Alignment, Text},
};
use profont::{PROFONT_10_POINT, PROFONT_12_POINT, PROFONT_14_POINT, PROFONT_24_POINT};

use crate::framebuffer::Framebuffer;
use crate::layout::*;
use crate::views::AppState;

pub fn draw(fb: &mut Framebuffer, state: &AppState) {
    // 1. Fill background
    fb.clear_color(BG_NOW);

    // 2. Header line
    draw_hline(fb, HEADER_LINE_Y, LINE_COLOR_1);

    // 3. Header: time (left), city (center), RSSI (right)
    let header_style = MonoTextStyle::new(&PROFONT_14_POINT, TEXT_HEADER);
    Text::new(&state.time_text, Point::new(10, 24), header_style)
        .draw(fb)
        .ok();

    // City name centered
    if let Some(cw) = &state.current_weather {
        let city_text = format!("{}, {}", cw.city, cw.country);
        Text::with_alignment(
            &city_text,
            Point::new(SCREEN_W / 2, 24),
            header_style,
            Alignment::Center,
        )
        .draw(fb)
        .ok();
    }

    // RSSI / status (right)
    let status_style = MonoTextStyle::new(&PROFONT_10_POINT, TEXT_STATUS);
    Text::with_alignment(
        &state.status_text,
        Point::new(SCREEN_W - 8, 24),
        status_style,
        Alignment::Right,
    )
    .draw(fb)
    .ok();

    // 4. Main weather card
    let card_top = 36;
    let card_h = 148;
    draw_card(
        fb,
        CARD_MARGIN, card_top,
        SCREEN_W - 2 * CARD_MARGIN, card_h,
        CARD_RADIUS as u32,
        CARD_FILL_NOW, CARD_BORDER_NOW, 1,
    );

    // 5. Weather icon (80x80)
    let icon = state
        .current_weather
        .as_ref()
        .map(|w| w.icon)
        .unwrap_or_default();
    icon.draw_80(fb, 18, card_top + 10);

    // 6. Temperature + condition text
    if let Some(cw) = &state.current_weather {
        // Large temperature
        let temp_text = format!("{:.0}°", cw.temp_f);
        let temp_style = MonoTextStyle::new(&PROFONT_24_POINT, TEXT_PRIMARY);
        Text::new(&temp_text, Point::new(120, card_top + 46), temp_style)
            .draw(fb)
            .ok();

        // "FEELS XX°" in accent blue
        let feels_text = format!("FEELS {:.0}°", cw.feels_f);
        let feels_style = MonoTextStyle::new(&PROFONT_14_POINT, TEXT_CONDITION);
        Text::new(&feels_text, Point::new(120, card_top + 72), feels_style)
            .draw(fb)
            .ok();

        // Condition description
        let cond_style = MonoTextStyle::new(&PROFONT_12_POINT, TEXT_DETAIL);
        Text::new(&cw.condition, Point::new(120, card_top + 94), cond_style)
            .draw(fb)
            .ok();

        // Wind + humidity + pressure line
        let stats_text = format!(
            "Hum {}%  Wind {:.0}mph  {} hPa",
            cw.humidity, cw.wind_mph, cw.pressure_hpa
        );
        let stats_style = MonoTextStyle::new(&PROFONT_10_POINT, TEXT_TERTIARY);
        Text::new(&stats_text, Point::new(18, card_top + 120), stats_style)
            .draw(fb)
            .ok();

        // Tap hint
        let hint_style = MonoTextStyle::new(&PROFONT_10_POINT, TEXT_BOTTOM);
        Text::new(
            "(tap temp = °F/°C)   (tap icon = refresh)",
            Point::new(18, card_top + 138),
            hint_style,
        )
        .draw(fb)
        .ok();
    } else {
        let no_data_style = MonoTextStyle::new(&PROFONT_14_POINT, TEXT_DETAIL);
        Text::new("Waiting for weather data...", Point::new(120, card_top + 60), no_data_style)
            .draw(fb)
            .ok();
    }

    // 7. Bottom section: two panels side by side
    let bottom_y = card_top + card_h + 6;  // 190
    let panel_gap = 6;
    let panel_w = (SCREEN_W - 2 * CARD_MARGIN - panel_gap) / 2;
    let panel_h = SCREEN_H - bottom_y - 8;

    // -- Left panel: Indoor --
    draw_card(
        fb,
        CARD_MARGIN, bottom_y,
        panel_w, panel_h,
        10,
        CARD_FILL_INDOOR, CARD_BORDER_INDOOR, 1,
    );

    let label_style = MonoTextStyle::new(&PROFONT_14_POINT, TEXT_HEADER);
    Text::new("Indoor", Point::new(CARD_MARGIN + 10, bottom_y + 20), label_style)
        .draw(fb)
        .ok();

    let indoor_style = MonoTextStyle::new(&PROFONT_12_POINT, TEXT_SECONDARY);
    let indoor_detail = MonoTextStyle::new(&PROFONT_10_POINT, TEXT_TERTIARY);

    if let Some(temp) = state.indoor_temp {
        let temp_text = format!("{:.1}°", temp);
        Text::new(&temp_text, Point::new(CARD_MARGIN + 10, bottom_y + 42), indoor_style)
            .draw(fb)
            .ok();
    }
    if let Some(hum) = state.indoor_humidity {
        let hum_text = format!("{:.0}% RH", hum);
        Text::new(&hum_text, Point::new(CARD_MARGIN + 100, bottom_y + 42), indoor_style)
            .draw(fb)
            .ok();
    }
    if let Some(press) = state.indoor_pressure {
        let press_text = format!("{:.0} hPa", press);
        Text::new(&press_text, Point::new(CARD_MARGIN + 10, bottom_y + 60), indoor_detail)
            .draw(fb)
            .ok();
    }

    // -- Right panel: Forecast preview --
    let right_x = CARD_MARGIN + panel_w + panel_gap;
    draw_card(
        fb,
        right_x, bottom_y,
        panel_w, panel_h,
        10,
        CARD_FILL_FORECAST_PREVIEW, CARD_BORDER_FORECAST_PREVIEW, 1,
    );

    // "Forecast >" header with arrow hint
    let fc_label = "Forecast  \u{25b8}";
    Text::new(fc_label, Point::new(right_x + 10, bottom_y + 20), label_style)
        .draw(fb)
        .ok();

    // Show next 2-3 day previews
    let preview_style = MonoTextStyle::new(&PROFONT_10_POINT, TEXT_TERTIARY);
    if let Some(fc) = &state.forecast {
        for (i, row) in fc.rows.iter().take(3).enumerate() {
            let py = bottom_y + 38 + i as i32 * 18;
            let label = format!("{} {}°", row.title, row.temp_f);
            Text::new(&label, Point::new(right_x + 44, py), preview_style)
                .draw(fb)
                .ok();
            // Small icon
            row.icon.draw_36(fb, right_x + 6, py - 14);
        }
    }

    // 8. Bottom hint
    let hint_style = MonoTextStyle::new(&PROFONT_10_POINT, TEXT_BOTTOM);
    Text::with_alignment(
        "(swipe \u{2190}/\u{2192} or tap header to switch pages)",
        Point::new(SCREEN_W / 2, SCREEN_H - 4),
        hint_style,
        Alignment::Center,
    )
    .draw(fb)
    .ok();
}
