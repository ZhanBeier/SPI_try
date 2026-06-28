#include "ntc_calc.h"
#include <math.h>

#define NTC_R25 10000.0f
#define NTC_B 3950.0f
#define NTC_R_FIXED 10000.0f
#define NTC_T0_K 298.15f

float ntc_calc_temperature(float v_ntc, float vcc)
{
    if (v_ntc <= 0.0f || v_ntc >= vcc)
        return -999.0f;
    float r_ntc = v_ntc * NTC_R_FIXED / (vcc - v_ntc);
    float t_k = 1.0f / (1.0f / NTC_T0_K + (1.0f / NTC_B) * logf(r_ntc / NTC_R25));
    return t_k - 273.15f;
}