#ifndef __SENSOR_DB_H__
#define __SENSOR_DB_H__

#include "zigbee.h"
#include <stdbool.h>

extern bool sensor_db_update(zigbee_64bDestAddr* zbAddr, uint8_t counter);


#endif /* __SENSOR_DB_H__ */
