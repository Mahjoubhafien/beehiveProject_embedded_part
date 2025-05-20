#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"
#include "main.h"

#define DHT_GPIO    GPIO_NUM_12
#define DHT_TYPE    DHT_TYPE_AM2301





void dht_test(void *pvParameters)
{
    float temperature;
    float humidity;

    // 1) Configure GPIO12 as open-drain, no internal pull-ups
    gpio_config_t io_conf = {
        .pin_bit_mask   = 1ULL << DHT_GPIO,
        .mode           = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 2) Wait for sensor to power-up and stabilize
    ESP_LOGI("DHT21_Example", "Waiting for sensor to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        esp_err_t res = dht_read_float_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature);
        if (res == ESP_OK) {
            ESP_LOGI("DHT21_Example", "Humidity: %.1f %%  Temperature: %.1f C", humidity, temperature);
        } else {
            ESP_LOGE("DHT21_Example", "Failed to read from DHT sensor: %s", esp_err_to_name(res));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
                DHTData_t data;
				data.temperature = temperature;
       			 data.humidity = humidity;
                xQueueSend(dhtQueue, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(2000));  // 2 seconds delay


    }
}
