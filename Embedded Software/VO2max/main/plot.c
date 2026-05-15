#include "plot.h"
#include "driver/uart.h"

#define UART_NUM_DEBUG UART_NUM_1
#define START_BYTE 0xAA

typedef union
{
    double value;
    uint8_t bytes[8];
} DoubleValue_u;

QueueHandle_t uartQueue;

void PLOT_Initialize(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_DEBUG, 1024, 1024, 10, &uartQueue, 0));
}

void PLOT_SendData(uint8_t id, double value, int64_t timestamp)
{
    uint8_t buffer[18];
    uint8_t *ts;
    DoubleValue_u d;

    d.value = value;

    buffer[0] = START_BYTE;
    buffer[1] = id;

    // float32
    buffer[2] = d.bytes[0];
    buffer[3] = d.bytes[1];
    buffer[4] = d.bytes[2];
    buffer[5] = d.bytes[3];
    buffer[6] = d.bytes[4];
    buffer[7] = d.bytes[5];
    buffer[8] = d.bytes[6];
    buffer[9] = d.bytes[7];

    // int64_t timestamp (raw bytes, little-endian ESP32)
    ts = (uint8_t *)&timestamp;

    for (int i = 0; i < 8; i++)
    {
        buffer[10 + i] = ts[i];
    }

    uart_write_bytes(UART_NUM_DEBUG, (const void *)buffer, sizeof(buffer));
}