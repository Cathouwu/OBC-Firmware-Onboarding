#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>
#include <stdbool.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);
}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  /* Send an event to the thermal manager queue */
  if (event == NULL) {
    return ERR_CODE_INVALID_ARG;
  }
  if (thermalMgrQueueHandle == NULL) { // Check if queue has been created
    return ERR_CODE_INVALID_STATE;
  }
  if (event->type == THERMAL_MGR_EVENT_OS_HANDLER_EVENT){
    xQueueSendToFront(thermalMgrQueueHandle, event, ( TickType_t ) 0);
  }  
  else if (event->type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD){
    xQueueSend(thermalMgrQueueHandle, event, ( TickType_t ) 10 );
  }
  return ERR_CODE_SUCCESS;
}

void osHandlerLM75BD(void) {
  /* Implement this function */
  thermal_mgr_event_t osHandlerEvent;
  osHandlerEvent.type = THERMAL_MGR_EVENT_OS_HANDLER_EVENT;
  thermalMgrSendEvent(&osHandlerEvent);
}

static void thermalMgr(void *pvParameters) {
  /* Implement this task */
  thermal_mgr_event_type_t queueBuffer;
  while (1) {
    if( xQueueReceive(thermalMgrQueueHandle, &(queueBuffer), portMAX_DELAY) == pdPASS ){
      float currentTemp = 0.0;
      readTempLM75BD(LM75BD_OBC_I2C_ADDR, &currentTemp); 
      if (queueBuffer == THERMAL_MGR_EVENT_OS_HANDLER_EVENT){
        if (currentTemp >= 80.0){
        overTemperatureDetected();
      }
      else if(currentTemp <= 75.0){
        safeOperatingConditions();
      }
      }
      addTemperatureTelemetry(currentTemp);
    }
  }
}

void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}
