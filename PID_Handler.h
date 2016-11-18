#pragma once

#include <PID_v1.h>
#include "ConfigClass.h"

double temperature = 25, Output = 0;
uint32_t lastrun, lastSetpoint = 0, windowStartTime;

PID pid(&temperature, &Output, &configStore.solltemp, configStore.consKp, configStore.consKi, configStore.consKd, DIRECT);

inline void resetPID() {
	pid.SetMode(MANUAL);
	Output = 0;
	pid.SetMode(AUTOMATIC);
}
