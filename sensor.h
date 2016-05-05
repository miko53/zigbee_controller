#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "zigbee.h"

extern void sensor_readAndProvideSensorData(zigbee_decodedFrame* decodedData, const char* scriptExe);

#endif /* __SENSOR_H__ */
