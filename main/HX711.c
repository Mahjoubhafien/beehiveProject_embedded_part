#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "HX711.h"

// Initialize GPIOs
void hx711_init(void)
{
    gpio_set_direction(HX711_SCK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(HX711_DT_PIN, GPIO_MODE_INPUT);
    gpio_set_level(HX711_SCK_PIN, 0);
}

// Check if HX711 is ready (DT goes low)
bool hx711_is_ready(void)
{
    return gpio_get_level(HX711_DT_PIN) == 0;
}

// Read 24-bit raw data from HX711
int32_t hx711_read_raw(void)
{

    while (!hx711_is_ready()) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    int32_t count = 0;

    for (uint8_t i = 0; i < 24; i++) {
        gpio_set_level(HX711_SCK_PIN, 1);
		vTaskDelay(pdMS_TO_TICKS(1));
        count = count << 1;
        gpio_set_level(HX711_SCK_PIN, 0);
		vTaskDelay(pdMS_TO_TICKS(1));
        if (gpio_get_level(HX711_DT_PIN)) {
            count++;
        }
    }

    // Set gain = 128 by giving 1 more clock pulse (for channel A, gain 128)
    gpio_set_level(HX711_SCK_PIN, 1);
	vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(HX711_SCK_PIN, 0);
	vTaskDelay(pdMS_TO_TICKS(1));
    // Convert from unsigned to signed 24-bit
    if (count & 0x800000) {
        count |= 0xFF000000; // Sign extend negative number
    }

    return count;
}

// Task to read data continuously
void hx711_task(void *arg)
{
    hx711_init();
    while (1) {
        int32_t raw = hx711_read_raw();
		ESP_LOGI("HX711", "Raw value: %" PRId32, raw);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

	


