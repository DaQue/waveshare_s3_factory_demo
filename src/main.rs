use anyhow::Result;
use core::ffi::c_void;
use log::info;
use std::time::{Duration, Instant};

const LCD_WIDTH: i32 = 320;
const LCD_HEIGHT: i32 = 480;
const CHUNK_LINES: i32 = 20;
const PCLK_HZ: u32 = 5_000_000;
const PCLK_HZ_FAST: u32 = 40_000_000;
const TRIAL_DELAY_MS: u64 = 3000;

const PIN_LCD_SCLK: i32 = 5;
const PIN_LCD_D0: i32 = 1;
const PIN_LCD_D1: i32 = 2;
const PIN_LCD_D2: i32 = 3;
const PIN_LCD_D3: i32 = 4;
const PIN_LCD_CS: i32 = 12;
const PIN_LCD_BL: i32 = 6;

#[repr(C)]
struct Axs15231bLcdInitCmd {
    cmd: i32,
    data: *const c_void,
    data_bytes: usize,
    delay_ms: u32,
}

unsafe impl Sync for Axs15231bLcdInitCmd {}

#[repr(C)]
struct Axs15231bVendorFlags {
    use_qspi_interface: u32,
}

#[repr(C)]
struct Axs15231bVendorConfig {
    init_cmds: *const Axs15231bLcdInitCmd,
    init_cmds_size: u16,
    flags: Axs15231bVendorFlags,
}

extern "C" {
    fn esp_lcd_new_panel_axs15231b(
        io: esp_idf_sys::esp_lcd_panel_io_handle_t,
        panel_dev_config: *const esp_idf_sys::esp_lcd_panel_dev_config_t,
        ret_panel: *mut esp_idf_sys::esp_lcd_panel_handle_t,
    ) -> esp_idf_sys::esp_err_t;
}

