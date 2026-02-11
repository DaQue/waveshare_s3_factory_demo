#include "bsp_bme280.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "bsp_i2c.h"

#define BME280_ADDR_PRIMARY 0x76
#define BME280_ADDR_SECONDARY 0x77

#define BME280_REG_CHIP_ID 0xD0
#define BME280_REG_RESET 0xE0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS 0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_PRESS_MSB 0xF7

#define BME280_CHIP_ID 0x60
#define BME280_RESET_CMD 0xB6

static const char *TAG = "bsp_bme280";

typedef struct {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
} bme280_calib_t;

static i2c_master_dev_handle_t s_dev_handle = NULL;
static bool s_available = false;
static uint8_t s_address = 0;
static bme280_calib_t s_calib = {};
static int32_t s_t_fine = 0;

static int16_t sign_extend_12(uint16_t raw)
{
    if (raw & 0x0800)
    {
        raw |= 0xF000;
    }
    return (int16_t)raw;
}

static esp_err_t bme280_reg_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (s_dev_handle == NULL || data == NULL || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
    if (bsp_i2c_lock(100))
    {
        ret = i2c_master_transmit_receive(s_dev_handle, &reg_addr, 1, data, len, pdMS_TO_TICKS(1000));
        bsp_i2c_unlock();
    }
    return ret;
}

static esp_err_t bme280_reg_write_u8(uint8_t reg_addr, uint8_t value)
{
    if (s_dev_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[2] = {reg_addr, value};
    esp_err_t ret = ESP_FAIL;
    if (bsp_i2c_lock(100))
    {
        ret = i2c_master_transmit(s_dev_handle, buf, sizeof(buf), pdMS_TO_TICKS(1000));
        bsp_i2c_unlock();
    }
    return ret;
}

static esp_err_t bme280_read_calibration(void)
{
    uint8_t part1[26] = {0};
    uint8_t part2[7] = {0};

    esp_err_t ret = bme280_reg_read(0x88, part1, sizeof(part1));
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = bme280_reg_read(0xE1, part2, sizeof(part2));
    if (ret != ESP_OK)
    {
        return ret;
    }

    s_calib.dig_t1 = (uint16_t)((part1[1] << 8) | part1[0]);
    s_calib.dig_t2 = (int16_t)((part1[3] << 8) | part1[2]);
    s_calib.dig_t3 = (int16_t)((part1[5] << 8) | part1[4]);

    s_calib.dig_p1 = (uint16_t)((part1[7] << 8) | part1[6]);
    s_calib.dig_p2 = (int16_t)((part1[9] << 8) | part1[8]);
    s_calib.dig_p3 = (int16_t)((part1[11] << 8) | part1[10]);
    s_calib.dig_p4 = (int16_t)((part1[13] << 8) | part1[12]);
    s_calib.dig_p5 = (int16_t)((part1[15] << 8) | part1[14]);
    s_calib.dig_p6 = (int16_t)((part1[17] << 8) | part1[16]);
    s_calib.dig_p7 = (int16_t)((part1[19] << 8) | part1[18]);
    s_calib.dig_p8 = (int16_t)((part1[21] << 8) | part1[20]);
    s_calib.dig_p9 = (int16_t)((part1[23] << 8) | part1[22]);

    s_calib.dig_h1 = part1[25];
    s_calib.dig_h2 = (int16_t)((part2[1] << 8) | part2[0]);
    s_calib.dig_h3 = part2[2];
    s_calib.dig_h4 = sign_extend_12((uint16_t)((part2[3] << 4) | (part2[4] & 0x0F)));
    s_calib.dig_h5 = sign_extend_12((uint16_t)((part2[5] << 4) | (part2[4] >> 4)));
    s_calib.dig_h6 = (int8_t)part2[6];

    return ESP_OK;
}

static esp_err_t bme280_configure(void)
{
    esp_err_t ret = bme280_reg_write_u8(BME280_REG_RESET, BME280_RESET_CMD);
    if (ret != ESP_OK)
    {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(5));

    ret = bme280_reg_write_u8(BME280_REG_CTRL_HUM, 0x01); // Humidity oversampling x1
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = bme280_reg_write_u8(BME280_REG_CTRL_MEAS, 0x27); // Temp x1, press x1, normal mode
    if (ret != ESP_OK)
    {
        return ret;
    }

    return bme280_reg_write_u8(BME280_REG_CONFIG, 0xA0); // 1000ms standby
}

static bool bme280_probe_addr(i2c_master_bus_handle_t bus_handle, uint8_t addr)
{
    esp_err_t ret = ESP_FAIL;
    if (bsp_i2c_lock(100))
    {
        ret = i2c_master_probe(bus_handle, addr, 100);
        bsp_i2c_unlock();
    }
    return ret == ESP_OK;
}

esp_err_t bsp_bme280_init(i2c_master_bus_handle_t bus_handle)
{
    if (s_available)
    {
        return ESP_OK;
    }

    uint8_t addresses[] = {BME280_ADDR_PRIMARY, BME280_ADDR_SECONDARY};
    for (size_t i = 0; i < sizeof(addresses); ++i)
    {
        uint8_t addr = addresses[i];
        if (!bme280_probe_addr(bus_handle, addr))
        {
            continue;
        }

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };

        i2c_master_dev_handle_t candidate = NULL;
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &candidate);
        if (ret != ESP_OK)
        {
            continue;
        }

        s_dev_handle = candidate;
        s_address = addr;

        uint8_t chip_id = 0;
        ret = bme280_reg_read(BME280_REG_CHIP_ID, &chip_id, 1);
        if (ret != ESP_OK || chip_id != BME280_CHIP_ID)
        {
            i2c_master_bus_rm_device(candidate);
            s_dev_handle = NULL;
            continue;
        }

        ret = bme280_read_calibration();
        if (ret != ESP_OK)
        {
            i2c_master_bus_rm_device(candidate);
            s_dev_handle = NULL;
            continue;
        }

        ret = bme280_configure();
        if (ret != ESP_OK)
        {
            i2c_master_bus_rm_device(candidate);
            s_dev_handle = NULL;
            continue;
        }

        s_available = true;
        ESP_LOGI(TAG, "BME280 initialized at address 0x%02X", s_address);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "BME280 not detected (checked 0x76, 0x77)");
    return ESP_ERR_NOT_FOUND;
}

