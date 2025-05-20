/*
 * HX711.h
 *
 *  Created on: 16 Apr 2025
 *      Author: KURAPIKA
 */

#ifndef MAIN_HX711_H_
#define MAIN_HX711_H_

// Define HX711 pins
#include <stdint.h>
// Tag for logging

#define HX711_DT_PIN   10  // Change to your actual GPIO
#define HX711_SCK_PIN  11  // Change to your actual GPIO
void hx711_task(void *arg);



#endif /* MAIN_HX711_H_ */
