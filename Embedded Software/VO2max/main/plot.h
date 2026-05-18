#ifndef __PLOT_H__
#define __PLOT_H__

#include <stdint.h>

/* Plot flags (enable/disable plots) */

#define PLOT_ENABLE true

#define TEMPERATURE_PLOT_ID 0
#define HUMIDITY_PLOT_ID 1
#define PRESSURE_PLOT_ID 2
#define ALTITUDE_PLOT_ID 3
#define RHO_PLOT_ID 4
#define O2_PLOT_ID 5
#define CO2_PLOT_ID 6
#define DIFFERENTIAL_PRESSURE_PLOT_ID 7
#define FLOW_PLOT_ID 8
#define EXPIRATORY_FLOW_PLOT_ID 9
#define CYCLE_EXHALED_VOLUME_PLOT_ID 10
#define TOTAL_EXHALED_VOLUME_PLOT_ID 11
#define RESPIRATORY_RATE_PLOT_ID 12
#define VO2_PLOT_ID 13
#define VO2MAX_PLOT_ID 14
#define VCO2_PLOT_ID 15
#define RESPIRATORY_QUOTIENT_PLOT_ID 16
#define POWER_PLOT_ID 17

#define PLOT_CHANNEL_0 true
#define PLOT_CHANNEL_1 true
#define PLOT_CHANNEL_2 true
#define PLOT_CHANNEL_3 true
#define PLOT_CHANNEL_4 true
#define PLOT_CHANNEL_5 true
#define PLOT_CHANNEL_6 true
#define PLOT_CHANNEL_7 true
#define PLOT_CHANNEL_8 true
#define PLOT_CHANNEL_9 true
#define PLOT_CHANNEL_10 true
#define PLOT_CHANNEL_11 true
#define PLOT_CHANNEL_12 true
#define PLOT_CHANNEL_13 true
#define PLOT_CHANNEL_14 true
#define PLOT_CHANNEL_15 true
#define PLOT_CHANNEL_16 true
#define PLOT_CHANNEL_17 true

#define CONCAT(A, B) A##B
#define CONCAT_EXPAND(A, B) CONCAT(A, B)

#define EXPAND_CHANNEL_ID_1(id) id
#define EXPAND_CHANNEL_ID(id) EXPAND_CHANNEL_ID_1(id)

#define CHANNEL_ENABLED(ch) CONCAT_EXPAND(PLOT_CHANNEL_, ch)

#define PLOT_DATA(id, value) CONCAT_EXPAND(PLOT_DATA_, CHANNEL_ENABLED(id))(id, value)

#if PLOT_ENABLE
#define PLOT_DATA_true(id, value) PLOT_SendData(id, value)
#else
#define PLOT_DATA_true(id, value) \
    while (0)                     \
    {                             \
    }
#endif
#define PLOT_DATA_false(id, value) \
    while (0)                      \
    {                              \
    }

void PLOT_Initialize(void);
void PLOT_SendData(uint8_t id, double value);

#endif