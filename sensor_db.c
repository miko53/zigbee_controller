
#include <stdlib.h>
#include <string.h>
#include "sensor_db.h"
#include "assert.h"

#define DEFAULT_ALLOCATED   (10)

typedef struct
{
  zigbee_64bDestAddr addr;
  uint16_t lastFrameID;
} sensor_db;

static sensor_db* sensor_db_pData;
static uint32_t sensor_db_count;
static uint32_t sensor_db_nbAllocated;

static sensor_db* sensor_db_search(zigbee_64bDestAddr* zbAddr);
static sensor_db* sensor_db_add(zigbee_64bDestAddr* zbAddr, uint8_t counter);

bool sensor_db_update(zigbee_64bDestAddr* zbAddr, uint8_t counter)
{
  bool isRetry;
  bool newlyAdded;
  sensor_db* pSensor;
  isRetry = false;

  if (sensor_db_pData == NULL)
  {
    sensor_db_pData = malloc(DEFAULT_ALLOCATED * sizeof(sensor_db));
    assert(sensor_db_pData != NULL);
    sensor_db_nbAllocated = DEFAULT_ALLOCATED;
    sensor_db_count = 0;
  }

  newlyAdded = false;
  pSensor = sensor_db_search(zbAddr);
  if (pSensor == NULL)
  {
    pSensor = sensor_db_add(zbAddr, counter);
    newlyAdded = true;
  }

  if ((newlyAdded == false) && (pSensor->lastFrameID == counter))
  {
    isRetry = true;
  }
  else
  {
    pSensor->lastFrameID = counter;
  }


  return isRetry;
}


static sensor_db* sensor_db_search(zigbee_64bDestAddr* zbAddr)
{
  sensor_db* found;

  found = NULL;
  for (uint32_t i = 0; i < sensor_db_count; i++)
  {
    int r = memcmp(*zbAddr, sensor_db_pData[i].addr, sizeof(zigbee_64bDestAddr));
    if (r == 0)
    {
      found = &sensor_db_pData[i];
      break;
    }
  }

  return found;
}


static sensor_db* sensor_db_add(zigbee_64bDestAddr* zbAddr, uint8_t counter)
{
  if (sensor_db_count >= sensor_db_nbAllocated)
  {
    sensor_db_nbAllocated = 2 * sensor_db_nbAllocated * sizeof(sensor_db);
    sensor_db* pNewData = realloc(sensor_db_pData, sensor_db_nbAllocated);
    sensor_db_pData = pNewData;
    assert(sensor_db_pData != NULL);
  }

  for (uint32_t i = 0; i < sizeof(*zbAddr); i++)
  {
    sensor_db_pData[sensor_db_count].addr[i] = (*zbAddr)[i];
  }
  sensor_db_pData[sensor_db_count].lastFrameID = counter;

  sensor_db* p;
  p = &sensor_db_pData[sensor_db_count];
  sensor_db_count++;
  return p;
}
