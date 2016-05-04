#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "serial.h"
#include "zigbee.h"
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include "zigbee_protocol.h"
#include "display.h"
#include <string.h>
#include <syslog.h>
#include "gpio.h"
#include <assert.h>
#include "sensor.h"

static int32_t configure(zigbee_obj* zigbee);
static void read_hardware_data(zigbee_obj* obj);

int main(int argc, char* argv[])
{
  zigbee_obj zigbee;
  int32_t fd;
  uint32_t status;
  uint8_t indicationStatus;
  uint8_t buffer[50];

  if (argc != 2)
  {
    fprintf(stderr, "Usage : %s tty_device\n", argv[0]);
  }

  //first, start by reset the zb component
  reset();

  fd = serial_setup(argv[1], 9600, SERIAL_PARITY_OFF, SERIAL_RTSCTS_OFF, SERIAL_STOPNB_1);
  if (fd < 0)
  {
    fprintf(stderr, "not possible to configurate serial line '%s'\n", argv[1]);
    return EXIT_FAILURE;
  }
  else
  {
    //     fprintf(stdout, "serial line ok, fd = %d\n", fd);
  }

  openlog("zb_controler", 0, LOG_USER);
  syslog(LOG_INFO, "starting...");

  zigbee_protocol_initialize(&zigbee, fd, buffer, 50);

  read_hardware_data(&zigbee);

  status = configure(&zigbee);
  if (status == 0)
  {
    status = zigbee_protocol_setNodeIdentifier(&zigbee, "ZBC1");
  }

  if (status == 0)
  {
    status = zigbee_protocol_startJoinNetwork(&zigbee, ZIGBEE_JOINING_ALWAYS_ACTIVATED);
  }

  if (status == 0)
  {
    syslog(LOG_INFO, "Network activated, wait for end of scanning");
    status = zigbee_protocol_waitEndOfAssociation(&zigbee, &indicationStatus);
    syslog(LOG_INFO, "Network association activated status = 0x%x, joining status = 0x%x", status, indicationStatus);
  }

  if ((status == 0) && (indicationStatus == 0))
  {
    zigbee_panID currentPanID;
    status = zigbee_protocol_getPanID(&zigbee, &currentPanID);
    syslog(LOG_INFO, "curren PAN ID = %d,%d,%d,%d,%d,%d,%d,%d", currentPanID[0], currentPanID[1], currentPanID[2],
           currentPanID[3], currentPanID[4], currentPanID[5], currentPanID[6], currentPanID[7]);

    //     uint16_t maxRFPayloadBytes;
    //     status = zigbee_protocol_getMaxRFPayloadBytes(&zigbee, &maxRFPayloadBytes);
    //
    //     fprintf(stdout, "maxRFPayloadBytes = %d\n", maxRFPayloadBytes);
    syslog(LOG_INFO, "Ready, waiting for data reception");
    while (1)
    {
      zb_handle_status statusH;
      uint8_t signalStrenght;
      statusH = zigbee_handle(&zigbee);
      if (statusH == ZB_RX_FRAME_RECEIVED)
      {
        sensor_readAndProvideSensorData(&zigbee.decodedData);
        status = zigbee_protocol_getReceivedSignalStrength(&zigbee, &signalStrenght);
        syslog(LOG_INFO, "strenght of signal for the last frame reception: 0x%x\n", signalStrenght);
      }
    }
  }
  else
  {
    syslog(LOG_EMERG, "configuration error %s", zigbee_get_indicationError(indicationStatus));
  }

  closelog();
  return EXIT_SUCCESS;
}


static int32_t configure(zigbee_obj* zigbee)
{
  zigbee_config config;
  zigbee_panID panID = { 'M', 'I', 'C', 'K', 0, 0, 0, 1};
  zb_status status;
  int32_t rc;

  memcpy(&config.panID, panID, sizeof(zigbee_panID));
  config.channelBitMask = ZIGGEE_DEFAULT_BITMASK;
  config.scanDuration = ZIGBEE_DEFAULT_SCAN_DURATION_EXPONENT;
  config.stackProfile = ZIGBEE_DEFAULT_STACK_PROFILE;
  config.encryption = false;
  memcpy(&config.networkKey, zigbee_randomEncryptionKey, sizeof(zigbee_encryptionKey));
  memcpy(&config.linkKey, zigbee_noLinkKey, sizeof(zigbee_linkKey));

  config.sleepPeriod = 0x708; //18s
  config.nbSleepPeriod = 5; //5*18 = 90s --> 1mn30

  status = zigbee_protocol_configure(zigbee, &config);
  if (status == ZB_CMD_SUCCESS)
  {
    syslog(LOG_INFO, "Configuration done");
  }
  else
  {
    syslog(LOG_EMERG, "Configuration failed status = 0x%x", status);
  }

  if (status == ZB_CMD_SUCCESS)
  {
    rc = 0;
  }
  else
  {
    rc = -1;
  }

  return rc;
}


static void read_hardware_data(zigbee_obj* obj)
{
  uint32_t status;
  uint16_t hwVersion;
  uint16_t fwVersion;
  uint32_t serialHigh;
  uint32_t serialLow;

  status = zigbee_protocol_retrieveHwVersion(obj, &hwVersion);
  if (status == 0)
  {
    syslog(LOG_INFO, "hardware version = %.2x", hwVersion);
  }

  status = zigbee_protocol_retrieveFwVersion(obj, &fwVersion);
  if (status == 0)
  {
    syslog(LOG_INFO, "firmware version  = %.2x", fwVersion);
  }

  status = zigbee_protocol_retrieveSerial(obj, &serialLow, &serialHigh);
  if (status == 0)
  {
    syslog(LOG_INFO, "serial number  (%.4x,%.4x)", serialHigh, serialLow);
  }
}