static LCD_INIT_CMD_00: [u8; 8] = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5];
static LCD_INIT_CMD_01: [u8; 17] = [
    0xC0, 0x10, 0x00, 0x02, 0x00, 0x00, 0x04, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00,
    0x00, 0x00, 0x00,
];
static LCD_INIT_CMD_02: [u8; 31] = [
    0x30, 0x3C, 0x24, 0x14, 0xD0, 0x20, 0xFF, 0xE0, 0x40, 0x19, 0x80, 0x80, 0x80, 0x20,
    0xF9, 0x10, 0x02, 0xFF, 0xFF, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xE0, 0x20, 0x7F,
    0xFF, 0x00, 0x5A,
];
static LCD_INIT_CMD_03: [u8; 30] = [
    0xE0, 0x40, 0x51, 0x24, 0x08, 0x05, 0x10, 0x01, 0x20, 0x15, 0x42, 0xC2, 0x22, 0x22,
    0xAA, 0x03, 0x10, 0x12, 0x60, 0x14, 0x1E, 0x51, 0x15, 0x00, 0x8A, 0x20, 0x00, 0x03,
    0x3A, 0x12,
];
static LCD_INIT_CMD_04: [u8; 22] = [
    0xA0, 0x06, 0xAA, 0x00, 0x08, 0x02, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55,
];
static LCD_INIT_CMD_05: [u8; 30] = [
    0x31, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x00, 0x53, 0xFF,
    0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0D, 0x00,
    0xFF, 0x40,
];
static LCD_INIT_CMD_06: [u8; 11] = [0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01];
static LCD_INIT_CMD_07: [u8; 29] = [
    0x00, 0x24, 0x33, 0x80, 0x00, 0xEA, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90,
    0x11, 0x06, 0xDC, 0xFA, 0x00, 0x00, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44,
    0x50,
];
static LCD_INIT_CMD_08: [u8; 23] = [
    0x18, 0x00, 0x00, 0x03, 0xFE, 0x3A, 0x4A, 0x20, 0x30, 0x10, 0x88, 0xDE, 0x0D, 0x08,
    0x0F, 0x0F, 0x01, 0x3A, 0x4A, 0x20, 0x10, 0x10, 0x00,
];
static LCD_INIT_CMD_09: [u8; 20] = [
    0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x03,
    0x00, 0x3F, 0x6A, 0x18, 0xC8, 0x22,
];
static LCD_INIT_CMD_10: [u8; 20] = [
    0x50, 0x32, 0x28, 0x00, 0xA2, 0x80, 0x8F, 0x00, 0x80, 0xFF, 0x07, 0x11, 0x9C, 0x67,
    0xFF, 0x24, 0x0C, 0x0D, 0x0E, 0x0F,
];
static LCD_INIT_CMD_11: [u8; 4] = [0x33, 0x44, 0x44, 0x01];
static LCD_INIT_CMD_12: [u8; 27] = [
    0x2C, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x68, 0x88, 0x00, 0x65, 0x09,
    0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x08, 0x08, 0x12, 0xA0, 0x08,
];
static LCD_INIT_CMD_13: [u8; 30] = [
    0x40, 0x8E, 0x8D, 0x01, 0x35, 0x04, 0x92, 0x74, 0x04, 0x92, 0x74, 0x04, 0x08, 0x6A,
    0x04, 0x46, 0x03, 0x03, 0x03, 0x03, 0x82, 0x01, 0x03, 0x00, 0xE0, 0x51, 0xA1, 0x00,
    0x00, 0x00,
];
static LCD_INIT_CMD_14: [u8; 30] = [
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x93, 0x00, 0x01, 0x83, 0x07, 0x07,
    0x00, 0x07, 0x07, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x84, 0x00, 0x20,
    0x01, 0x00,
];
static LCD_INIT_CMD_15: [u8; 19] = [
    0x03, 0x01, 0x0B, 0x09, 0x0F, 0x0D, 0x1E, 0x1F, 0x18, 0x1D, 0x1F, 0x19, 0x40, 0x8E,
    0x04, 0x00, 0x20, 0xA0, 0x1F,
];
static LCD_INIT_CMD_16: [u8; 12] = [0x02, 0x00, 0x0A, 0x08, 0x0E, 0x0C, 0x1E, 0x1F, 0x18, 0x1D, 0x1F, 0x19];
static LCD_INIT_CMD_17: [u8; 12] = [0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F];
static LCD_INIT_CMD_18: [u8; 12] = [0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F];
static LCD_INIT_CMD_19: [u8; 8] = [0x44, 0x73, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90];
static LCD_INIT_CMD_20: [u8; 17] = [
    0x3B, 0x28, 0x10, 0x16, 0x0C, 0x06, 0x11, 0x28, 0x5C, 0x21, 0x0D, 0x35, 0x13, 0x2C,
    0x33, 0x28, 0x0D,
];
static LCD_INIT_CMD_21: [u8; 17] = [
    0x37, 0x28, 0x10, 0x16, 0x0B, 0x06, 0x11, 0x28, 0x5C, 0x21, 0x0D, 0x35, 0x14, 0x2C,
    0x33, 0x28, 0x0F,
];
static LCD_INIT_CMD_22: [u8; 17] = [
    0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36,
    0x3A, 0x2F, 0x0D,
];
static LCD_INIT_CMD_23: [u8; 17] = [
    0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36,
    0x32, 0x2F, 0x0F,
];
static LCD_INIT_CMD_24: [u8; 17] = [
    0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36,
    0x3A, 0x2F, 0x0D,
];
static LCD_INIT_CMD_25: [u8; 17] = [
    0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36,
    0x3A, 0x2F, 0x0F,
];
static LCD_INIT_CMD_26: [u8; 16] = [
    0x85, 0x85, 0x95, 0x82, 0xAF, 0xAA, 0xAA, 0x80, 0x10, 0x30, 0x40, 0x40, 0x20, 0xFF,
    0x60, 0x30,
];
static LCD_INIT_CMD_27: [u8; 4] = [0x85, 0x85, 0x95, 0x85];
static LCD_INIT_CMD_28: [u8; 8] = [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00];
static LCD_INIT_CMD_29: [u8; 4] = [0x00, 0x00, 0x00, 0x00];

