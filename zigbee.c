
#include <stdint.h>
#include <assert.h>
#include "zigbee.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <syslog.h>

const zigbee_panID zigbee_randomPanID = { 0, 0, 0, 0, 0, 0, 0, 0};
const zigbee_encryptionKey zigbee_randomEncryptionKey = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
const zigbee_linkKey zigbee_noLinkKey = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

typedef enum
{
  AT_SET_PAN_ID,
  AT_GET_PAN_ID,
  AT_SCAN_CHAN,
  AT_SCAN_DURATION,
  AT_STACK_PROFILE,
  AT_ENCRYPTION_ENABLED,
  AT_SET_NETWORK_KEY,
  AT_SET_LINK_KEY,
  AT_ENCRYPTION_OPTS,
  AT_SET_JOIN_TIME,
  AT_SET_NODE_ID,
  AT_GET_ASSOCIATION_IND,
  AT_GET_HW_VERSION,
  AT_GET_FW_VERSION,
  AT_GET_SERIAL_HIGH,
  AT_GET_SERIAL_LOW,
  AT_SET_SLEEP_MODE,
  AT_APPLY_CHANGES,
  AT_GET_RF_PAYLOAD,
  AT_GET_RECV_SIGNAL_STRENGHT,
  AT_GET_NODE_DISCOVER,
  AT_IO_D5,
  AT_IO_P0_RSSI,
  AT_IO_PWM_RSSI_TIMER,
  AT_SET_BAUD_RATE,
  AT_SET_NUMBER_OF_SLEEP_PERIOD,
  AT_SET_SLEEP_PERIOD,
  AT_NB_COMMAND,
} zigbee_AT_COMMAND_LIST;

const char* zigbee_AT_COMMAND[AT_NB_COMMAND] =
{
  /* AT_SET_PAN_ID               */ "ID",
  /* AT_GET_PAN_ID               */ "OP",
  /* AT_SCAN_CHAN                */ "SC",
  /* AT_SCAN_DURATION            */ "SD",
  /* AT_STACK_PROFILE            */ "ZS",
  /* AT_ENCRYPTION_ENABLED       */ "EE",
  /* AT_SET_NETWORK_KEY          */ "NK",
  /* AT_SET_LINK_KEY             */ "KY",
  /* AT_ENCRYPTION_OPTS          */ "EO",
  /* AT_SET_JOIN_TIME            */ "NJ",
  /* AT_SET_NODE_ID              */ "NI",
  /* AT_GET_ASSOCIATION_IND      */ "AI",
  /* AT_GET_HW_VERSION           */ "HV",
  /* AT_GET_FW_VERSION           */ "VR",
  /* AT_GET_SERIAL_HIGH          */ "SH",
  /* AT_GET_SERIAL_LOW           */ "SL",
  /* AT_SET_SLEEP_MODE           */ "SM",
  /* AT_APPLY_CHANGES            */ "AC",
  /* AT_GET_RF_PAYLOAD           */ "NP",
  /* AT_GET_RECV_SIGNAL_STRENGHT */ "DB",
  /* AT_GET_NODE_DISCOVER        */ "ND",
  /* AT_IO_D5                    */ "D5",
  /* AT_IO_P0_RSSI               */ "P0",
  /* AT_IO_PWM_RSSI_TIMER        */ "RP",
  /* AT_SET_BAUD_RATE            */ "BD",
  /* AT_SET_NUMBER_OF_SLEEP_PERIOD*/ "SN",
  /* AT_SET_SLEEP_PERIOD*/           "SP"
};

static uint8_t zigbee_doChecksum(uint8_t* frame, uint32_t size);
static void zigbee_appendChecksum(uint8_t* buffer, uint32_t* frameSize);
static void zigbee_setZigBeeFrameSize(uint8_t* buffer, uint32_t sizeFrame);
static bool zigbee_decodeATResponseFrame(uint8_t* frame, uint32_t size, zigbee_decodedFrame* decodedFrame);
static bool zibgee_decodeReceivePacket(uint8_t* frame, uint32_t size, zigbee_decodedFrame* decodedFrame);

#define ZIGBEE_START_DELIMITER      (0x7E)
#define ZIGBEE_HEADER_SIZE          (3)
#define ZIGBEE_ENCAPSULATION_SIZE   (ZIGBEE_HEADER_SIZE + 1)


