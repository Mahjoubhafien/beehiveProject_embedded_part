/*
 * gps.c
 *
 *  Created on: 11 Apr 2025
 *      Author: KURAPIKA
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "gps.h"
#include "main.h"


// FreeRTOS Task to handle GPS data
void gps_task(void *arg) {
    uint8_t *data = (uint8_t *)malloc(RD_BUF_SIZE);
    char gpgga_sentence[RD_BUF_SIZE] = {0};  // Initialize buffer

    while (1) {
        ESP_LOGI("GPS", "GPS Task running...");

        // Read data from UART
        int len = uart_read_bytes(GPS_UART_PORT, data, RD_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGD("GPS", "Raw data: %s", data);  // Debug log

            char *start = strstr((char *)data, "$GPGGA");
            if (start != NULL) {
                char *end = strstr(start, "\r\n");
                if (end != NULL) {
                    size_t gpgga_len = end - start;
                    if (gpgga_len < sizeof(gpgga_sentence)) {
                        memset(gpgga_sentence, 0, sizeof(gpgga_sentence));
                        strncpy(gpgga_sentence, start, gpgga_len);
                        
                        // Check if GPS has fix (field 7 should be '1' or '2')
                        char *fix_field = strchr(gpgga_sentence + 6, ','); // Skip to field 6
                        if ((fix_field && *(fix_field + 1) == '1') || *(fix_field + 1) == '2') {
                            // Parse the GPGGA sentence
                            char *token;
                            int field_index = 0;
                            char *latitude_str = NULL;
                            char *lat_dir = NULL;
                            char *longitude_str = NULL;
                            char *lon_dir = NULL;

                            token = strtok(gpgga_sentence, ",");
                            while (token != NULL) {
                                field_index++;

                                if (field_index == 3) latitude_str = token;
                                if (field_index == 4) lat_dir = token;
                                if (field_index == 5) longitude_str = token;
                                if (field_index == 6) lon_dir = token;

                                token = strtok(NULL, ",");
                            }

                            if (latitude_str && lat_dir && longitude_str && lon_dir) {
                                // Convert NMEA to decimal
                                float lat_deg = atof(latitude_str) / 100.0;
                                float lat_min = lat_deg - (int)lat_deg;
                                float latitude = (int)lat_deg + (lat_min * 100.0) / 60.0;
                                if (lat_dir[0] == 'S') latitude *= -1;

                                float lon_deg = atof(longitude_str) / 100.0;
                                float lon_min = lon_deg - (int)lon_deg;
                                float longitude = (int)lon_deg + (lon_min * 100.0) / 60.0;
                                if (lon_dir[0] == 'W') longitude *= -1;

                                GPSData_t gpsdata = {
                                    .latitude = latitude,
                                    .longitude = longitude
                                };

                                xQueueSend(gpsQueue, &gpsdata, pdMS_TO_TICKS(100));                   
                                ESP_LOGI("GPS", "Lat: %.6f, Lon: %.6f", latitude, longitude);
                            }
                        } else {
                            ESP_LOGW("GPS", "No GPS fix available");
                        }
                    }
                }
            }
        } else {
            ESP_LOGE("GPS", "No data received from GPS");
        }

        vTaskDelay(pdMS_TO_TICKS(1500));
    }

    free(data);
    vTaskDelete(NULL);
}