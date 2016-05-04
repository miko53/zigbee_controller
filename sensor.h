#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "zigbee.h"

extern void sensor_readAndProvideSensorData(zigbee_decodedFrame* decodedData);

#endif /* __SENSOR_H__ */
