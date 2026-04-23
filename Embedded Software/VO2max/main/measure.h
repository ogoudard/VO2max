#ifndef __MEASURE_H__
#define __MEASURE_H__

void MEASURE_Initialize(void);
void MEASURE_MassFlowTask(void *pvParameters);
void MEASURE_O2Task(void *pvParameters);
void MEASURE_CO2Task(void *pvParameters);

#endif