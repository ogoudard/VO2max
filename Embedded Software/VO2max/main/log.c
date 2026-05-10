#include "log.h"
#include "driver/uart.h"

#define UART_NUM_DEBUG UART_NUM_1
#define START_BYTE 0xAA

typedef union
{
    float value;
    uint8_t bytes[4];
} FloatValue_u;

QueueHandle_t uartQueue;

void LOG_Initialize(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_DEBUG, 1024, 1024, 10, &uartQueue, 0));
}

void LOG_SendData(uint8_t id, float value, int64_t timestamp)
{
    uint8_t buffer[14];

    FloatValue_u f;
    f.value = value;

    buffer[0] = START_BYTE;
    buffer[1] = id;

    // float32
    buffer[2] = f.bytes[0];
    buffer[3] = f.bytes[1];
    buffer[4] = f.bytes[2];
    buffer[5] = f.bytes[3];

    // int64_t timestamp (raw bytes, little-endian ESP32)
    uint8_t *ts = (uint8_t *)&timestamp;

    for (int i = 0; i < 8; i++)
    {
        buffer[6 + i] = ts[i];
    }

    uart_write_bytes(UART_NUM_DEBUG, (const void *)buffer, 14);
}