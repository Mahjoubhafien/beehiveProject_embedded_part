/*
 * smsControl.c
 *
 *  Created on: 16 Jun 2025
 *      Author: KURAPIKA
 */

#include "smsControl.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdint.h>
#include <string.h>
#include "main.h"  // for DHTData_t, UART_PORT, etc.

#define TAG_SMS "SMS_CTRL"


extern QueueHandle_t dhtQueue;

// Hardcoded data
#define TEMP_VALUE 27.6
#define HUM_VALUE 81.6
#define GPS_LAT 36.806
#define GPS_LON 10.181


void sim800_send_sms(const char *number, const char *message) {
    char buffer[128];

    if (xSemaphoreTake(sim800_uart_mutex, portMAX_DELAY) == pdTRUE) {
        // Set SMS text mode (very important before sending)
        uart_write_bytes(UART_PORT, "AT+CMGF=1\r\n", strlen("AT+CMGF=1\r\n"));
        vTaskDelay(pdMS_TO_TICKS(500));

        // Prepare the CMGS command
        sprintf(buffer, "AT+CMGS=\"%s\"\r\n", number);
        uart_write_bytes(UART_PORT, buffer, strlen(buffer));
        vTaskDelay(pdMS_TO_TICKS(500));  // Wait for '>' prompt

        // Send the message body
        uart_write_bytes(UART_PORT, message, strlen(message));
        uart_write_bytes(UART_PORT, "\x1A", 1);  // CTRL+Z to send
        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for sending to complete

        ESP_LOGI(TAG_SMS, "SMS sent to %s: %s", number, message);

        xSemaphoreGive(sim800_uart_mutex);
    } else {
        ESP_LOGE(TAG_SMS, "Failed to take sim800_uart_mutex in sim800_send_sms");
    }
}

void sms_control_task(void *pvParameters) {
    uint8_t rx_buffer[512];
    int total_len = 0, len = 0;  // Declare here

    while (1) {
        total_len = 0; // reset for each loop

        if (xSemaphoreTake(sim800_uart_mutex, portMAX_DELAY) == pdTRUE) {
            // Request all messages (use "REC UNREAD" later)
            uart_write_bytes(UART_PORT, "AT+CMGL=\"REC UNREAD\"\r\n", strlen("AT+CMGL=\"REC UNREAD\"\r\n"));
            vTaskDelay(pdMS_TO_TICKS(500));  // Wait for SIM800C to respond

            // Read response
            do {
                len = uart_read_bytes(UART_PORT, rx_buffer + total_len,
                                      sizeof(rx_buffer) - 1 - total_len, pdMS_TO_TICKS(300));
                if (len > 0) total_len += len;
            } while (len > 0 && total_len < sizeof(rx_buffer) - 1);

            rx_buffer[total_len] = '\0';

            xSemaphoreGive(sim800_uart_mutex);
        } else {
            ESP_LOGE(TAG_SMS, "Failed to take sim800_uart_mutex in sms_control_task");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Avoid tight loop on failure
            continue;
        }

        ESP_LOGI(TAG_SMS, "Len = %d", total_len);
        ESP_LOGI(TAG_SMS, "RAW SMS:\n%s", rx_buffer);

        // Parse line-by-line
        char *line = strtok((char *)rx_buffer, "\r\n");
        while (line != NULL) {
            if (strstr(line, "+CMGL:")) {
                // Parse the index and number
                int index = 0;
                char number[32] = {0};

                if (sscanf(line, "+CMGL: %d,\"REC READ\",\"%31[^\"]", &index, number) != 2) {
                    // Try REC UNREAD if needed
                    sscanf(line, "+CMGL: %d,\"REC UNREAD\",\"%31[^\"]", &index, number);
                }

                // Get next line (message body)
                char *body = strtok(NULL, "\r\n");
                if (body != NULL && index > 0) {
                    ESP_LOGI(TAG_SMS, "SMS from %s: %s", number, body);

                    // Prepare reply
                    char reply[128] = {0};
                    if (strstr(body, "TEMP")) {
                        DHTData_t latest_dht;
                        xQueueReceive(dhtQueue, &latest_dht, 0);
                        snprintf(reply, sizeof(reply), "Temp: %.1f C", latest_dht.temperature);
                    } else if (strstr(body, "HUM")) {
                        DHTData_t latest_dht;
                        xQueuePeek(dhtQueue, &latest_dht, 0);
                        snprintf(reply, sizeof(reply), "Humidity: %.1f %%", latest_dht.humidity);
                    } else if (strstr(body, "GPS")) {
                        GPSData_t latest_GPS;
                        xQueueReceive(gpsQueue, &latest_GPS, 0);
                        snprintf(reply, sizeof(reply), "Location: https://maps.google.com/?q=%.6f,%.6f", latest_GPS.latitude, latest_GPS.longitude);
                    } else if (strstr(body, "ALL")) {
                        GPSData_t latest_GPS;
                        DHTData_t latest_dht;
                        xQueuePeek(dhtQueue, &latest_dht, 0);
                        xQueueReceive(gpsQueue, &latest_GPS, 0);
                        snprintf(reply, sizeof(reply),
                                 "Temp: %.1f C, Hum: %.1f %%, Location: https://maps.google.com/?q=%.6f,%.6f",
                                 latest_dht.temperature, latest_dht.humidity, latest_GPS.latitude, latest_GPS.longitude);
                    } else {
                        snprintf(reply, sizeof(reply), "Unknown command");
                    }

                    // Send reply (this function already handles mutex)
                    sim800_send_sms(number, reply);
                    ESP_LOGI(TAG_SMS, "Sent SMS to %s: %s", number, reply);

                    if (xSemaphoreTake(sim800_uart_mutex, portMAX_DELAY) == pdTRUE) {
                        // Delete processed message
                        char del_cmd[32];
                        snprintf(del_cmd, sizeof(del_cmd), "AT+CMGD=%d\r\n", index);
                        uart_write_bytes(UART_PORT, del_cmd, strlen(del_cmd));
                        vTaskDelay(pdMS_TO_TICKS(300));
                        xSemaphoreGive(sim800_uart_mutex);
                    } else {
                        ESP_LOGE(TAG_SMS, "Failed to take sim800_uart_mutex to delete SMS");
                    }
                }
            }
            line = strtok(NULL, "\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));  // Wait before next check (5 seconds)
    }
}