bool bsp_bme280_is_available(void)
{
    return s_available;
}

esp_err_t bsp_bme280_read(bsp_bme280_data_t *out)
{
    if (out == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_available)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t status = 0;
    esp_err_t ret = bme280_reg_read(BME280_REG_STATUS, &status, 1);
    if (ret != ESP_OK)
    {
        return ret;
    }
    if (status & 0x08)
    {
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    uint8_t raw[8] = {0};
    ret = bme280_reg_read(BME280_REG_PRESS_MSB, raw, sizeof(raw));
    if (ret != ESP_OK)
    {
        return ret;
    }

    int32_t adc_p = (int32_t)((((uint32_t)raw[0]) << 12) | (((uint32_t)raw[1]) << 4) | ((raw[2] >> 4) & 0x0F));
    int32_t adc_t = (int32_t)((((uint32_t)raw[3]) << 12) | (((uint32_t)raw[4]) << 4) | ((raw[5] >> 4) & 0x0F));
    int32_t adc_h = (int32_t)((((uint32_t)raw[6]) << 8) | raw[7]);

    if (adc_t == 0x80000 || adc_p == 0x80000)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    int32_t var1 = ((((adc_t >> 3) - ((int32_t)s_calib.dig_t1 << 1))) * ((int32_t)s_calib.dig_t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)s_calib.dig_t1)) * ((adc_t >> 4) - ((int32_t)s_calib.dig_t1))) >> 12) * ((int32_t)s_calib.dig_t3)) >> 14;
    s_t_fine = var1 + var2;
    int32_t temp_x100 = (s_t_fine * 5 + 128) >> 8;
    float temp_c = (float)temp_x100 / 100.0f;

    int64_t p_var1 = ((int64_t)s_t_fine) - 128000;
    int64_t p_var2 = p_var1 * p_var1 * (int64_t)s_calib.dig_p6;
    p_var2 = p_var2 + ((p_var1 * (int64_t)s_calib.dig_p5) << 17);
    p_var2 = p_var2 + (((int64_t)s_calib.dig_p4) << 35);
    p_var1 = ((p_var1 * p_var1 * (int64_t)s_calib.dig_p3) >> 8) + ((p_var1 * (int64_t)s_calib.dig_p2) << 12);
    p_var1 = (((((int64_t)1) << 47) + p_var1) * (int64_t)s_calib.dig_p1) >> 33;

    float pressure_hpa = 0.0f;
    if (p_var1 != 0)
    {
        int64_t p = 1048576 - adc_p;
        p = (((p << 31) - p_var2) * 3125) / p_var1;
        p_var1 = (((int64_t)s_calib.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
        p_var2 = (((int64_t)s_calib.dig_p8) * p) >> 19;
        p = ((p + p_var1 + p_var2) >> 8) + (((int64_t)s_calib.dig_p7) << 4);
        pressure_hpa = (float)(p / 256.0) / 100.0f;
    }

    int32_t h_x1024 = 0;
    if (adc_h != 0x8000)
    {
        int32_t h = s_t_fine - 76800;
        h = (((((adc_h << 14) - (((int32_t)s_calib.dig_h4) << 20) - (((int32_t)s_calib.dig_h5) * h)) + 16384) >> 15) *
             (((((((h * ((int32_t)s_calib.dig_h6)) >> 10) * (((h * ((int32_t)s_calib.dig_h3)) >> 11) + 32768)) >> 10) + 2097152) *
               ((int32_t)s_calib.dig_h2) +
               8192) >>
              14));
        h = h - (((((h >> 15) * (h >> 15)) >> 7) * ((int32_t)s_calib.dig_h1)) >> 4);
        h = (h < 0 ? 0 : h);
        h = (h > 419430400 ? 419430400 : h);
        h_x1024 = h >> 12;
    }

    float humidity = (float)h_x1024 / 1024.0f;
    if (humidity < 0.0f)
    {
        humidity = 0.0f;
    }
    if (humidity > 100.0f)
    {
        humidity = 100.0f;
    }

    out->temperature_c = temp_c;
    out->temperature_f = temp_c * 1.8f + 32.0f;
    out->humidity_pct = humidity;
    out->pressure_hpa = pressure_hpa;
    return ESP_OK;
}
