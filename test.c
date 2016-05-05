
#include <stdio.h>
#include "sensor.h"
#include "stdint.h"
#include "unused.h"
#include "configfile.h"

const uint8_t payload1[] = {0x0, 0x8, 0x3, 0x1, 0x3, 0x16, 0x2d, 0x2, 0x3, 0x1d, 0xf0, 0x3, 0x3, 0x1, 0xd3};

int main (int argc, char* argv[])
{
  UNUSED(argc);
  UNUSED(argv);

  zigbee_decodedFrame frame;

  frame.receivedPacket.receiver64bAddr[0] = 0x0;
  frame.receivedPacket.receiver64bAddr[1] = 0x13;
  frame.receivedPacket.receiver64bAddr[2] = 0xa2;
  frame.receivedPacket.receiver64bAddr[3] = 0x0;
  frame.receivedPacket.receiver64bAddr[4] = 0x40;
  frame.receivedPacket.receiver64bAddr[5] = 0xbd;
  frame.receivedPacket.receiver64bAddr[6] = 0x47;
  frame.receivedPacket.receiver64bAddr[7] = 0x18;

  frame.receivedPacket.payloadSize = 15;
  frame.receivedPacket.payload = (uint8_t*) payload1;
  frame.type = ZIGBEE_RECEIVE_PACKET;
  sensor_readAndProvideSensorData(&frame, "./post_data.rb");

  if (argc >= 2)
  {
    configfile_read(argv[1]);
    fprintf(stdout, "script = %s\n", config_scriptName);
    fprintf(stdout, "pandID = (%d)\n", config_nbPanID);
    for(uint32_t i = 0; i < config_nbPanID; i++)
    {
      fprintf(stdout, "%d = 0x%x\n", i, config_panID[i]);
    }
  }
  
}