static LCD_INIT_CMDS: [Axs15231bLcdInitCmd; 32] = [
    Axs15231bLcdInitCmd {
        cmd: 0xBB,
        data: LCD_INIT_CMD_00.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_00.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xA0,
        data: LCD_INIT_CMD_01.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_01.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xA2,
        data: LCD_INIT_CMD_02.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_02.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xD0,
        data: LCD_INIT_CMD_03.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_03.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xA3,
        data: LCD_INIT_CMD_04.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_04.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC1,
        data: LCD_INIT_CMD_05.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_05.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC3,
        data: LCD_INIT_CMD_06.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_06.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC4,
        data: LCD_INIT_CMD_07.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_07.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC5,
        data: LCD_INIT_CMD_08.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_08.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC6,
        data: LCD_INIT_CMD_09.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_09.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC7,
        data: LCD_INIT_CMD_10.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_10.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xC9,
        data: LCD_INIT_CMD_11.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_11.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xCF,
        data: LCD_INIT_CMD_12.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_12.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xD5,
        data: LCD_INIT_CMD_13.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_13.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xD6,
        data: LCD_INIT_CMD_14.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_14.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xD7,
        data: LCD_INIT_CMD_15.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_15.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xD8,
        data: LCD_INIT_CMD_16.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_16.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xD9,
        data: LCD_INIT_CMD_17.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_17.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xDD,
        data: LCD_INIT_CMD_18.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_18.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xDF,
        data: LCD_INIT_CMD_19.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_19.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xE0,
        data: LCD_INIT_CMD_20.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_20.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xE1,
        data: LCD_INIT_CMD_21.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_21.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xE2,
        data: LCD_INIT_CMD_22.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_22.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xE3,
        data: LCD_INIT_CMD_23.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_23.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xE4,
        data: LCD_INIT_CMD_24.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_24.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xE5,
        data: LCD_INIT_CMD_25.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_25.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xA4,
        data: LCD_INIT_CMD_26.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_26.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xA4,
        data: LCD_INIT_CMD_27.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_27.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0xBB,
        data: LCD_INIT_CMD_28.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_28.len(),
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0x13,
        data: core::ptr::null(),
        data_bytes: 0,
        delay_ms: 0,
    },
    Axs15231bLcdInitCmd {
        cmd: 0x11,
        data: core::ptr::null(),
        data_bytes: 0,
        delay_ms: 120,
    },
    Axs15231bLcdInitCmd {
        cmd: 0x2C,
        data: LCD_INIT_CMD_29.as_ptr().cast(),
        data_bytes: LCD_INIT_CMD_29.len(),
        delay_ms: 0,
    },
];

#[derive(Clone, Copy)]
struct LaneMap {
    d0: i32,
    d1: i32,
    d2: i32,
    d3: i32,
}

#[derive(Clone, Copy)]
struct TrialConfig {
    name: &'static str,
    lanes: LaneMap,
    spi_mode: i32,
    quad: bool,
    cmd_bits: i32,
    param_bits: i32,
    pclk_hz: u32,
    rgb_order: esp_idf_sys::lcd_rgb_element_order_t,
    data_endian: esp_idf_sys::lcd_rgb_data_endian_t,
    invert: bool,
    swap_xy: bool,
    mirror_x: bool,
    mirror_y: bool,
    sio_mode: bool,
    use_custom_init: bool,
    use_qspi_interface: bool,
}

