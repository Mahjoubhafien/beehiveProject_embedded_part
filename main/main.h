/*
 * main.h
 *
 *  Created on: 8 Apr 2025
 *      Author: KURAPIKA
 */

#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "sim800c.h"
#include "dht22.h"
#include "HX711.h"
#include "gps.h"


#define WIFI_SSID      "TOPNET_3D78"
#define WIFI_PASSWORD  "Hafiene2025"
#define WIFI_CONNECTED_BIT BIT0

typedef struct {
    float temperature;
    float humidity;
} DHTData_t;

typedef struct {
    float latitude;
    float longitude;
} GPSData_t;

extern QueueHandle_t dhtQueue;
extern QueueHandle_t gpsQueue;



#endif /* MAIN_MAIN_H_ */
