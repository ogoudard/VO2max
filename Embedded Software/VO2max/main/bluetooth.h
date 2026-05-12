#pragma once

#include <stdbool.h>

/**
 * @brief Initialize and start the BLE driver.
 *
 * Spawns an internal FreeRTOS task that initializes the BT controller,
 * Bluedroid stack, registers GATT/GAP callbacks and starts advertising.
 * Call once from app_main before any BLUETOOTH_Send* call.
 */
void BLUETOOTH_Initialize(void);

/**
 * @brief Returns true if a BLE central is currently connected.
 *
 * Thread-safe (reads an atomic flag).
 */
bool BLUETOOTH_IsConnected(void);

/**
 * @brief Send individual VO2 measurements as BLE notifications.
 *
 * All four functions are thread-safe and may be called from any FreeRTOS task
 * independently and at different rates. Silently drops the update when no
 * client is connected or the driver is not yet ready.
 *
 * Values are transmitted as 4-byte IEEE-754 little-endian floats.
 */
void BLUETOOTH_SendVO2Max(float vo2max); /**< Peak VO2max      (ml/min/kg)      */
void BLUETOOTH_SendVO2(float vo2);       /**< Current VO2      (ml/min/kg)      */
void BLUETOOTH_SendVCO2(float vco2);     /**< Current VCO2     (ml/min/kg)      */
void BLUETOOTH_SendRQ(float rq);         /**< Respiratory quotient              */