fn build_trials() -> Vec<TrialConfig> {
    let lanes = LaneMap {
        d0: PIN_LCD_D0,
        d1: PIN_LCD_D1,
        d2: PIN_LCD_D2,
        d3: PIN_LCD_D3,
    };

    vec![
        TrialConfig {
            name: "QIO_cmd8_qspiif0_m3_5MHz",
            lanes,
            spi_mode: 3,
            quad: true,
            cmd_bits: 8,
            param_bits: 8,
            pclk_hz: PCLK_HZ,
            rgb_order: esp_idf_sys::lcd_rgb_element_order_t_LCD_RGB_ELEMENT_ORDER_RGB,
            data_endian: esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG,
            invert: false,
            swap_xy: false,
            mirror_x: false,
            mirror_y: false,
            sio_mode: false,
            use_custom_init: true,
            use_qspi_interface: false,
        },
        TrialConfig {
            name: "QIO_cmd8_qspiif0_m0_5MHz",
            lanes,
            spi_mode: 0,
            quad: true,
            cmd_bits: 8,
            param_bits: 8,
            pclk_hz: PCLK_HZ,
            rgb_order: esp_idf_sys::lcd_rgb_element_order_t_LCD_RGB_ELEMENT_ORDER_RGB,
            data_endian: esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG,
            invert: false,
            swap_xy: false,
            mirror_x: false,
            mirror_y: false,
            sio_mode: false,
            use_custom_init: true,
            use_qspi_interface: false,
        },
        TrialConfig {
            name: "QIO_cmd8_qspiif0_m3_40MHz",
            lanes,
            spi_mode: 3,
            quad: true,
            cmd_bits: 8,
            param_bits: 8,
            pclk_hz: PCLK_HZ_FAST,
            rgb_order: esp_idf_sys::lcd_rgb_element_order_t_LCD_RGB_ELEMENT_ORDER_RGB,
            data_endian: esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG,
            invert: false,
            swap_xy: false,
            mirror_x: false,
            mirror_y: false,
            sio_mode: false,
            use_custom_init: true,
            use_qspi_interface: false,
        },
        TrialConfig {
            name: "QSPI_cmd32_qspiif1_m3_5MHz",
            lanes,
            spi_mode: 3,
            quad: true,
            cmd_bits: 32,
            param_bits: 8,
            pclk_hz: PCLK_HZ,
            rgb_order: esp_idf_sys::lcd_rgb_element_order_t_LCD_RGB_ELEMENT_ORDER_RGB,
            data_endian: esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG,
            invert: false,
            swap_xy: false,
            mirror_x: false,
            mirror_y: false,
            sio_mode: false,
            use_custom_init: true,
            use_qspi_interface: true,
        },
        TrialConfig {
            name: "QSPI_cmd32_qspiif1_m3_40MHz",
            lanes,
            spi_mode: 3,
            quad: true,
            cmd_bits: 32,
            param_bits: 8,
            pclk_hz: PCLK_HZ_FAST,
            rgb_order: esp_idf_sys::lcd_rgb_element_order_t_LCD_RGB_ELEMENT_ORDER_RGB,
            data_endian: esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG,
            invert: false,
            swap_xy: false,
            mirror_x: false,
            mirror_y: false,
            sio_mode: false,
            use_custom_init: true,
            use_qspi_interface: true,
        },
    ]
}

fn main() -> Result<()> {
    esp_idf_sys::link_patches();
    // Use ESP-IDF logger so output goes to UART reliably.
    esp_idf_svc::log::EspLogger::initialize_default();

    info!("BOOT OK (waveshare_s3_3p)");
    info!(
        "Display bring-up trial harness (PCLK per-trial: {} / {} Hz)",
        PCLK_HZ, PCLK_HZ_FAST
    );
    info!("Panel driver: AXS15231B (factory init table, QSPI cmd32/param8)");

    init_usb_serial_jtag();
    enable_backlight();

    let trials = build_trials();
    for (idx, trial) in trials.iter().enumerate() {
        info!(
            "[TRIAL {:02}] {} | lanes {}{}{}{} mode {} quad {} cmd_bits {} param_bits {} pclk {} rgb {:?} endian {:?} invert {} swap {} mirror {}{} sio {} init {} qspi_if {}",
            idx + 1,
            trial.name,
            trial.lanes.d0,
            trial.lanes.d1,
            trial.lanes.d2,
            trial.lanes.d3,
            trial.spi_mode,
            trial.quad,
            trial.cmd_bits,
            trial.param_bits,
            trial.pclk_hz,
            trial.rgb_order,
            trial.data_endian,
            trial.invert,
            trial.swap_xy,
            trial.mirror_x,
            if trial.mirror_y { "Y" } else { "N" },
            trial.sio_mode,
            if trial.use_custom_init { "custom" } else { "default" },
            trial.use_qspi_interface
        );

        let mut ctx = match start_trial(*trial) {
            Ok(ctx) => ctx,
            Err(err) => {
                info!("[TRIAL {:02}] error: {:?}", idx + 1, err);
                continue;
            }
        };

        if let Err(err) = draw_solid_color(ctx.panel, 0x07E0, trial.data_endian) {
            info!("[TRIAL {:02}] error: {:?}", idx + 1, err);
            ctx.cleanup();
            continue;
        }

        info!(
            "[TRIAL {:02}] Enter result: p=pass, gf=green flash, nf=noise flash, b=blank",
            idx + 1
        );
        let result = read_result_code();
        info!("[TRIAL {:02}] RESULT {}", idx + 1, result);

        if result == "p" {
            if let Err(err) = draw_box_x_pattern(ctx.panel, trial.data_endian) {
                info!("[TRIAL {:02}] box/x draw error: {:?}", idx + 1, err);
            } else {
                info!("[TRIAL {:02}] Box/X shown (PASS). Stopping trials.", idx + 1);
            }
            ctx.cleanup();
            break;
        }

        ctx.cleanup();
        std::thread::sleep(Duration::from_millis(TRIAL_DELAY_MS));
    }

    info!("All trials complete. Reset to re-run.");
    loop {
        std::thread::sleep(Duration::from_secs(1));
    }
}

