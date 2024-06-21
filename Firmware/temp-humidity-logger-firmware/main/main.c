#include <stdio.h>
#include "driver/i2c.h"
#include "aht20.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <float.h>
#include "ssd1306.h"
#include "font8x8_basic.h"

#define I2C_MASTER_SCL_IO   6   /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO   7   /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM      I2C_NUM_0               /*!< I2C port number for master dev */

static aht20_dev_handle_t aht20 = NULL;

static const char *TAG = "TEMP_METER";
SSD1306_t dev;

void i2c_init_() {
    const i2c_config_t i2c_conf2 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };

    i2c_param_config(I2C_MASTER_NUM, &i2c_conf2);
    //i2c_driver_install(I2C_MASTER_NUM, i2c_conf2.mode, 0, 0, 0);
}

void i2c_sensor_ath20_init(void) {
    aht20_i2c_config_t i2c_conf = {
        .i2c_port = I2C_MASTER_NUM,
        .i2c_addr = AHT20_ADDRRES_0,
    };
    i2c_init_();
    aht20_new_sensor(&i2c_conf, &aht20);
}


void app_main(void) {
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    nvs_handle_t my_handle;
    ret = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS handle!", esp_err_to_name(ret));
        return;
    }
    uint32_t temperature_raw;
    uint32_t humidity_raw;
    float temperature;
    float humidity;
    float max_temp = 0;
    float min_temp = FLT_MAX;
    size_t required_size = 4;
    // Read max temp
    ret = nvs_get_blob(my_handle, "max_temp", NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        max_temp = 0; // Default value
        nvs_set_blob(my_handle, "max_temp", &max_temp, sizeof(max_temp));
    } else {
        nvs_get_blob(my_handle, "max_temp", &max_temp, &required_size);
    }
    // Read min temp
    ret = nvs_get_blob(my_handle, "min_temp", NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        min_temp = FLT_MAX; // Default value
        nvs_set_blob(my_handle, "min_temp", &min_temp, sizeof(min_temp));
    } else {
        nvs_get_blob(my_handle, "min_temp", &min_temp, &required_size);
    }
    printf("min temp: %.2f \n", min_temp);
    // Commit initial values
    ret = nvs_commit(my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS!", esp_err_to_name(ret));
    }
    i2c_master_init(&dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, -1);
    i2c_sensor_ath20_init();
    
    ssd1306_init(&dev, 128, 64);
    ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
    char* buf = (char*)malloc(4 * sizeof(char));
    char* buf2 = (char*)malloc(20 * sizeof(char));
    while(1) {
        
        aht20_read_temperature_humidity(aht20, &temperature_raw, &temperature, &humidity_raw, &humidity);
        if (temperature > max_temp) {
            max_temp = temperature;
        }
        if(temperature < min_temp) {
            min_temp = temperature;
        }
        ESP_LOGI("AHT20", "Temp: %.2f, Max temp: %.2f, Min temp: %.2f", temperature, max_temp, min_temp);
        snprintf(buf, 10, "%.1f", temperature);
        snprintf(buf2, 20, "%.1f        %.1f", min_temp, max_temp);
        ssd1306_display_text_x3(&dev, 0, buf, sizeof(buf), false);
        ssd1306_display_text(&dev, 5, "Min         Max", 20, false);
        ssd1306_display_text(&dev, 7, buf2, 20, false);
        ret = nvs_set_blob(my_handle, "max_temp", &max_temp, sizeof(max_temp));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Error (%s) setting max_temp!", esp_err_to_name(ret));
            }
        ret = nvs_set_blob(my_handle, "min_temp", &min_temp, sizeof(min_temp));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Error (%s) setting min_temp!", esp_err_to_name(ret));
            }
        // Commit updated values to NVS
        ret = nvs_commit(my_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) committing NVS!", esp_err_to_name(ret));
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    aht20_del_sensor(aht20);
    i2c_driver_delete(I2C_MASTER_NUM);
}
