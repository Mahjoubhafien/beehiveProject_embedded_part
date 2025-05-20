/*
 * sim800c.h
 *
 *  Created on: 8 Apr 2025
 *      Author: KURAPIKA
 */

#ifndef MAIN_SIM800C_H_
#define MAIN_SIM800C_H_

#include <stdbool.h>
#include <stdint.h>
#define TAG "SIM800C"

// UART Configuration (Adjust pins based on your wiring)
#define UART_PORT     UART_NUM_1
#define TX_PIN             GPIO_NUM_15
#define RX_PIN             GPIO_NUM_23
#define BUF_SIZE           1024
#define AT_CMD_TIMEOUT_MS  2000
void sim800c_reset (void);
void sim800_wait_response();
void sim800c_task(void *pvParameters);
void sim800_send_command(const char *cmd);
void sim800_http_post_task(void);
void sim800_send_raw(const char *data);
void sim800_http_get_task(void);
void sim800_http_post_data_task(float temperature, float humidity, float weight, const char* location);
#endif /* MAIN_SIM800C_H_ */