struct LcdContext {
    host: esp_idf_sys::spi_host_device_t,
    io: esp_idf_sys::esp_lcd_panel_io_handle_t,
    panel: esp_idf_sys::esp_lcd_panel_handle_t,
}

impl LcdContext {
    fn cleanup(&mut self) {
        unsafe {
            esp_idf_sys::esp_lcd_panel_del(self.panel);
            esp_idf_sys::esp_lcd_panel_io_del(self.io);
            esp_idf_sys::spi_bus_free(self.host);
        }
    }
}

fn start_trial(trial: TrialConfig) -> Result<LcdContext> {
    let mut bus_cfg = esp_idf_sys::spi_bus_config_t::default();
    bus_cfg.__bindgen_anon_1.mosi_io_num = trial.lanes.d0;
    bus_cfg.__bindgen_anon_2.miso_io_num = if trial.quad { trial.lanes.d1 } else { -1 };
    bus_cfg.__bindgen_anon_3.quadwp_io_num = if trial.quad { trial.lanes.d2 } else { -1 };
    bus_cfg.__bindgen_anon_4.quadhd_io_num = if trial.quad { trial.lanes.d3 } else { -1 };
    bus_cfg.sclk_io_num = PIN_LCD_SCLK;
    bus_cfg.max_transfer_sz = (LCD_WIDTH * LCD_HEIGHT * 2) as i32;
    bus_cfg.flags = 0;
    bus_cfg.intr_flags = 0;

    let host = esp_idf_sys::spi_host_device_t_SPI2_HOST;
    unsafe {
        esp_idf_sys::spi_bus_initialize(
            host,
            &bus_cfg,
            esp_idf_sys::spi_common_dma_t_SPI_DMA_CH_AUTO,
        );
    }

    let mut io: esp_idf_sys::esp_lcd_panel_io_handle_t = std::ptr::null_mut();
    let mut io_cfg = esp_idf_sys::esp_lcd_panel_io_spi_config_t {
        cs_gpio_num: PIN_LCD_CS,
        dc_gpio_num: -1,
        spi_mode: trial.spi_mode,
        pclk_hz: trial.pclk_hz,
        trans_queue_depth: 10,
        on_color_trans_done: None,
        user_ctx: std::ptr::null_mut(),
        lcd_cmd_bits: trial.cmd_bits,
        lcd_param_bits: trial.param_bits,
        flags: esp_idf_sys::esp_lcd_panel_io_spi_config_t__bindgen_ty_1 {
            _bitfield_align_1: [],
            _bitfield_1: esp_idf_sys::esp_lcd_panel_io_spi_config_t__bindgen_ty_1::new_bitfield_1(
                0,
                0,
                0,
                0,
                if trial.quad { 1 } else { 0 },
                if trial.sio_mode { 1 } else { 0 },
                0,
                0,
            ),
            __bindgen_padding_0: [0; 3],
        },
    };

    let io_res = unsafe {
        esp_idf_sys::esp_lcd_new_panel_io_spi(
            host as esp_idf_sys::esp_lcd_spi_bus_handle_t,
            &mut io_cfg,
            &mut io,
        )
    };
    if io_res != esp_idf_sys::ESP_OK {
        unsafe { esp_idf_sys::spi_bus_free(host) };
        return Err(anyhow::anyhow!("esp_lcd_new_panel_io_spi failed {}", io_res));
    }

    let mut panel: esp_idf_sys::esp_lcd_panel_handle_t = std::ptr::null_mut();
    let (init_cmds, init_cmds_size) = if trial.use_custom_init {
        (LCD_INIT_CMDS.as_ptr(), LCD_INIT_CMDS.len() as u16)
    } else {
        (core::ptr::null(), 0u16)
    };

    let vendor_config = Axs15231bVendorConfig {
        init_cmds,
        init_cmds_size,
        flags: Axs15231bVendorFlags {
            use_qspi_interface: if trial.use_qspi_interface { 1 } else { 0 },
        },
    };

    let panel_cfg = esp_idf_sys::esp_lcd_panel_dev_config_t {
        reset_gpio_num: -1,
        __bindgen_anon_1: esp_idf_sys::esp_lcd_panel_dev_config_t__bindgen_ty_1 {
            rgb_ele_order: trial.rgb_order,
        },
        data_endian: trial.data_endian,
        bits_per_pixel: 16,
        flags: esp_idf_sys::esp_lcd_panel_dev_config_t__bindgen_ty_2 {
            _bitfield_align_1: [],
            _bitfield_1: esp_idf_sys::esp_lcd_panel_dev_config_t__bindgen_ty_2::new_bitfield_1(0),
            __bindgen_padding_0: [0; 3],
        },
        vendor_config: &vendor_config as *const Axs15231bVendorConfig as *mut c_void,
    };

    let panel_res = unsafe { esp_lcd_new_panel_axs15231b(io, &panel_cfg, &mut panel) };
    if panel_res != esp_idf_sys::ESP_OK {
        unsafe {
            esp_idf_sys::esp_lcd_panel_io_del(io);
            esp_idf_sys::spi_bus_free(host);
        }
        return Err(anyhow::anyhow!(
            "esp_lcd_new_panel_axs15231b failed {}",
            panel_res
        ));
    }

    unsafe {
        esp_idf_sys::esp_lcd_panel_reset(panel);
        esp_idf_sys::esp_lcd_panel_init(panel);
        esp_idf_sys::esp_lcd_panel_invert_color(panel, trial.invert);
        esp_idf_sys::esp_lcd_panel_swap_xy(panel, trial.swap_xy);
        esp_idf_sys::esp_lcd_panel_mirror(panel, trial.mirror_x, trial.mirror_y);
        esp_idf_sys::esp_lcd_panel_disp_off(panel, false);
    }

    Ok(LcdContext { host, io, panel })
}

