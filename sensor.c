#include <assert.h>
#include "sensor.h"
#include "stdint.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "unused.h"
#include <syslog.h>
#include <stdlib.h>
#include "sensor_db.h"
#include "webcmd.h"

typedef struct
{
  uint8_t type;
  uint8_t status;
  uint16_t data;
} __attribute__((packed)) sensorData;

typedef struct
{
  uint8_t sensorDataNumber;
  sensorData sensors[];
} __attribute__((packed)) sensorFrameStruct;


typedef struct
{
  uint8_t v1;
  uint8_t v2;
  uint8_t v3;
} __attribute__((packed)) sensorDbgFrameStruct;

typedef struct
{
  uint8_t dataType;
  uint8_t counter;
  union
  {
    sensorFrameStruct frame;
    sensorDbgFrameStruct dbgFrame;
  };
} __attribute__((packed)) zb_payload_frame;

typedef enum
{
  SENSOR_PROTOCOL_DATA_TYPE = 0x00,
  SENSOR_PROTOCOL_DBG_TYPE  = 0x01
} zb_payload_type;

typedef enum
{
  SENSOR_HYT221_TEMP = 0x01,
  SENSOR_HYT221_HUM = 0x02,
  SENSOR_VOLTAGE = 0x03,
  ACT_HEATER = 0x81
} sensor_Type;

#define SENSOR_MAX  (100)
#define SENSOR_CMD_LINE_SIZE (1024)
#define SENSOR_TMP_SIZE (50)
#define HEX_SIZE  (3)

typedef enum
{
  DOUBLE,
  STRING
} sensor_data_type;

typedef struct
{
  uint8_t id;
  char* unit;
  sensor_data_type type;
  union
  {
    double value;
    const char* sValue;
  };
} sensorDataForScript;


static sensorDataForScript gData[SENSOR_MAX];
static uint32_t gIndex;

static void sensor_readData(zb_payload_frame* payload);
static void sensor_buildAddress(zigbee_64bDestAddr* zbAddr, char* buffer, uint32_t size);

void sensor_readAndProvideSensorData(zigbee_decodedFrame* decodedData, const char* scriptExe)
{
  assert(decodedData != NULL);
  assert(decodedData->type == ZIGBEE_RECEIVE_PACKET);
  char commandline[SENSOR_CMD_LINE_SIZE];
  char address[SENSOR_TMP_SIZE];
  char temp[SENSOR_TMP_SIZE];
  uint32_t i;
  bool isRetry;

  zb_payload_frame* payload = (zb_payload_frame*) decodedData->receivedPacket.payload;

  sensor_buildAddress(&decodedData->receivedPacket.receiver64bAddr, address, SENSOR_TMP_SIZE);
  switch (payload->dataType)
  {
    case SENSOR_PROTOCOL_DATA_TYPE:
      isRetry = sensor_db_update(&decodedData->receivedPacket.receiver64bAddr, payload->counter);
      if (isRetry == false)
      {
        sensor_readData(payload);
        snprintf(commandline, SENSOR_CMD_LINE_SIZE, "%s address=", scriptExe);
        strcat(commandline, address);
        strcat(commandline, " ");

        for (i = 0; i < gIndex; i++)
        {
          temp[0] = '\0';
          snprintf(temp, SENSOR_TMP_SIZE, "id=%d ", gData[i].id);
          strcat(commandline, temp);
          strcat(commandline, gData[i].unit);
          strcat(commandline, "=");
          switch (gData[i].type)
          {
            case DOUBLE:
              snprintf(temp, SENSOR_TMP_SIZE, "%.3f ", gData[i].value);
              break;

            case STRING:
              snprintf(temp, SENSOR_TMP_SIZE, "%s ", gData[i].sValue);
              break;

            default:
              //not possible, decoded earlier...
              assert(false);
              break;
          }
          strcat(commandline, temp);
        }
        syslog(LOG_DEBUG, "commandline: %s", commandline);
        system(commandline);
      }
      else
      {
        syslog(LOG_INFO, "retry frame for '%s', counter = %u", address, payload->counter);
      }
      break;

    case SENSOR_PROTOCOL_DBG_TYPE:
      syslog(LOG_INFO, "dbg frame for '%s', buffer = 0x%x - 0x%x - 0x%x", address, payload->dbgFrame.v1, payload->dbgFrame.v2,
             payload->dbgFrame.v3);
      break;

    default:
      break;
  }
}


