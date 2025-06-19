/*
 * getWebServerData.h
 *
 *  Created on: 19 Jun 2025
 *      Author: KURAPIKA
 */

#ifndef MAIN_GETWEBSERVERDATA_H_
#define MAIN_GETWEBSERVERDATA_H_
#include <stddef.h>

void getWebServerData_task(void *pvParameters);

void sim800_wait_response_get(char *out_buffer, size_t max_len);

#endif /* MAIN_GETWEBSERVERDATA_H_ */