fn draw_solid_color(
    panel: esp_idf_sys::esp_lcd_panel_handle_t,
    color: u16,
    endian: esp_idf_sys::lcd_rgb_data_endian_t,
) -> Result<()> {
    let len = (LCD_WIDTH * LCD_HEIGHT * 2) as usize;
    if let Some(ptr) = try_alloc_frame_buffer(len) {
        let slice = unsafe { std::slice::from_raw_parts_mut(ptr, len) };
        fill_color_bytes(slice, color, endian);
        let res = unsafe {
            esp_idf_sys::esp_lcd_panel_draw_bitmap(
                panel,
                0,
                0,
                LCD_WIDTH,
                LCD_HEIGHT,
                slice.as_ptr() as *const core::ffi::c_void,
            )
        };
        unsafe {
            esp_idf_sys::heap_caps_free(ptr as *mut core::ffi::c_void);
        }
        if res != esp_idf_sys::ESP_OK {
            return Err(anyhow::anyhow!("draw_bitmap failed {}", res));
        }
        return Ok(());
    }

    info!("PSRAM full-frame alloc failed, falling back to chunked draw (RAMWR forced)");
    draw_solid_color_chunked(panel, color, endian)
}

fn draw_box_x_pattern(
    panel: esp_idf_sys::esp_lcd_panel_handle_t,
    endian: esp_idf_sys::lcd_rgb_data_endian_t,
) -> Result<()> {
    let width = LCD_WIDTH as usize;
    let height = LCD_HEIGHT as usize;
    let len = width * height * 2;
    if let Some(ptr) = try_alloc_frame_buffer(len) {
        let buf = unsafe { std::slice::from_raw_parts_mut(ptr, len) };
        fill_box_x_pattern(buf, width, height, endian);
        let res = unsafe {
            esp_idf_sys::esp_lcd_panel_draw_bitmap(
                panel,
                0,
                0,
                LCD_WIDTH,
                LCD_HEIGHT,
                buf.as_ptr() as *const core::ffi::c_void,
            )
        };
        unsafe {
            esp_idf_sys::heap_caps_free(ptr as *mut core::ffi::c_void);
        }
        if res != esp_idf_sys::ESP_OK {
            return Err(anyhow::anyhow!("draw_bitmap failed {}", res));
        }
        return Ok(());
    }

    info!("PSRAM full-frame alloc failed, falling back to chunked Box/X (RAMWR forced)");
    draw_box_x_pattern_chunked(panel, endian)
}