static uint32_t zigbee_encode_ATcmd(uint8_t* buffer, uint32_t size, uint8_t frameID, const char* atCmd, uint8_t* data,
                                    uint32_t dataSize)
{
  uint32_t i;
  uint32_t sizeFrame;
  sizeFrame  = 0;

  if (size >= (8 + dataSize))
  {
    buffer[sizeFrame++] = ZIGBEE_START_DELIMITER;
    sizeFrame += 2; //for size frame insertion
    buffer[sizeFrame++] = ZIGBEE_API_AT_CMD;
    buffer[sizeFrame++] = frameID;
    buffer[sizeFrame++] = atCmd[0];
    buffer[sizeFrame++] = atCmd[1];

    for (i = 0; i < dataSize; i++)
    {
      buffer[sizeFrame++] = data[i];
    }

    zigbee_appendChecksum(buffer, &sizeFrame);
    zigbee_setZigBeeFrameSize(buffer, sizeFrame);
  }

  return sizeFrame;
}

static void zigbee_appendChecksum(uint8_t* buffer, uint32_t* sizeFrame)
{
  buffer[*sizeFrame] = zigbee_doChecksum(&buffer[ZIGBEE_HEADER_SIZE], &buffer[*sizeFrame] - &buffer[ZIGBEE_HEADER_SIZE]);
  (*sizeFrame)++;
}


static uint8_t zigbee_doChecksum(uint8_t* frame, uint32_t size)
{
  uint8_t checksum;

  checksum = 0xFF;
  for (uint32_t i = 0; i < size; i++)
  {
    checksum -= frame[i];
  }

  return checksum;
}

static void zigbee_setZigBeeFrameSize(uint8_t* buffer, uint32_t sizeFrame)
{
  buffer[1] = ((uint16_t) (sizeFrame - ZIGBEE_ENCAPSULATION_SIZE)) >> 8;
  buffer[2] = (uint8_t) (sizeFrame - ZIGBEE_ENCAPSULATION_SIZE);
}


//AT ID
uint32_t zigbee_encode_SetPanID(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_panID* panID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_PAN_ID],
                        (uint8_t*) panID, sizeof(zigbee_panID));
}

uint32_t zigbee_encode_getPanID(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_PAN_ID],
                        NULL, 0);
}


uint32_t zigbee_encode_setNumberOfSleepPeriod(uint8_t* buffer, uint32_t size, uint8_t frameID, uint16_t nSleepPeriod)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_NUMBER_OF_SLEEP_PERIOD],
                        (uint8_t*) &nSleepPeriod, sizeof(nSleepPeriod));
}

uint32_t zigbee_encode_setSleepPeriod(uint8_t* buffer, uint32_t size, uint8_t frameID, uint16_t sleepPeriod)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_SLEEP_PERIOD],
                        (uint8_t*) &sleepPeriod, sizeof(sleepPeriod));
}


//AT SC
uint32_t zigbee_encode_setScanChannelBitmask(uint8_t* buffer, uint32_t size, uint8_t frameID, uint16_t bitmask)
{
  bitmask = htons(bitmask);
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SCAN_CHAN],
                        (uint8_t*) &bitmask, sizeof(uint16_t));
}

//AT SD
uint32_t zigbee_encode_setScanDurationExponent(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t duration)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SCAN_DURATION],
                        &duration, sizeof(uint8_t));
}


//AT ZS
uint32_t zigbee_encode_setStackProfile(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t stackProfile)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_STACK_PROFILE],
                        &stackProfile, sizeof(uint8_t));
}

//AT EE
uint32_t zigbee_encode_setEncryptionEnabled(uint8_t* buffer, uint32_t size, uint8_t frameID, bool encryptionEnabled)
{
  uint8_t encryption;
  if (encryptionEnabled == true)
  {
    encryption = 1;
  }
  else
  {
    encryption = 0;
  }

  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_ENCRYPTION_ENABLED],
                        &encryption, sizeof(uint8_t));
}

//AT NK
uint32_t zigbee_encode_setNetworkEncryptionKey(uint8_t* buffer, uint32_t size, uint8_t frameID,
    zigbee_encryptionKey* key)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_NETWORK_KEY],
                        (uint8_t*) key, sizeof(zigbee_encryptionKey));
}

//AT KY
uint32_t zigbee_encode_setLinkKey(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_linkKey* key)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_LINK_KEY],
                        (uint8_t*) key, sizeof(zigbee_linkKey));
}

//AT NJ
uint32_t zigbee_encode_setEncryptionOptions(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t options)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_ENCRYPTION_OPTS],
                        &options, sizeof(uint8_t));
}

//AT NJ
uint32_t zigbee_encode_SetJoinTime(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t joinTimeInSecond)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_JOIN_TIME],
                        &joinTimeInSecond, sizeof(uint8_t));
}

