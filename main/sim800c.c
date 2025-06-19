/*
 * sim800c.c
 *
 *  Created on: 8 Apr 2025
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
#include"sim800c.h"
#include "main.h"

// SMS Alerts Related Parameters
bool isAlertsON = true;
bool sim800Reset = false;
bool isAlertStopLocal = false;
bool isAlertStopRemote = false;
int alertCounterLocal = 0;
int alertCounterRemote = 0;
float MIN_TEMP = 32;
float MAX_TEMP = 36;
float MIN_HUMIDITY = 50;
float MAX_HUMIDITY = 70;

static const char *SIM_TAG = "SIM800C";

TaskHandle_t sim800_task_handle = NULL;

uint8_t error_count=0;

    DHTData_t latestDHT = {0};  // Initialize to avoid garbage
    GPSData_t latestGPS = {0};
    ESPNowData_t latestESPNow = {0};
    
    
    
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

        ESP_LOGI(SIM_TAG, "SMS sent to %s: %s", number, message);

        xSemaphoreGive(sim800_uart_mutex);
    } else {
        ESP_LOGE(SIM_TAG, "Failed to take sim800_uart_mutex in sim800_send_sms");
    }
}

/*
void sim800c_reset(void){
	
	ESP_LOGI(SIM_TAG, "=== Start SIM800C Reset ===");

	sim800_send_command("AT+CFUN=1,1");
    vTaskDelay(pdMS_TO_TICKS(10000));

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
    sim800_send_command("AT+HTTPPARA=\"URL\",\"http://197.14.101.52:5000/temphum\"");  // Set the POST URL
    sim800_wait_response();
    
    // Specify content type (application/json for JSON data)
    sim800_send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"");  // Set the content type for JSON
    sim800_wait_response();

    
    // Enable redirect
	sim800_send_command("AT+HTTPPARA=\"REDIR\",1");
	sim800_wait_response();
	
}*/

void sim800_send_command(const char *cmd) {
    if (xSemaphoreTake(sim800_uart_mutex, portMAX_DELAY) == pdTRUE) {
        uart_flush(UART_PORT);
        uart_write_bytes(UART_PORT, cmd, strlen(cmd));
        uart_write_bytes(UART_PORT, "\r\n", 2);  // CR+LF
        xSemaphoreGive(sim800_uart_mutex);
    } else {
        ESP_LOGE("SIM800C", "Failed to take mutex for send_command");
    }
}

void sim800_send_raw(const char *data) {
    if (xSemaphoreTake(sim800_uart_mutex, portMAX_DELAY) == pdTRUE) {
        uart_write_bytes(UART_PORT, data, strlen(data));
        ESP_LOGI("SIM800C", "Sent raw data: %s", data);
        xSemaphoreGive(sim800_uart_mutex);
    } else {
        ESP_LOGE("SIM800C", "Failed to take mutex for send_raw");
    }
}

void sim800_wait_response() {
    if (xSemaphoreTake(sim800_uart_mutex, portMAX_DELAY) == pdTRUE) {
        uint8_t data[BUF_SIZE];
        memset(data, 0, sizeof(data));

        int len = uart_read_bytes(UART_PORT, data, BUF_SIZE - 1, pdMS_TO_TICKS(AT_CMD_TIMEOUT_MS));
        if (len > 0) {
            data[len] = '\0';
            if (strstr((char *)data, "ERROR") != NULL) {
                error_count++;
                ESP_LOGE(SIM_TAG, "Response Error:\n%s", (char *)data);
                ESP_LOGW(SIM_TAG, "Error Number %d", error_count);
                if (error_count >= 5) {
    				ESP_LOGI(SIM_TAG, "=== SIM800C Reset ===");
                    error_count = 0;
                    //sim800c_reset();
                    sim800Reset = true;
                }
            } else {
                ESP_LOGI(SIM_TAG, "Response:\n%s", (char *)data);
            }
        } else {
            ESP_LOGW(SIM_TAG, "No response or timeout");
        }

        xSemaphoreGive(sim800_uart_mutex);
    } else {
        ESP_LOGE("SIM800C", "Failed to take mutex for wait_response");
    }
}