fn fill_box_x_pattern(buf: &mut [u8], width: usize, height: usize, endian: esp_idf_sys::lcd_rgb_data_endian_t) {
    let margin_x = (LCD_WIDTH / 8).max(20);
    let margin_y = (LCD_HEIGHT / 6).max(20);
    let box_x0 = margin_x;
    let box_y0 = margin_y;
    let box_w = LCD_WIDTH - 2 * margin_x;
    let box_h = LCD_HEIGHT - 2 * margin_y;
    let box_x1 = box_x0 + box_w;
    let box_y1 = box_y0 + box_h;

    for yy in 0..height {
        let yy_i = yy as i32;
        for xx in 0..width {
            let xx_i = xx as i32;
            let mut color = 0x0000u16; // black
            if xx_i >= box_x0 && xx_i < box_x1 && yy_i >= box_y0 && yy_i < box_y1 {
                color = 0x001F; // blue
                let bx = xx_i - box_x0;
                let by = yy_i - box_y0;
                if bx == by || bx == (box_w - 1 - by) {
                    color = 0xFFE0; // yellow
                }
            }
            let idx = (yy * width + xx) * 2;
            match endian {
                esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG => {
                    buf[idx] = (color >> 8) as u8;
                    buf[idx + 1] = (color & 0xFF) as u8;
                }
                _ => {
                    buf[idx] = (color & 0xFF) as u8;
                    buf[idx + 1] = (color >> 8) as u8;
                }
            }
        }
    }
}

fn fill_color_bytes(buf: &mut [u8], color: u16, endian: esp_idf_sys::lcd_rgb_data_endian_t) {
    let (hi, lo) = ((color >> 8) as u8, (color & 0xFF) as u8);
    match endian {
        esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG => {
            for chunk in buf.chunks_exact_mut(2) {
                chunk[0] = hi;
                chunk[1] = lo;
            }
        }
        _ => {
            for chunk in buf.chunks_exact_mut(2) {
                chunk[0] = lo;
                chunk[1] = hi;
            }
        }
    }
}

fn try_alloc_frame_buffer(len: usize) -> Option<*mut u8> {
    unsafe {
        let ptr = esp_idf_sys::heap_caps_malloc(
            len,
            esp_idf_sys::MALLOC_CAP_SPIRAM | esp_idf_sys::MALLOC_CAP_8BIT,
        ) as *mut u8;
        if !ptr.is_null() {
            return Some(ptr);
        }
        let ptr = esp_idf_sys::heap_caps_malloc(
            len,
            esp_idf_sys::MALLOC_CAP_INTERNAL | esp_idf_sys::MALLOC_CAP_8BIT,
        ) as *mut u8;
        if !ptr.is_null() {
            return Some(ptr);
        }
    }
    None
}

fn draw_solid_color_chunked(
    panel: esp_idf_sys::esp_lcd_panel_handle_t,
    color: u16,
    endian: esp_idf_sys::lcd_rgb_data_endian_t,
) -> Result<()> {
    let mut buf = vec![0u8; (LCD_WIDTH * CHUNK_LINES * 2) as usize];
    fill_color_bytes(&mut buf, color, endian);

    let mut y = 0;
    while y < LCD_HEIGHT {
        let y_end = (y + CHUNK_LINES).min(LCD_HEIGHT);
        unsafe {
            esp_idf_sys::esp_lcd_panel_draw_bitmap(
                panel,
                0,
                y,
                LCD_WIDTH,
                y_end,
                buf.as_ptr() as *const core::ffi::c_void,
            );
        }
        y = y_end;
    }
    Ok(())
}

