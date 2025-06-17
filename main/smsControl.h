/*
 * smsControl.h
 *
 *  Created on: 16 Jun 2025
 *      Author: KURAPIKA
 */

#ifndef MAIN_SMSCONTROL_H_
#define MAIN_SMSCONTROL_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void sms_control_task(void *pvParameters);   // Declare the task function
void sim800_send_sms(const char *number, const char *message);  // Optional if used outside


#endif /* MAIN_SMSCONTROL_H_ */