//AT NI
uint32_t zigbee_encode_SetNodeIdentifier(uint8_t* buffer, uint32_t size, uint8_t frameID, char* string)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_NODE_ID],
                        (uint8_t*) string, strlen(string));
}

//AT AI
uint32_t zigbee_encode_getAssociationIndication(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_ASSOCIATION_IND],
                        NULL, 0);
}


//AT HV
uint32_t zigbee_encode_getHardwareVersion(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_HW_VERSION],
                        NULL, 0);
}

//AT VR
uint32_t zigbee_encode_getFirmwareVersion(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_FW_VERSION],
                        NULL, 0);
}


//AT SH
uint32_t zigbee_encode_getSerialNumberHigh(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_SERIAL_HIGH],
                        NULL, 0);
}

//AT SL
uint32_t zigbee_encode_getSerialNumberLow(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_SERIAL_LOW],
                        NULL, 0);
}

//AT SM
uint32_t zigbee_encode_setSleepMode(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_sleepMode sleepMode)
{
  uint8_t slm;
  slm = (uint8_t) sleepMode;
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_SLEEP_MODE],
                        &slm, sizeof(uint8_t));
}

//AT AC
uint32_t zigbee_encode_applyChanges(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_APPLY_CHANGES],
                        NULL, 0);
}

//AT NP
uint32_t zigbee_encode_getRFPayloadBytes(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_RF_PAYLOAD],
                        NULL, 0);
}


//AT DB
uint32_t zigbee_encode_getReceivedSignalStrenght(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_RECV_SIGNAL_STRENGHT],
                        NULL, 0);
}

// AT ND Node Discover
uint32_t zigbee_encode_nodeDiscover(uint8_t* buffer, uint32_t size, uint8_t frameID)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_GET_NODE_DISCOVER],
                        NULL, 0);
}

// AT D5 IO DIO5
uint32_t zigbee_encode_D5(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t state)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_IO_D5],
                        &state, sizeof(uint8_t));
}

// AT PO IO PWM RRSI
uint32_t zigbee_encode_P0(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t state)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_IO_P0_RSSI],
                        &state, sizeof(uint8_t));
}


// ATPWM TIMER I
uint32_t zigbee_encode_RSSI_PWM_Timer(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t value)
{
  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_IO_PWM_RSSI_TIMER],
                        &value, sizeof(uint8_t));
}


// AT BD
uint32_t zigbee_encode_setBaudRate(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_baudrate baudrate)
{
  uint8_t bd;
  bd = (uint8_t) baudrate;

  return
    zigbee_encode_ATcmd(buffer, size,
                        frameID,
                        zigbee_AT_COMMAND[AT_SET_BAUD_RATE],
                        &bd, sizeof(uint8_t));
}



uint32_t zigbee_encode_transmitRequest(uint8_t* buffer, uint32_t sizeBuffer, uint8_t frameID,
                                       zigbee_64bDestAddr* destAddr64b, uint16_t destAddr16b, uint8_t* payload, uint8_t size)
{
  uint32_t sizeFrame;
  uint32_t index;
  sizeFrame  = 0;
  assert(sizeBuffer >= 8);

  buffer[sizeFrame++] = ZIGBEE_START_DELIMITER;
  sizeFrame += 2; //for size frame insertion
  buffer[sizeFrame++] = ZIGBEE_API_TRANSMIT_REQUEST;
  buffer[sizeFrame++] = frameID;

  for (index = 0; index < 8; index++)
  {
    buffer[sizeFrame++] = (*destAddr64b)[index];
  }

  buffer[sizeFrame++] = (uint8_t) (destAddr16b >> 8);
  buffer[sizeFrame++] = (uint8_t) (destAddr16b);

  //broadcoast radius
  buffer[sizeFrame++] = 0;
  //options
  buffer[sizeFrame++] = 0;

  for (index = 0; index < size; index++)
  {
    buffer[sizeFrame++] = payload[index];
  }

  zigbee_appendChecksum(buffer, &sizeFrame);
  zigbee_setZigBeeFrameSize(buffer, sizeFrame);

  return sizeFrame;
}


bool zigbee_decodeHeader(uint8_t* frame, uint32_t size, uint16_t* sizeOfNextData)
{
  bool bCorrectlyDecoded;
  bCorrectlyDecoded = false;

  if ((size == 3) && (frame[0] == ZIGBEE_START_DELIMITER))
  {
    *sizeOfNextData = (frame[1] << 8) | (frame[2]);
    bCorrectlyDecoded = true;
  }
  return bCorrectlyDecoded;
}



