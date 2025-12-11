#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>

// FreeRTOS Task Functions
void gpsTask(void *parameter);
void loraTask(void *parameter);
void smsTask(void *parameter);
void bluetoothTask(void *parameter);
void displayTask(void *parameter);
void keyboardTask(void *parameter);

// Task Handles
extern TaskHandle_t gpsTaskHandle;
extern TaskHandle_t loraTaskHandle;
extern TaskHandle_t smsTaskHandle;
extern TaskHandle_t bluetoothTaskHandle;
extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t keyboardTaskHandle;

#endif
