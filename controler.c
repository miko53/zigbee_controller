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
#include "configfile.h"
#include "daemonize.h"
#include "unused.h"

static int32_t configure(zigbee_obj* zigbee, zigbee_panID* panID, bool bWriteData);
static void read_hardware_data(zigbee_obj* obj);
static void run(zigbee_obj* zigbee);
static void onDataCallBack(zigbee_obj* obj, zigbee_decodedFrame* pFrame);

#define ZB_BUFFER_SIZE      (100)

static uint8_t zb_buffer[ZB_BUFFER_SIZE];


int main(int argc, char* argv[])
{
  zigbee_obj zigbee;
  int32_t fd;
  uint32_t status;
  uint8_t indicationStatus;
  char* configFile;
  bool bAsDaemon;
  bool bWriteConfig;
  int32_t baudRate;

  int opt;
  bAsDaemon = false;
  configFile = NULL;
  bWriteConfig = false;
  baudRate = 115200; //by default if device already configurated set to 115200

  while ((opt = getopt(argc, argv, "whdc:b:")) != -1)
  {
    switch (opt)
    {
      case 'd':
        bAsDaemon = true;
        break;

      case 'c':
        configFile = optarg;
        break;
        
      case 'w':
        bWriteConfig = true;
        break;
        
      case 'b':
        baudRate = atoi(optarg); // Note: default speed of zigbee device is 9600
        break;

      case 'h':
      default:
        fprintf(stderr, "usage : %s -c <config file> (-d -> for daemonize) -w --> write data on zb -b <initial serial speed, default:115200> \n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (configFile == NULL)
  {
    fprintf(stderr, "no config file provided\nExiting...\n");
    exit(EXIT_FAILURE);
  }

  status = configfile_read(configFile);
  if (status != 0)
  {
    exit(EXIT_FAILURE);
  }

  if ((config_scriptName == NULL) || (config_ttydevice == NULL) || (config_panID == NULL) || (config_gpio_reset == NULL))
  {
    fprintf(stderr, "config file is not complete\n");
    exit(EXIT_FAILURE);
  }

  zigbee_panID panID;
  uint32_t i;
  for (i = 0; i < 8; i++)
  {
    panID[i] = config_panID[i];
  }

  if (bAsDaemon)
  {
    daemonize();
  }

  openlog("zb_controler", 0, LOG_USER);
  syslog(LOG_INFO, "starting...");

  //first, start by reset the zb component
  reset(config_gpio_reset);

  fd = serial_setup(config_ttydevice, baudRate, SERIAL_PARITY_OFF, SERIAL_RTSCTS_OFF, SERIAL_STOPNB_1);
  if (fd < 0)
  {
    syslog(LOG_EMERG, "not possible to configurate serial line '%s'", config_ttydevice);
    exit(EXIT_FAILURE);
  }

  zigbee_protocol_initialize(&zigbee, fd, zb_buffer, ZB_BUFFER_SIZE, onDataCallBack);
  read_hardware_data(&zigbee);

  status = configure(&zigbee, &panID, bWriteConfig);
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
    run(&zigbee);
  }
  else
  {
    syslog(LOG_EMERG, "configuration error %s", zigbee_get_indicationError(indicationStatus));
  }

  closelog();
  return EXIT_SUCCESS;
}


static void run(zigbee_obj* zigbee)
{
  uint32_t status;
  zigbee_panID currentPanID;
  status = zigbee_protocol_getPanID(zigbee, &currentPanID);
  if (status == 0)
  {
    syslog(LOG_INFO, "curren PAN ID = %d,%d,%d,%d,%d,%d,%d,%d", currentPanID[0], currentPanID[1], currentPanID[2],
           currentPanID[3], currentPanID[4], currentPanID[5], currentPanID[6], currentPanID[7]);
    //     uint16_t maxRFPayloadBytes;
    //     status = zigbee_protocol_getMaxRFPayloadBytes(&zigbee, &maxRFPayloadBytes);
    //
    //     fprintf(stdout, "maxRFPayloadBytes = %d\n", maxRFPayloadBytes);
  }

  syslog(LOG_INFO, "Ready, waiting for data reception");
  while (1)
  {
    zb_handle_status statusH;
    //     uint8_t signalStrenght;
    statusH = zigbee_handle(zigbee);
    if (statusH == ZB_RX_FRAME_RECEIVED)
    {
      //       status = zigbee_protocol_getReceivedSignalStrength(zigbee, &signalStrenght);
      //       syslog(LOG_INFO, "strenght of signal for the last frame reception: 0x%x\n", signalStrenght);
    }
  }
}

void onDataCallBack(zigbee_obj* obj, zigbee_decodedFrame* pFrame)
{
  UNUSED(obj);
  sensor_readAndProvideSensorData(pFrame, config_scriptName);
}


static int32_t configure(zigbee_obj* zigbee, zigbee_panID* panID, bool bWriteData)
{
  zigbee_config config;
  zb_status status;
  int32_t rc;

  memcpy(&config.panID, panID, sizeof(zigbee_panID));
  config.channelBitMask = ZIGGEE_DEFAULT_BITMASK;
  config.scanDuration = ZIGBEE_DEFAULT_SCAN_DURATION_EXPONENT;
  config.stackProfile = ZIGBEE_DEFAULT_STACK_PROFILE;
  config.encryption = false;
  memcpy(&config.networkKey, zigbee_randomEncryptionKey, sizeof(zigbee_encryptionKey));
  memcpy(&config.linkKey, zigbee_noLinkKey, sizeof(zigbee_linkKey));

  config.sleepPeriod = 0xA08; //25,6s
  config.nbSleepPeriod = 12; //12*25.6 = 308s -~> 5mn
  config.writeData = bWriteData;

  //   config.sleepPeriod = 0xAF0; //28s
  //   config.nbSleepPeriod = 11; //11*28 = 308s -~> 5mn

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