static void sensor_buildAddress(zigbee_64bDestAddr* zbAddr, char* buffer, uint32_t size)
{
  UNUSED(size);
  uint32_t i;
  char temp[HEX_SIZE];
  assert(buffer != NULL);
  buffer[0] = '\0';

  strcpy(buffer, "xb@");

  for (i = 0; i < sizeof(*zbAddr); i++)
  {
    snprintf(temp, HEX_SIZE, "%.2x", (*zbAddr)[i]);
    strcat(buffer, temp);
    if (i != (sizeof(*zbAddr) - 1))
    {
      strcat(buffer, ":");
    }
  }
}

static void sensor_readData(zb_payload_frame* payload)
{
  uint8_t nbSensor;
  uint8_t id;
  uint16_t temp_raw;
  uint16_t humidity_raw;
  uint16_t batt_raw;
  double temp;
  double humidity;
  double batt;

  nbSensor = payload->frame.sensorDataNumber;
  gIndex = 0;

  for (id = 0; ((id < nbSensor) && (gIndex < SENSOR_MAX)); id++)
  {
    switch (payload->frame.sensors[id].type)
    {
      case SENSOR_HYT221_TEMP:
        if (payload->frame.sensors[id].status == 0x03)
        {
          temp_raw = ntohs(payload->frame.sensors[id].data);
          temp = ((165.0 * temp_raw) / 16383.0) - 40.0;
          gData[gIndex].id = id;
          gData[gIndex].type = DOUBLE;
          gData[gIndex].value = temp;
          gData[gIndex].unit = "temp";
          gIndex++;
        }
        break;

      case SENSOR_HYT221_HUM:
        if (payload->frame.sensors[id].status == 0x03)
        {
          humidity_raw = ntohs(payload->frame.sensors[id].data);
          humidity = (100.0 * humidity_raw) / 16383.0;
          gData[gIndex].id = id;
          gData[gIndex].type = DOUBLE;
          gData[gIndex].value = humidity;
          gData[gIndex].unit = "humd";
          gIndex++;
        }
        break;

      case SENSOR_VOLTAGE:
        if (payload->frame.sensors[id].status == 0x03)
        {
          batt_raw = ntohs(payload->frame.sensors[id].data);
          batt = (batt_raw * 3.3) / 1023.0;
          batt = (batt * (2.2 + 4.7)) / 2.2;
          gData[gIndex].id = id;
          gData[gIndex].type = DOUBLE;
          gData[gIndex].value = batt;
          gData[gIndex].unit = "volt";
          gIndex++;
        }
        break;

      case ACT_HEATER:
        if (payload->frame.sensors[id].status == 0x03)
        {
          bool bOk;
          bOk = true;
          switch (ntohs(payload->frame.sensors[id].data))
          {
            case CONFORT:
              gData[gIndex].sValue = "CONFORT";
              break;

            case ECO:
              gData[gIndex].sValue = "ECO";
              break;

            case HG:
              gData[gIndex].sValue = "HG";
              break;

            case STOP:
              gData[gIndex].sValue = "STOP";
              break;

            default:
              bOk = false;
              syslog(LOG_INFO, "unable to decode heat value %d. Skypping...", ntohs(payload->frame.sensors[id].data));
              break;
          }

          if (bOk)
          {
            gData[gIndex].id = id;
            gData[gIndex].type = STRING;
            gData[gIndex].unit = "heat";
            gIndex++;
          }
        }
        break;

      default:
        break;
    }
  }
}