// === SIM800C Task ===
void sim800c_task(void *pvParameters) {
	
    // Wait for SIM800C boot
/*
	sim800_send_command("AT+CFUN=1,1");
    vTaskDelay(pdMS_TO_TICKS(10000));

    ESP_LOGI(SIM_TAG, "=== SIM800C Initialization Sequence ===");
    
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
    sim800_send_command("AT+HTTPPARA=\"URL\",\"102.157.170.74:5000/temphum\"");  // Set the POST URL
    sim800_wait_response();
    
    // Specify content type (application/json for JSON data)
    sim800_send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"");  // Set the content type for JSON
    sim800_wait_response();

    // Enable redirect
	//sim800_send_command("AT+HTTPPARA=\"REDIR\",1");
	//sim800_wait_response();
///////////////SSL/////////////////
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

    ESP_LOGI(SIM_TAG, "SIM800C Initialization Complete.");
*/

    ///////////// Start the POST Reaquest /////////////////
	sim800_http_post_task();

	
    //sim800_http_get_task();
	//sim800_http_post_data_task(25,10,90,"Tunis");


    // Optionally suspend or delete the task
    //vTaskDelete(NULL);
    
    
         // Wait for 5 minutes before repeating the sequence
       // vTaskDelay(pdMS_TO_TICKS(30000));  // 300000 ms = 5 minutes


}
void sim800_http_post_task(void) {

	while(1)
    {
  if (!sim800Reset){
			
		DHTData_t tempDHT;
		GPSData_t tempGPS;
        ESPNowData_t tempESPNow;
		// Try to receive new DHT data
		if (xQueueReceive(dhtQueue, &tempDHT, pdMS_TO_TICKS(10))) {
  		  latestDHT = tempDHT;  // Update persistent copy
			}

		// Try to receive new GPS data
		if (xQueueReceive(gpsQueue, &tempGPS, pdMS_TO_TICKS(2000))) {
  		  latestGPS = tempGPS;  // Update persistent copy
  		  }  	
  		 // Try to receive new ESP-NOW data (non-blocking)
        if (xQueueReceive(espnowQueue, &tempESPNow, pdMS_TO_TICKS(10))) {
            latestESPNow = tempESPNow;
        }
        // Construct the JSON payload with all available data
        char json_payload[512];  // Increased size to accommodate all data
        snprintf(json_payload, sizeof(json_payload),
            "{"
            "\"local_sensor\":{"
           		 "\"id\":\"sensor-001\","
                "\"temperature\":%.2f,"
                "\"humidity\":%.2f,"
                "\"latitude\":%.6f,"
                "\"longitude\":%.6f"
            "},"
            "\"remote_sensor\":{"
                "\"id\":\"%s\","
                "\"temperature\":%.2f,"
                "\"humidity\":%.2f,"
                "\"latitude\":%.6f,"
                "\"longitude\":%.6f"
            "}"
            "}",
            // Local sensor data (DHT + GPS)
            latestDHT.temperature,
            latestDHT.humidity,
            latestGPS.latitude,
            latestGPS.longitude,
            // Remote sensor data (ESP-NOW)
            latestESPNow.sensor_id,
            latestESPNow.temperature,
            latestESPNow.humidity,
            latestESPNow.latitude,
            latestESPNow.longitude
        );
        
AlertConfig_t active_config;
if (xQueuePeek(alertConfigQueue, &active_config, pdMS_TO_TICKS(10)) == pdPASS) {  // Non-blocking read

    // === LOCAL SENSOR ALERT HANDLER ===
    if (active_config.is_alerts_on && !isAlertStopLocal) {
        char alert_msg[256];

        if (!(latestDHT.temperature <= active_config.max_temp && latestDHT.temperature >= active_config.min_temp)) {
            snprintf(alert_msg, sizeof(alert_msg),
                "ALERT from sensor-001:\nHigh Temp: %.2f C\nLocation: https://maps.google.com/?q=%.6f,%.6f",
                latestDHT.temperature,
                latestGPS.latitude,
                latestGPS.longitude
            );
            sim800_send_sms(user_phone_number, alert_msg);
            alertCounterLocal++;
        }
        else if (!(latestDHT.humidity <= active_config.max_humidity && latestDHT.humidity >= active_config.min_humidity)) {
            snprintf(alert_msg, sizeof(alert_msg),
                "ALERT from sensor-001:\nHigh Humidity: %.2f%%\nLocation: https://maps.google.com/?q=%.6f,%.6f",
                latestDHT.humidity,
                latestGPS.latitude,
                latestGPS.longitude
            );
            sim800_send_sms(user_phone_number, alert_msg);
            alertCounterLocal++;
        }

        if (alertCounterLocal >= 5) {
            ESP_LOGI("SIM800C", "Calling due to local sensor alert...");
            char call_cmd[64];
            snprintf(call_cmd, sizeof(call_cmd), "ATD%s;", user_phone_number);
            sim800_send_command(call_cmd);
            sim800_wait_response();
            alertCounterLocal = 0;
            isAlertStopLocal = true;
        }
    }

    // === REMOTE SENSOR ALERT HANDLER ===
    if (active_config.is_alerts_on && !isAlertStopRemote) {
        char alert_msg[256];

        if (!(latestESPNow.temperature <= active_config.max_temp && latestESPNow.temperature >= active_config.min_temp)) {
            snprintf(alert_msg, sizeof(alert_msg),
                "ALERT from %s:\nHigh Temp: %.2f C\nLocation: https://maps.google.com/?q=%.6f,%.6f",
                latestESPNow.sensor_id,
                latestESPNow.temperature,
                latestESPNow.latitude,
                latestESPNow.longitude
            );
            sim800_send_sms(user_phone_number, alert_msg);
            alertCounterRemote++;
        }
        else if (!(latestESPNow.humidity <= active_config.max_humidity && latestESPNow.humidity >= active_config.min_humidity)) {
            snprintf(alert_msg, sizeof(alert_msg),
                "ALERT from %s:\nHigh Humidity: %.2f%%\nLocation: https://maps.google.com/?q=%.6f,%.6f",
                latestESPNow.sensor_id,
                latestESPNow.humidity,
                latestESPNow.latitude,
                latestESPNow.longitude
            );
            sim800_send_sms(user_phone_number, alert_msg);
            alertCounterRemote++;
        }

        if (alertCounterRemote >= 5) {
            ESP_LOGI("SIM800C", "Calling due to remote sensor alert...");
            char call_cmd[64];
            snprintf(call_cmd, sizeof(call_cmd), "ATD%s;", user_phone_number);
            sim800_send_command(call_cmd);
            sim800_wait_response();
            alertCounterRemote = 0;
            isAlertStopRemote = true;
        }
    }

    // === Reset LOCAL alert if safe ===
    if ((latestDHT.temperature <= active_config.max_temp && latestDHT.temperature >= active_config.min_temp) &&
        (latestDHT.humidity <= active_config.max_humidity && latestDHT.humidity >= active_config.min_humidity)) {
        isAlertStopLocal = false;
        alertCounterLocal = 0;
    }

    // === Reset REMOTE alert if safe ===
    if ((latestESPNow.temperature <= active_config.max_temp && latestESPNow.temperature >= active_config.min_temp) &&
        (latestESPNow.humidity <= active_config.max_humidity && latestESPNow.humidity >= active_config.min_humidity)) {
        isAlertStopRemote = false;
        alertCounterRemote = 0;
    }

} else {
    ESP_LOGW("ALERT", "No configuration available in alertConfigQueue.");
}


    ESP_LOGI("HTTP POST", "Sending JSON: %s", json_payload);
	
    ESP_LOGI("SIM800C", "=== Starting HTTP POST Task ===");
    
    // Prepare to send data (specify the length of the data to be sent)
	char cmd[32];
	sprintf(cmd, "AT+HTTPDATA=%d,10000", strlen(json_payload));

	sim800_send_command(cmd);       // Send AT+HTTPDATA with correct length
	sim800_wait_response();

    // Send the actual data (Post data in JSON format)
    sim800_send_raw(json_payload);  // POST data in JSON format
    sim800_wait_response();
    
        // Set the URL for the HTTP POST request
    sim800_send_command("AT+HTTPPARA=\"URL\",\"197.14.101.52:5000/temphum\"");  // Set the POST URL
    sim800_wait_response();

    	
    // Trigger the HTTP POST action
    sim800_send_command("AT+HTTPACTION=1");  // 1 indicates a POST request
    sim800_wait_response();  // Wait for response, typically HTTP status code, e.g., +HTTPACTION: 1,200,<data_length>
    
    // Read server response (you can print this for debugging)
    sim800_send_command("AT+HTTPREAD");  // Read the server's response
    sim800_wait_response();

    // Terminate the HTTP session (clean up resources)
    //sim800_send_command("AT+HTTPTERM");  // Terminate HTTP service
   //sim800_wait_response();

    // Deactivate the bearer profile (optional but recommended to release resources)
    //sim800_send_command("AT+SAPBR=0,1");  // Deactivate bearer profile
    //sim800_wait_response();

    ESP_LOGI("SIM800C", "=== HTTP POST Done ===");
    
    
	


    }else{
	ESP_LOGI(SIM_TAG, "=== Start SIM800C Reset ===");

	sim800_send_command("AT+CFUN=1,1");
    vTaskDelay(pdMS_TO_TICKS(10000));

   
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
    sim800_send_command("AT+HTTPPARA=\"URL\",\"197.14.101.52:5000/temphum\"");  // Set the POST URL
    sim800_wait_response();
    
    // Specify content type (application/json for JSON data)
    sim800_send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"");  // Set the content type for JSON
    sim800_wait_response();
    
    // Set SMS text mode
    uart_write_bytes(UART_PORT, "AT+CMGF=1\r\n", strlen("AT+CMGF=1\r\n"));
    
    sim800Reset = false;
    
    ESP_LOGI(SIM_TAG, "=== End SIM800C Reset ===");

	}
	
		//SIM800c task delay
    	vTaskDelay(pdMS_TO_TICKS(5000));  // 5s
    }
    
  }
  
void sim800_http_get_task(void) {
    ESP_LOGI("SIM800C", "=== Starting HTTP GET Task ===");

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

    // Set the URL for the HTTP GET request
    sim800_send_command("AT+HTTPPARA=\"URL\",\"http://jsonplaceholder.typicode.com/posts/1\"");  // Set the GET URL to httpbin.org
    sim800_wait_response();

    // Trigger the HTTP GET action
    sim800_send_command("AT+HTTPACTION=0");  // 0 indicates a GET request
    sim800_wait_response();  // Wait for response, typically HTTP status code, e.g., +HTTPACTION: 0,200,<data_length>

    // Terminate the HTTP session (clean up resources)
    //sim800_send_command("AT+HTTPTERM");  // Terminate HTTP service
    //sim800_wait_response();
    
    // Read server response (you can print this for debugging)
    sim800_send_command("AT+HTTPREAD");  // Read the server's response
    sim800_wait_response();

    // Deactivate the bearer profile (optional but recommended to release resources)
    //sim800_send_command("AT+SAPBR=0,1");  // Deactivate bearer profile
    //sim800_wait_response();

    ESP_LOGI("SIM800C", "=== HTTP GET Done ===");
}