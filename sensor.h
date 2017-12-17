#ifndef __SENSOR_H__
#define __SENSOR_H__

#include "zigbee.h"
#include "webcmd.h"

extern void sensor_readAndProvideSensorData(zigbee_decodedFrame* decodedData, const char* scriptExe);
extern uint32_t sensor_build_command(webmsg* receivedCmd, uint8_t buffer[], uint32_t size);

#endif /* __SENSOR_H__ */
