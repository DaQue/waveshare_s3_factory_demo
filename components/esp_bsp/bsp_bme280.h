#ifndef __BSP_BME280_H__
#define __BSP_BME280_H__

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    float temperature_c;
    float temperature_f;
    float humidity_pct;
    float pressure_hpa;
} bsp_bme280_data_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_bme280_init(i2c_master_bus_handle_t bus_handle);
bool bsp_bme280_is_available(void);
esp_err_t bsp_bme280_read(bsp_bme280_data_t *out);

#ifdef __cplusplus
}
#endif

#endif // __BSP_BME280_H__
