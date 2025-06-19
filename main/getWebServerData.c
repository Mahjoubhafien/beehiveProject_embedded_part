/*
 * getWebServerData.c
 *
 *  Created on: 19 Jun 2025
 *      Author: KURAPIKA
 */
#include "driver/gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>
#include <sys/unistd.h>
#include "main.h"

void parse_alert_config(const char *json, AlertConfig_t *config) {
    char alerts_state[6] = {0};

    sscanf(json,
           "{\"MIN_TEMP\":%d,\"MAX_TEMP\":%d,\"MIN_HUMIDITY\":%d,\"MAX_HUMIDITY\":%d,\"isAlertsON\":%5[^}]",
           &config->min_temp,
           &config->max_temp,
           &config->min_humidity,
           &config->max_humidity,
           alerts_state);

    config->is_alerts_on = (strstr(alerts_state, "true") != NULL);
}

void sim800_wait_response_get(char *out_buffer, size_t max_len){
// Read response into buffer (this depends on your existing UART logic)
    int len = uart_read_bytes(UART_PORT, (uint8_t *)out_buffer, max_len, pdMS_TO_TICKS(3000));
    if (len > 0) {
        out_buffer[len] = '\0'; // Null-terminate
        ESP_LOGI("SIM800_RESPONSE", "%s", out_buffer);
    } else {
        out_buffer[0] = '\0';
    }
}
void getWebServerData_task(void *pvParameters) {
    char response_buf[256];
    AlertConfig_t new_config;

	while(1){
	ESP_LOGI("SIM800C", "=== Starting HTTP GET Task ===");

    // Set the URL for the HTTP GET request
    sim800_send_command("AT+HTTPPARA=\"URL\",\"197.14.101.52:5000/api/alert-config\"");  // Set the GET URL to httpbin.org
    sim800_wait_response();

    // Trigger the HTTP GET action
    sim800_send_command("AT+HTTPACTION=0");  // 0 indicates a GET request
    sim800_wait_response();

    // Terminate the HTTP session (clean up resources)
    //sim800_send_command("AT+HTTPTERM");  // Terminate HTTP service
    //sim800_wait_response();
    
    // Read server response (you can print this for debugging)
    sim800_send_command("AT+HTTPREAD");  // Read the server's response
    sim800_wait_response_get(response_buf, sizeof(response_buf));  // ✅ FIXED
    
		char *json_start = strchr(response_buf, '{');
        if (json_start) {
            parse_alert_config(json_start, &new_config);

            xQueueOverwrite(alertConfigQueue, &new_config);

            ESP_LOGI("CONFIG", "Parsed Config: TEMP [%d-%d], HUM [%d-%d], ALERTS: %s",
                     new_config.min_temp,
                     new_config.max_temp,
                     new_config.min_humidity,
                     new_config.max_humidity,
                     new_config.is_alerts_on ? "ON" : "OFF");
        } else {
            ESP_LOGW("SIM800C", "Failed to find JSON in response");
        }
        
    ESP_LOGI("SIM800C", "=== HTTP GET Done ===");
    
   vTaskDelay(pdMS_TO_TICKS(10000));  // 300000 ms = 5 minutes

    }
	
}
