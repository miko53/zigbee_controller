
#include <stdio.h>
#include "sensor.h"
#include "stdint.h"
#include "unused.h"
#include "configfile.h"
#include "sensor_db.h"
#include <assert.h>
#include "webcmd.h"
#include <string.h>

const uint8_t payload1[] = {0x0, 0x8, 0x3, 0x1, 0x3, 0x16, 0x2d, 0x2, 0x3, 0x1d, 0xf0, 0x3, 0x0, 0x1, 0xd3};
const uint8_t payload2[] = {0x0, 0x5, 0x3, 0x1, 0x3, 0x17, 0x95, 0x2, 0x3, 0x13, 0x7b, 0x3, 0x3, 0x1, 0xcf};

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

  frame.receivedPacket.payloadSize = 15;
  frame.receivedPacket.payload = (uint8_t*) payload2;
  frame.type = ZIGBEE_RECEIVE_PACKET;
  sensor_readAndProvideSensorData(&frame, "./post_data.rb");

  if (argc >= 2)
  {
    configfile_read(argv[1]);
    fprintf(stdout, "script = %s\n", config_scriptName);
    fprintf(stdout, "pandID = (%d)\n", config_nbPanID);
    for (uint32_t i = 0; i < config_nbPanID; i++)
    {
      fprintf(stdout, "%d = 0x%x\n", i, config_panID[i]);
    }
  }

  zigbee_64bDestAddr addr;
  addr[0] = 0;
  addr[1] = 1;
  addr[2] = 2;
  addr[3] = 3;
  addr[4] = 4;
  addr[5] = 5;
  addr[6] = 6;
  addr[7] = 7;

  bool isRetry;
  isRetry = sensor_db_update(&addr, 0);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == false);

  isRetry = sensor_db_update(&addr, 0);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == true);

  isRetry = sensor_db_update(&addr, 1);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == false);

  zigbee_64bDestAddr addr2;
  addr2[0] = 0;
  addr2[1] = 10;
  addr2[2] = 20;
  addr2[3] = 30;
  addr2[4] = 40;
  addr2[5] = 55;
  addr2[6] = 60;
  addr2[7] = 70;

  isRetry = sensor_db_update(&addr, 0);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == false);


  isRetry = sensor_db_update(&addr2, 0);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == false);

  isRetry = sensor_db_update(&addr, 10);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == false);

  isRetry = sensor_db_update(&addr2, 0);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == true);

  isRetry = sensor_db_update(&addr, 10);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == true);

  isRetry = sensor_db_update(&addr2, 255);
  fprintf(stdout, "isRetry = %d\n", isRetry);
  assert(isRetry == false);


  bool bCorrectlyDecoded;
  char message[255];
  webmsg msg;
  const char* pMsg = "xb@00:13:a2:00:40:d9:68:9c;3;ECO\nxb@00:13:a2:00:40:d9:68:9d;3;CONFORT\n";
  zigbee_64bDestAddr zbAddr;
  zbAddr[0] = 0;
  zbAddr[1] = 0x13;
  zbAddr[2] = 0xa2;
  zbAddr[3] = 0;
  zbAddr[4] = 0x40;
  zbAddr[5] = 0xd9;
  zbAddr[6] = 0x68;
  zbAddr[7] = 0x9c;
  strcpy(message, pMsg);

  bCorrectlyDecoded = webcmd_decodeFrame(message, sizeof(pMsg), &msg);
  assert(bCorrectlyDecoded == true);
  assert(msg.command == ECO);
  assert(msg.sensor_id = 3);
  for (uint32_t i = 0; i < ZIGBEE_MAX_MAC_ADDRESS_NUMBER; i++)
  {
    assert(msg.zbAddress[i] == zbAddr[i]);
  }
}


