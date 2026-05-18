#ifndef __BLUETOOTH_H__
#define __BLUETOOTH_H__

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t g_heartRateQueue;
extern QueueHandle_t g_powerQueue;

/* ------------------------------------------------------------------ */
/*  Initialisation                                                      */
/* ------------------------------------------------------------------ */

/**
 * BLUETOOTH_Initialize()
 *
 * Call once from app_main() after nvs_flash_init().
 * Starts an internal FreeRTOS task; does not block the caller.
 */
void BLUETOOTH_Initialize(void);

/* ------------------------------------------------------------------ */
/*  Status                                                              */
/* ------------------------------------------------------------------ */

/** Returns true when a downstream device (smartwatch / head unit) is connected. */
bool BLUETOOTH_IsConnected(void);

/** Returns true when the BLE HR monitor sensor is subscribed and sending data. */
bool BLUETOOTH_IsHRConnected(void);

/** Returns true when the BLE power meter sensor is subscribed and sending data. */
bool BLUETOOTH_IsPowerConnected(void);

/* ------------------------------------------------------------------ */
/*  Send computed spirometer values (thread-safe, callable from any    */
/*  FreeRTOS task).                                                     */
/* ------------------------------------------------------------------ */

void BLUETOOTH_SendVO2Max(float v);
void BLUETOOTH_SendVO2(float v);
void BLUETOOTH_SendVCO2(float v);
void BLUETOOTH_SendRQ(float v);

/* ------------------------------------------------------------------ */
/*  Application callbacks — implement these in your application file.  */
/*                                                                      */
/*  Called from the Bluedroid event-handler task each time a new       */
/*  sensor notification arrives.  Keep them short; do NOT block or     */
/*  call BLE APIs from within them.                                     */
/*  Weak no-op stubs are provided so the project links without them.   */
/* ------------------------------------------------------------------ */

/** Called with the decoded heart rate in beats-per-minute. */
void BLUETOOTH_OnHeartRate(uint16_t bpm);

/** Called with the instantaneous power in watts. */
void BLUETOOTH_OnPower(int16_t watts);

#endif