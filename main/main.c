#include "driver/gpio.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <sys/unistd.h>
#include "esp_http_client.h"
#include "gps.h"
#include "main.h"
#include "nvs_flash.h"


static const char *WIFI_LOG_TAG = "WIFI_SETUP";
static EventGroupHandle_t wifi_event_group;

QueueHandle_t dhtQueue;
QueueHandle_t gpsQueue;




// WIFI STA Config
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(WIFI_LOG_TAG, "Connecting to WiFi...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(WIFI_LOG_TAG, "Disconnected. Trying to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(WIFI_LOG_TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_LOG_TAG, "WiFi Station Initialized.");
}
void ESP32_STA_START(void)
{
	    ESP_ERROR_CHECK(nvs_flash_init());
    
    wifi_init_sta();

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_LOG_TAG, "Connected to WiFi. Ready to go!");
    } else {
        ESP_LOGE(WIFI_LOG_TAG, "Failed to connect to WiFi.");
    }
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
	dhtQueue = xQueueCreate(5, sizeof(DHTData_t));
	gpsQueue = xQueueCreate(5, sizeof(GPSData_t));
    if (dhtQueue == NULL && gpsQueue==NULL ) {
        printf("Queue creation failed\n");
        return;
    }

	//init Uart
	 sim800_UART_init();
   	 GPS_UART_init();
 //Start tasks
  xTaskCreate(dht_test, "dht21_task", 4096, NULL, 5, NULL);
  xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL);
  xTaskCreate(sim800c_task, "sim800c_task", 4096, NULL, 5, NULL);


   	//ESP32_STA_START();
   	//send_sensor_data(50, 10);
}