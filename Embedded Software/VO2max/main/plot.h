#ifndef __PLOT_H__
#define __PLOT_H__

#include <stdint.h>

void PLOT_Initialize(void);
void PLOT_SendData(uint8_t id, double value, int64_t timestamp);

#endif