bool zigbee_decodeFrame(uint8_t* frame, uint16_t frameSize, zigbee_decodedFrame* decodedFrame)
{
  bool bCorrectlyDecoded;
  uint8_t checksum;
  bCorrectlyDecoded = false;

  if (frameSize >= 1)
  {
    checksum = zigbee_doChecksum(frame, frameSize - 1);
    if (checksum != frame[frameSize - 1])
    {
      syslog(LOG_ERR, "Checksum KO");
      bCorrectlyDecoded = false;
    }
    else
    {
      bCorrectlyDecoded = true;
    }
  }

  if (bCorrectlyDecoded)
  {
    decodedFrame->type = frame[0];
    switch (decodedFrame->type)
    {
      case ZIGBEE_AT_COMMAND_RESPONSE:
        decodedFrame->atCmd.frameID = frame[1];
        bCorrectlyDecoded = zigbee_decodeATResponseFrame(frame, frameSize, decodedFrame);
        break;

      case ZIGBEE_MODEM_STATUS:
        decodedFrame->modemStatus = frame[1];
        bCorrectlyDecoded = true;
        break;

      case ZIGBEE_TRANSMIT_STATUS:
        decodedFrame->transmitStatus.frameID = frame[1];
        decodedFrame->transmitStatus.destAddr = ((uint16_t) frame[2]) << 8 | frame[3];
        decodedFrame->transmitStatus.transmitRetryCount = frame[4];
        decodedFrame->transmitStatus.deliveryStatus = frame[5];
        decodedFrame->transmitStatus.discoveryStatus = frame[6];
        break;

      case ZIGBEE_RECEIVE_PACKET:
        bCorrectlyDecoded = zibgee_decodeReceivePacket(frame, frameSize, decodedFrame);
        break;

      default:
        bCorrectlyDecoded = false;
        break;
    }
  }

  return bCorrectlyDecoded;
}


static bool zigbee_decodeATResponseFrame(uint8_t* frame, uint32_t size, zigbee_decodedFrame* decodedFrame)
{
  decodedFrame->atCmd.ATcmd[0] = frame[2];
  decodedFrame->atCmd.ATcmd[1] = frame[3];
  decodedFrame->atCmd.status = frame[4];
  decodedFrame->atCmd.size = size - 6;
  if (decodedFrame->atCmd.size == 0)
  {
    decodedFrame->atCmd.data = NULL;
  }
  else
  {
    decodedFrame->atCmd.data = &frame[5];
  }

  return true;
}

static bool zibgee_decodeReceivePacket(uint8_t* frame, uint32_t size, zigbee_decodedFrame* decodedFrame)
{
  uint32_t i;
  for (i = 0; i < 8; i++)
  {
    decodedFrame->receivedPacket.receiver64bAddr[i] = frame[1 + i];
  }
  decodedFrame->receivedPacket.receiver16bAddr = ((uint16_t) frame[9]) << 8 | frame[10];
  decodedFrame->receivedPacket.receiveOptions = frame[11];
  decodedFrame->receivedPacket.payloadSize = size - 13;
  if (decodedFrame->receivedPacket.payloadSize == 0)
  {
    decodedFrame->receivedPacket.payload = NULL;
  }
  else
  {
    decodedFrame->receivedPacket.payload = &frame[12];
  }

  return true;
}

char* zigbee_get_indicationError(uint8_t indicationStatus)
{
  char* errorString;
  errorString = "uknown error";

  switch (indicationStatus)
  {
    case 0x21:
      errorString = "scan found no PANs";
      break;
    case 0x22:
      errorString = "scan found no valid PANs based on current SC and ID settings";
      break;
    case 0x23:
      errorString = "valid coordinator found, but NJ joining time expired";
      break;
    case 0x24:
      errorString = "no joinable beacons were found";
      break;
    case 0x25:
      errorString = "Unexpected state, node should not be attempting to join at this time";
      break;
    case 0x27:
      errorString = "Node Joining attempt failed (typically due to incompatible security settings)";
      break;
    case 0x2A:
      errorString = "Coordinator Start attempt failed";
      break;
    case 0x2B:
      errorString = "Checking for an existing coordinator";
      break;
    case 0x2C:
      errorString = "Attempt to leave the network failed";
      break;
    case 0xAB:
      errorString = "Attempted to join a device that did not respond.";
      break;
    case 0xAC:
      errorString = "Secure join error - network security key received unsecured";
      break;
    case 0xAD:
      errorString = "Secure join error - network security key not received";
      break;
    case 0xAF:
      errorString = "Secure join error - joining device does not have the right preconfigured link key";
      break;

    default:
      errorString = "uknown error";
      break;
  }
  return errorString;
}
