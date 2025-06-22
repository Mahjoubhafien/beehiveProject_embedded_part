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
QueueHandle_t alertConfigQueue;
SemaphoreHandle_t sim800_uart_mutex = NULL;

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
//sim800c init need to be repeated 3 time to be init without error!
void sim800c_init(void){
	
	// Wait for SIM800C boot
	sim800_send_command("AT+CFUN=1,1");
    vTaskDelay(pdMS_TO_TICKS(10000));

    ESP_LOGI("SIM_INIT", "=== SIM800C Initialization Sequence ===");
    
    sim800_send_command("AT");
    sim800_wait_response();
    
    sim800_send_command("AT+CPIN?");
    sim800_wait_response();

    sim800_send_command("AT+CSQ");
    sim800_wait_response();

    sim800_send_command("AT+CREG?");
    sim800_wait_response();

    sim800_send_command("AT+CGATT=1");
    sim800_wait_response();

    sim800_send_command("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    sim800_wait_response();

    sim800_send_command("AT+SAPBR=3,1,\"APN\",\"internet.tn\"");
    sim800_wait_response();

     // Initialize the GPRS connection (activate the bearer profile and connect to the network)
    sim800_send_command("AT+SAPBR=1,1");   // Activate bearer profile
    sim800_wait_response();

    sim800_send_command("AT+SAPBR=2,1");   // Query bearer profile (check if connected)
    sim800_wait_response();
    
    		
    // Initialize HTTP connection
    sim800_send_command("AT+HTTPINIT");    // Initialize HTTP service
    sim800_wait_response();
	
    // Set up the CID (context ID for the bearer profile)
    sim800_send_command("AT+HTTPPARA=\"CID\",1");  // Set CID to 1
    sim800_wait_response();

    // Set the URL for the HTTP POST request
    sim800_send_command("AT+HTTPPARA=\"URL\",\"197.26.165.184:5000/temphum\"");  // Set the POST URL
    sim800_wait_response();
    
    // Specify content type (application/json for JSON data)
    sim800_send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"");  // Set the content type for JSON
    sim800_wait_response();
    
    // Set SMS text mode
    uart_write_bytes(UART_PORT, "AT+CMGF=1\r\n", strlen("AT+CMGF=1\r\n"));


    // Enable redirect
	//sim800_send_command("AT+HTTPPARA=\"REDIR\",1");
	//sim800_wait_response();
/*///////////////SSL/////////////////
	sim800_send_command("AT+FSCREATE=\"ca.crt\"");
	sim800_wait_response();
	
	sim800_send_command("AT+FSWRITE=\"ca.crt\",0,1721,10");
	sim800_wait_response();
	sim800_send_raw("MIIFBjCCAu6gAwIBAgIRAIp9PhPWLzDvI4a9KQdrNPgwDQYJKoZIhvcNAQELBQAwTzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAwWhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3MgRW5jcnlwdDEMMAoGA1UEAxMDUjExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuoe8XBsAOcvKCs3UZxD5ATylTqVhyybKUvsVAbe5KPUoHu0nsyQYOWcJDAjs4DqwO3cOvfPlOVRBDE6uQdaZdN5R2+97/1i9qLcT9t4x1fJyyXJqC4N0lZxGAGQUmfOx2SLZzaiSqhwmej/+71gFewiVgdtxD4774zEJuwm+UE1fj5F2PVqdnoPy6cRms+EGZkNIGIBloDcYmpuEMpexsr3E+BUAnSeI++JjF5ZsmydnS8TbKF5pwnnwSVzgJFDhxLyhBax7QG0AtMJBP6dYuC/FXJuluwme8f7rsIU5/agK70XEeOtlKsLPXzze41xNG/cLJyuqC0J3U095ah2H2QIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQUxc9GpOr0w8B6bJXELbBeki8m47kwHwYDVR0jBBgwFoAUebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAChhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcGA1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcNAQELBQADggIBAE7iiV0KAxyQOND1H/lxXPjDj7I3iHpvsCUf7b632IYGjukJhM1yv4Hz/MrPU0jtvfZpQtSlET41yBOykh0FX+ou1Nj4ScOt9ZmWnO8m2OG0JAtIIE3801S0qcYhyOE2G/93ZCkXufBL713qzXnQv5C/viOykNpKqUgxdKlEC+Hi9i2DcaR1e9KUwQUZRhy5j/PEdEglKg3l9dtD4tuTm7kZtB8v32oOjzHTYw+7KdzdZiw/sBtnUfhBPORNuay4pJxmY/WrhSMdzFO2q3Gu3MUBcdo27goYKjL9CTF8j/Zz55yctUoVaneCWs/ajUX+HypkBTA+c8LGDLnWO2NKq0YD/pnARkAnYGPfUDoHR9gVSp/qRx+ZWghiDLZsMwhN1zjtSC0uBWiugF3vTNzYIEFfaPG7Ws3jDrAMMYebQ95JQ+HIBD/RPBuHRTBpqKlyDnkSHDHYPiNX3adPoPAcgdF3H2/W0rmoswMWgTlLn1Wu0mrks7/qpdWfS6PJ1jty80r2VKsM/Dj3YIDfbjXKdaFU5C+8bhfJGqU3taKauuz0wHVGT3eo6FlWkWYtbt4pgdamlwVeZEW+LM7qZEJEsMNPrfC03APKmZsJgpWCDWOKZvkZcvjVuYkQ4omYCTX5ohy+knMjdOmdH9c7SpqEWBDC86fiNex+O0XOMEZSa8DA");
	uint8_t end_char = 0x1A;
    uart_write_bytes(UART_PORT,  &end_char, sizeof(end_char));
	sim800_send_command("AT+SSLSETROOT=\"ca.crt\"");
	sim800_wait_response();


    // Enable HTTPS
	sim800_send_command("AT+HTTPSSL=1");       
	sim800_wait_response();
*/

    ESP_LOGI("SIM_INIT", "SIM800C Initialization Complete.");
	
}
void app_main(void) {
	// Create the queue with space for 5 SensorData_t elements
// Create queues
    dhtQueue = xQueueCreate(30, sizeof(DHTData_t));
    gpsQueue = xQueueCreate(30, sizeof(GPSData_t));
    espnowQueue = xQueueCreate(30, sizeof(sensor_data_t));  // Buffer 10 messages
    alertConfigQueue = xQueueCreate(1, sizeof(AlertConfig_t));
    if (dhtQueue == NULL || gpsQueue == NULL || espnowQueue == NULL) {
        ESP_LOGE(TAG, "Queue creation failed");
        return;
    }// Initialize NVS
    sim800_uart_mutex = xSemaphoreCreateMutex();
    if (sim800_uart_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create SIM800 UART mutex");
        return;
    }

    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Initialize WiFi and ESP-NOW
    wifi_init();
    espnow_init();
    
	//init Uart
	
	sim800_UART_init();
   	GPS_UART_init();
   	 sim800c_init();
   	 
  /////////////////////////////////////////////// Start tasks ////////////////////////////////////
 
  xTaskCreate(dht_test, "dht21_task", 4096, NULL, 6, NULL);
  
  xTaskCreate(gps_task, "gps_task", 4096, NULL, 6, NULL);
  
  xTaskCreate(sim800c_task, "sim800c_task", 8192, NULL, 6, NULL);
  
  //xTaskCreate(sms_control_task, "sms_control_task", 4096, NULL, 4, NULL);

  xTaskCreate(espnow_receiver_task, "espnow_receiver_task", 4096, NULL, 6, NULL);
 
   xTaskCreate(getWebServerData_task, "getWebServerData_task", 4096, NULL, 6, NULL);

  ESP_LOGI(TAG, "ESP-NOW Receiver initialized and waiting for data...");

}
