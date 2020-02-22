#ifndef ZIGBEE_H
#define ZIGBEE_H

#include <stdint.h>
#include <stdbool.h>

#define ZIGBEE_JOINING_ALWAYS_ACTIVATED       (0xFF)
#define ZIGGEE_DEFAULT_BITMASK                (0xFFFF)
#define ZIGBEE_DEFAULT_SCAN_DURATION_EXPONENT (3)
#define ZIGBEE_DEFAULT_STACK_PROFILE          (0)
#define ZIGBEE_DEFAULT_ENCRYPTION_OPTION      (0)
#define ZIGBEE_MAX_MAC_ADDRESS_NUMBER         (8)

typedef uint8_t zigbee_panID[ZIGBEE_MAX_MAC_ADDRESS_NUMBER];
typedef uint8_t zigbee_encryptionKey[16];
typedef uint8_t zigbee_linkKey[16];

typedef uint8_t zigbee_64bDestAddr[ZIGBEE_MAX_MAC_ADDRESS_NUMBER];

typedef enum
{
  SLEEP_DISABLED = 0,
  PIN_SLEEP = 1,
  CYCLIC_SLEEP = 4,
  CYCLIC_SLEEP_PIN_WAKE = 5
} zigbee_sleepMode;

typedef enum
{
  ZB_BD_1200 = 0,
  ZB_BD_2400 = 1,
  ZB_BD_4800 = 2,
  ZB_BD_9600 = 3,
  ZB_BD_19200 = 4,
  ZB_BD_38400 = 5,
  ZB_BD_57600 = 6,
  ZB_BD_115200 = 7,
} zigbee_baudrate;


extern const zigbee_panID zigbee_randomPanID;
extern const zigbee_encryptionKey zigbee_randomEncryptionKey;
extern const zigbee_linkKey zigbee_noLinkKey;

extern uint32_t zigbee_encode_SetJoinTime(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t joinTimeInSecond);
extern uint32_t zigbee_encode_SetPanID(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_panID* panID);
extern uint32_t zigbee_encode_setScanChannelBitmask(uint8_t* buffer, uint32_t size, uint8_t frameID, uint16_t bitmask);
extern uint32_t zigbee_encode_setScanDurationExponent(uint8_t* buffer, uint32_t size, uint8_t frameID,
    uint8_t duration);
extern uint32_t zigbee_encode_setStackProfile(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t stackProfile);
extern uint32_t zigbee_encode_setEncryptionEnabled(uint8_t* buffer, uint32_t size, uint8_t frameID,
    bool encryptionEnabled);
extern uint32_t zigbee_encode_setNetworkEncryptionKey(uint8_t* buffer, uint32_t size, uint8_t frameID,
    zigbee_encryptionKey* key);
extern uint32_t zigbee_encode_setLinkKey(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_linkKey* key);
extern uint32_t zigbee_encode_setEncryptionOptions(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t options);
extern uint32_t zigbee_encode_getAssociationIndication(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getHardwareVersion(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getFirmwareVersion(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getSerialNumberHigh(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getSerialNumberLow(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_SetNodeIdentifier(uint8_t* buffer, uint32_t size, uint8_t frameID, char* string);
extern char*    zigbee_get_indicationError(uint8_t indicationStatus);
extern uint32_t zigbee_encode_setSleepMode(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_sleepMode sleepMode);
extern uint32_t zigbee_encode_applyChanges(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_write(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getPanID(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getRFPayloadBytes(uint8_t* buffer, uint32_t size, uint8_t frameID);
extern uint32_t zigbee_encode_getReceivedSignalStrenght(uint8_t* buffer, uint32_t size, uint8_t frameID);

extern uint32_t zigbee_encode_nodeDiscover(uint8_t* buffer, uint32_t size, uint8_t frameID);

extern uint32_t zigbee_encode_D5(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t state);
extern uint32_t zigbee_encode_P0(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t state);
extern uint32_t zigbee_encode_RSSI_PWM_Timer(uint8_t* buffer, uint32_t size, uint8_t frameID, uint8_t value);
extern uint32_t zigbee_encode_setBaudRate(uint8_t* buffer, uint32_t size, uint8_t frameID, zigbee_baudrate baudrate);
extern uint32_t zigbee_encode_setNumberOfSleepPeriod(uint8_t* buffer, uint32_t size, uint8_t frameID,
    uint16_t nSleepPeriod);
extern uint32_t zigbee_encode_setSleepPeriod(uint8_t* buffer, uint32_t size, uint8_t frameID, uint16_t sleepPeriod);

extern uint32_t zigbee_encode_getOperatingChannel(uint8_t* buffer, uint32_t size, uint8_t frameID);

extern uint32_t zigbee_encode_transmitRequest(uint8_t* buffer, uint32_t sizeBuffer, uint8_t frameID,
    zigbee_64bDestAddr* destAddr64b, uint16_t destAddr16b, uint8_t* payload, uint8_t size);

typedef enum
{
  ZIGBEE_API_AT_CMD           = 0x08,
  ZIGBEE_API_TRANSMIT_REQUEST = 0x10,
  ZIGBEE_AT_COMMAND_RESPONSE  = 0x88,
  ZIGBEE_MODEM_STATUS         = 0x8A,
  ZIGBEE_TRANSMIT_STATUS      = 0x8B,
  ZIGBEE_RECEIVE_PACKET       = 0x90,
} zigbee_frameType;

#define ZIGBEE_UNKNOWN_16B_ADDR    (0xFFFE)

typedef enum
{
  ZIGBEE_OK = 0,
  ZIGBEE_ERROR = 1,
  ZIGBEE_INVALID_COMMAND = 3,
  ZIGBEE_INVALID_PARAMETER = 3,
  ZIGBEE_TX_FAILURE = 4
} zigbee_commandStatus;

typedef struct
{
  uint8_t frameID;
  uint8_t ATcmd[2];
  zigbee_commandStatus status;
  uint8_t* data;
  uint32_t size;
} zigbee_atCommandResponse;

typedef struct
{
  uint8_t frameID;
  uint16_t destAddr;
  uint8_t transmitRetryCount;
  uint8_t deliveryStatus;
  uint8_t discoveryStatus;
} zigbee_transmitStatus;

typedef struct
{
  zigbee_64bDestAddr receiver64bAddr;
  uint16_t receiver16bAddr;
  uint8_t receiveOptions;
  uint8_t* payload;
  uint32_t payloadSize;
} zigbee_receivePacket;


typedef struct
{
  zigbee_frameType type;
  union
  {
    zigbee_atCommandResponse atCmd;
    uint8_t modemStatus;
    zigbee_transmitStatus transmitStatus;
    zigbee_receivePacket receivedPacket;
  };
} zigbee_decodedFrame;

extern bool zigbee_decodeHeader(uint8_t* frame, uint32_t size, uint16_t* sizeOfNextData);
extern bool zigbee_decodeFrame(uint8_t* frame, uint16_t frameSize, zigbee_decodedFrame* decodedFrame);

#endif /* ZIGBEE_H */
