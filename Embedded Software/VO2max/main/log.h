#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>

void LOG_Initialize(void);
void LOG_SendData(uint8_t id, float value, int64_t timestamp);

#endif