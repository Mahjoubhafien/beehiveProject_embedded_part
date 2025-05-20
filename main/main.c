#include "driver/gpio.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include "esp_now.h"
#include <sys/unistd.h>
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "gps.h"
#include "main.h"

QueueHandle_t dhtQueue;
QueueHandle_t gpsQueue;
QueueHandle_t espnowQueue;  // Add this with your other queue definitions

typedef struct __attribute__((packed)) {
    char sensor_id[12];
    float temperature;
    float humidity;
    float longitude;
    float latitude;
} sensor_data_t;

// Correct callback signature for ESP-IDF v5.3.1
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
	if (data_len == sizeof(sensor_data_t)) {
	        sensor_data_t received_data;
	        memcpy(&received_data, data, sizeof(sensor_data_t));

	        if (xQueueSend(espnowQueue, &received_data, pdMS_TO_TICKS(100)) != pdTRUE) {
	            ESP_LOGE(TAG, "Failed to send data to queue (queue full?)");
	        } else {
	            ESP_LOGI(TAG, "received data from esp now | ID: %s | Temp: %.2f | Hum: %.2f",
	                     received_data.sensor_id, received_data.temperature, received_data.humidity);
	        }
	    } else {
	        ESP_LOGE(TAG, "Data length mismatch: received %d bytes, expected %d", data_len, sizeof(sensor_data_t));
	    }
	}
void espnow_init() {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}
void espnow_receiver_task(void *pvParameters) {
    while(1) {
        // Just keep the task running - all work is done in the callback
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void wifi_init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}


// UART config
void sim800_UART_init(void)
{
	    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    
    uart_param_config(UART_PORT, &uart_config);

	uart_set_pin(UART_PORT, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
} 

void GPS_UART_init(void)
{
	    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(GPS_UART_PORT, BUF_SIZE * 2, 0, 0, NULL, 0);
    
    uart_param_config(GPS_UART_PORT, &uart_config);

	uart_set_pin(GPS_UART_PORT, GPS_TXD_PIN, GPS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
} 
void app_main(void) {
	// Create the queue with space for 5 SensorData_t elements
// Create queues
    dhtQueue = xQueueCreate(30, sizeof(DHTData_t));
    gpsQueue = xQueueCreate(30, sizeof(GPSData_t));
    espnowQueue = xQueueCreate(30, sizeof(sensor_data_t));  // Buffer 10 messages
    
    if (dhtQueue == NULL || gpsQueue == NULL || espnowQueue == NULL) {
        ESP_LOGE(TAG, "Queue creation failed");
        return;
    }// Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize WiFi and ESP-NOW
    wifi_init();
    espnow_init();
    
	//init Uart
	
	sim800_UART_init();
   	//GPS_UART_init();
   	 
   	 
 //Start tasks
 
  xTaskCreate(dht_test, "dht21_task", 4096, NULL, 5, NULL);
  //xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
  xTaskCreate(sim800c_task, "sim800c_task", 4096, NULL, 5, NULL);
  xTaskCreate(espnow_receiver_task, "espnow_receiver_task", 4096, NULL, 5, NULL);
   ESP_LOGI(TAG, "ESP-NOW Receiver initialized and waiting for data...");


}
