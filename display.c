
#include "display.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <arpa/inet.h>

#define TRACE_ACTIVATED     0

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
  uint8_t dataType;
  uint8_t counter;
  union
  {
    sensorFrameStruct frame;
  };
} __attribute__((packed)) zb_payload_frame;


void display_decodedType(zigbee_decodedFrame* decodedData)
{
#ifdef TRACE_ACTIVATED
  assert(decodedData != NULL);

  fprintf(stdout, "decodedData.type = %.2x\n", decodedData->type);
  switch (decodedData->type)
  {
    case ZIGBEE_AT_COMMAND_RESPONSE:
      fprintf(stdout, "decodedData.atCmd.frameID = %d\n", decodedData->atCmd.frameID);
      fprintf(stdout, "decodedData.atCmd.ATcmd = %c %c\n", decodedData->atCmd.ATcmd[0], decodedData->atCmd.ATcmd[1]);
      fprintf(stdout, "decodedData.atCmd.status = 0x%x\n", decodedData->atCmd.status);
      fprintf(stdout, "decodedData.atCmd.data = %p\n", decodedData->atCmd.data);
      fprintf(stdout, "decodedData.atCmd.size = %d\n", decodedData->atCmd.size);
      for (uint32_t i = 0; i < decodedData->atCmd.size; i++)
      {
        fprintf(stdout, "%.2x ", decodedData->atCmd.data[i]);
      }
      fprintf(stdout, "\n");
      break;

    case ZIGBEE_TRANSMIT_STATUS:
      fprintf(stdout, "decodedData.transmitStatus.frameID = %d\n", decodedData->transmitStatus.frameID);
      fprintf(stdout, "decodedData.transmitStatus.destAddr = 0x%x\n", decodedData->transmitStatus.destAddr);
      fprintf(stdout, "decodedData.transmitStatus.transmitRetryCount = %d\n", decodedData->transmitStatus.transmitRetryCount);
      fprintf(stdout, "decodedData.transmitStatus.deliveryStatus = 0x%x\n", decodedData->transmitStatus.deliveryStatus);
      fprintf(stdout, "decodedData.transmitStatus.discoveryStatus = 0x%x\n", decodedData->transmitStatus.discoveryStatus);
      fprintf(stdout, "\n");
      break;

    case ZIGBEE_MODEM_STATUS:
      fprintf(stdout, "decodedData.modemStatus = %d\n", decodedData->modemStatus);
      break;

    case ZIGBEE_RECEIVE_PACKET:
      {
        uint32_t i;
        fprintf(stdout, "decodedData.receivedPacket.receiver64bAddr = ");
        for (i = 0; i < 8; i++)
        {
          fprintf(stdout, "0x%x, ", decodedData->receivedPacket.receiver64bAddr[i]);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "decodedData.receivedPacket.receiver16bAddr = 0x%x\n", decodedData->receivedPacket.receiver16bAddr);
        fprintf(stdout, "decodedData.receivedPacket.receiveOptions  = 0x%x\n", decodedData->receivedPacket.receiveOptions);
        fprintf(stdout, "decodedData.receivedPacket.payloadSize  = %d\n", decodedData->receivedPacket.payloadSize);
        fprintf(stdout, "decodedData.receivedPacket.payload  = %p\n", decodedData->receivedPacket.payload);
        if (decodedData->receivedPacket.payload != NULL)
        {
          fprintf(stdout, "payload = %s\n", decodedData->receivedPacket.payload);
          for (i = 0; i < decodedData->receivedPacket.payloadSize; i++)
          {
            fprintf(stdout, "0x%x, ", decodedData->receivedPacket.payload[i]);
          }
          fprintf(stdout, "\n");

          zb_payload_frame* payload = (zb_payload_frame*) decodedData->receivedPacket.payload;
          uint16_t temp_raw;
          uint16_t humidity_raw;
          uint16_t batt_raw;
          float temp, humidity, batt;
          temp_raw = ntohs(payload->frame.sensors[0].data);
          humidity_raw = ntohs(payload->frame.sensors[1].data);
          batt_raw = ntohs(payload->frame.sensors[2].data);

          humidity = (100.0 * humidity_raw) / 16383;
          temp = ((165.0 * temp_raw) / 16383) - 40;
          batt = (batt_raw * 3.3) / 1023;
          time_t t = time(NULL);
          struct tm tm = *localtime(&t);
          fprintf(stdout, "now: %d-%d-%d %d:%d:%d : ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                  tm.tm_sec);
          fprintf(stdout, "type = %u, counter = %u, ", payload->dataType, payload->counter);
          if (payload->frame.sensors[0].status == 0x03)
          {
            fprintf(stdout, "temperature = %f°C, ", temp);
          }
          else
          {
            fprintf(stdout, "temperature = --°C, ");
          }

          if (payload->frame.sensors[1].status == 0x03)
          {
            fprintf(stdout, "humidity = %f%%, ", humidity);
          }
          else
          {
            fprintf(stdout, "humidity = --%%, ");
          }

          if (payload->frame.sensors[2].status == 0x03)
          {
            fprintf(stdout, "batt = %fV --> %fVbatt\n", batt, (batt * (2.2 + 4.7)) / 2.2);
          }
          else
          {
            fprintf(stdout, "batt = --V\n");
          }

          //temperature = %f°C, humidity = %f%% batt (%f V)\n", payload->dataType, payload->counter, temp, humidity, batt);
        }
      }
      break;

    default:
      fprintf(stdout, "no known type\n");
      break;
  }
#endif /* TRACE_ACTIVATED */
}


void display_frame(char* displayTextHeader, uint8_t* frame, uint32_t size)
{
#ifdef TRACE_ACTIVATED
  bool bAtCommandResponse = false;
  fprintf(stdout, "(%s) frame : \n", displayTextHeader);
  for (uint32_t i = 0; i < size; i++)
  {
    if ((i == 3) && ((frame[i] == ZIGBEE_AT_COMMAND_RESPONSE) || (frame[i] == ZIGBEE_API_AT_CMD)))
    {
      bAtCommandResponse = true;
    }

    if ((bAtCommandResponse == true) && ((i == 5) || (i == 6)))
    {
      fprintf(stdout, "'%c' ", frame[i]);
    }
    else
    {
      fprintf(stdout, "%.2x ", frame[i]);
    }
  }
  fprintf(stdout, "\n");
#endif /* TRACE_ACTIVATED */
}