fn draw_box_x_pattern_chunked(
    panel: esp_idf_sys::esp_lcd_panel_handle_t,
    endian: esp_idf_sys::lcd_rgb_data_endian_t,
) -> Result<()> {
    let mut buf = vec![0u8; (LCD_WIDTH * CHUNK_LINES * 2) as usize];
    let margin_x = (LCD_WIDTH / 8).max(20);
    let margin_y = (LCD_HEIGHT / 6).max(20);
    let box_x0 = margin_x;
    let box_y0 = margin_y;
    let box_w = LCD_WIDTH - 2 * margin_x;
    let box_h = LCD_HEIGHT - 2 * margin_y;
    let box_x1 = box_x0 + box_w;
    let box_y1 = box_y0 + box_h;

    let mut y = 0;
    while y < LCD_HEIGHT {
        let y_end = (y + CHUNK_LINES).min(LCD_HEIGHT);
        let mut idx = 0;
        for yy in y..y_end {
            for xx in 0..LCD_WIDTH {
                let mut color = 0x0000u16; // black
                if (xx as i32) >= box_x0
                    && (xx as i32) < box_x1
                    && (yy as i32) >= box_y0
                    && (yy as i32) < box_y1
                {
                    color = 0x001F; // blue
                    let bx = (xx as i32) - box_x0;
                    let by = (yy as i32) - box_y0;
                    if bx == by || bx == (box_w - 1 - by) {
                        color = 0xFFE0; // yellow
                    }
                }
                match endian {
                    esp_idf_sys::lcd_rgb_data_endian_t_LCD_RGB_DATA_ENDIAN_BIG => {
                        buf[idx] = (color >> 8) as u8;
                        buf[idx + 1] = (color & 0xFF) as u8;
                    }
                    _ => {
                        buf[idx] = (color & 0xFF) as u8;
                        buf[idx + 1] = (color >> 8) as u8;
                    }
                }
                idx += 2;
            }
        }
        unsafe {
            esp_idf_sys::esp_lcd_panel_draw_bitmap(
                panel,
                0,
                y,
                LCD_WIDTH,
                y_end,
                buf.as_ptr() as *const core::ffi::c_void,
            );
        }
        y = y_end;
    }
    Ok(())
}

fn init_usb_serial_jtag() {
    unsafe {
        let mut cfg = esp_idf_sys::usb_serial_jtag_driver_config_t {
            tx_buffer_size: 1024,
            rx_buffer_size: 1024,
        };
        esp_idf_sys::usb_serial_jtag_driver_install(&mut cfg);
    }
}

fn enable_backlight() {
    unsafe {
        let io_conf = esp_idf_sys::gpio_config_t {
            pin_bit_mask: 1u64 << (PIN_LCD_BL as u64),
            mode: esp_idf_sys::gpio_mode_t_GPIO_MODE_OUTPUT,
            pull_up_en: esp_idf_sys::gpio_pullup_t_GPIO_PULLUP_DISABLE,
            pull_down_en: esp_idf_sys::gpio_pulldown_t_GPIO_PULLDOWN_DISABLE,
            intr_type: esp_idf_sys::gpio_int_type_t_GPIO_INTR_DISABLE,
        };
        esp_idf_sys::gpio_config(&io_conf);
        esp_idf_sys::gpio_set_level(PIN_LCD_BL, 1);
    }
}

fn read_result_code() -> String {
    let tick_hz = esp_idf_sys::configTICK_RATE_HZ as u32;
    let ticks = ((10 * tick_hz) / 1000).max(1);
    let mut buf = [0u8; 1];
    let mut out = String::new();
    let start = Instant::now();
    loop {
        let n = unsafe {
            esp_idf_sys::usb_serial_jtag_read_bytes(buf.as_mut_ptr().cast(), 1, ticks)
        };
        if n > 0 {
            let b = buf[0];
            if b == b'\r' || b == b'\n' {
                if !out.is_empty() {
                    return out;
                }
            } else if b == 0x08 || b == 0x7f {
                out.pop();
            } else if let Some(ch) = char::from_u32(b as u32) {
                if !ch.is_control() {
                    out.push(ch);
                }
            }
        }
        if start.elapsed() > Duration::from_secs(120) && !out.is_empty() {
            return out;
        }
    }
}
