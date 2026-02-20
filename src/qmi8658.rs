//! QMI8658 6-axis IMU (accelerometer + gyroscope) driver.
//!
//! Board axis orientation (Waveshare ESP32-S3-Touch-LCD-3.5B):
//!   Y axis = long edge (top-to-bottom). Negative Y = top of display.
//!   X axis = short edge (left-to-right). Positive X = USB port side.
//!   Z axis = perpendicular to screen. Positive Z = back of board (face down).
//!
//! In normal landscape (top up): gravity reads ~-1g on Y.
//! Face down on table: gravity reads ~+1g on Z.

use esp_idf_hal::i2c::I2cDriver;
use log::info;

const QMI8658_ADDR: u8 = 0x6B;

// Registers
const REG_WHO_AM_I: u8 = 0x00;
const REG_CTRL2: u8 = 0x03; // Accelerometer config
const REG_CTRL3: u8 = 0x04; // Gyroscope config
const REG_CTRL7: u8 = 0x08; // Sensor enable
const REG_TEMP_L: u8 = 0x33;
#[allow(dead_code)]
const REG_ACC_X_L: u8 = 0x35;

const WHO_AM_I_EXPECTED: u8 = 0x05;

/// Accelerometer full-scale range.
const ACCEL_SCALE_G: f32 = 8.0; // ±8g (CTRL2 = 0x24)
/// Gyroscope full-scale range.
const GYRO_SCALE_DPS: f32 = 512.0; // ±512 dps (CTRL3 = 0x54)

pub struct ImuReading {
    pub accel_x: f32,
    pub accel_y: f32,
    pub accel_z: f32,
    pub gyro_x: f32,
    pub gyro_y: f32,
    pub gyro_z: f32,
    pub temp_c: f32,
}

fn write_reg(i2c: &mut I2cDriver<'_>, reg: u8, val: u8) -> bool {
    i2c.write(QMI8658_ADDR, &[reg, val], 100).is_ok()
}

fn read_reg(i2c: &mut I2cDriver<'_>, reg: u8) -> Option<u8> {
    let mut buf = [0u8; 1];
    i2c.write_read(QMI8658_ADDR, &[reg], &mut buf, 100).ok()?;
    Some(buf[0])
}

fn read_regs(i2c: &mut I2cDriver<'_>, reg: u8, buf: &mut [u8]) -> bool {
    i2c.write_read(QMI8658_ADDR, &[reg], buf, 100).is_ok()
}

fn raw_to_i16(low: u8, high: u8) -> i16 {
    (high as i16) << 8 | low as i16
}

/// Initialize the QMI8658 IMU. Returns true if successful.
pub fn init(i2c: &mut I2cDriver<'_>) -> bool {
    let who = match read_reg(i2c, REG_WHO_AM_I) {
        Some(v) => v,
        None => {
            info!("QMI8658: not found at 0x{:02X}", QMI8658_ADDR);
            return false;
        }
    };

    if who != WHO_AM_I_EXPECTED {
        info!("QMI8658: unexpected WHO_AM_I=0x{:02X} (expected 0x{:02X})", who, WHO_AM_I_EXPECTED);
        return false;
    }

    // Enable address auto-increment for burst reads
    if !write_reg(i2c, 0x02, 0x60) {
        info!("QMI8658: failed to write CTRL1");
        return false;
    }

    // Configure accelerometer: ±8g, 125Hz ODR
    if !write_reg(i2c, REG_CTRL2, 0x24) {
        info!("QMI8658: failed to write CTRL2");
        return false;
    }

    // Configure gyroscope: ±512 dps, 125Hz ODR
    if !write_reg(i2c, REG_CTRL3, 0x54) {
        info!("QMI8658: failed to write CTRL3");
        return false;
    }

    // Enable both accelerometer and gyroscope
    if !write_reg(i2c, REG_CTRL7, 0x03) {
        info!("QMI8658: failed to write CTRL7");
        return false;
    }

    info!("QMI8658: initialized (±8g, ±512dps, 125Hz)");
    true
}

/// Read accelerometer, gyroscope, and temperature in one burst.
pub fn read(i2c: &mut I2cDriver<'_>) -> Option<ImuReading> {
    // Read temp (2 bytes) + accel (6 bytes) + gyro (6 bytes) = 14 bytes starting at 0x33
    let mut buf = [0u8; 14];
    if !read_regs(i2c, REG_TEMP_L, &mut buf) {
        return None;
    }

    let temp_raw = raw_to_i16(buf[0], buf[1]);
    let temp_c = temp_raw as f32 / 256.0;

    // Accel starts at buf[2] (register 0x35)
    let ax_raw = raw_to_i16(buf[2], buf[3]);
    let ay_raw = raw_to_i16(buf[4], buf[5]);
    let az_raw = raw_to_i16(buf[6], buf[7]);

    // Gyro starts at buf[8] (register 0x3B)
    let gx_raw = raw_to_i16(buf[8], buf[9]);
    let gy_raw = raw_to_i16(buf[10], buf[11]);
    let gz_raw = raw_to_i16(buf[12], buf[13]);

    // Convert to physical units
    let accel_lsb = 32768.0 / ACCEL_SCALE_G;
    let gyro_lsb = 32768.0 / GYRO_SCALE_DPS;

    Some(ImuReading {
        accel_x: ax_raw as f32 / accel_lsb,
        accel_y: ay_raw as f32 / accel_lsb,
        accel_z: az_raw as f32 / accel_lsb,
        gyro_x: gx_raw as f32 / gyro_lsb,
        gyro_y: gy_raw as f32 / gyro_lsb,
        gyro_z: gz_raw as f32 / gyro_lsb,
        temp_c,
    })
}
