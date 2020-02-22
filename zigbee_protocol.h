#ifndef ZIGBEE_PROTOCOL_H
#define ZIGBEE_PROTOCOL_H

#include <stdint.h>
#include "zigbee.h"

typedef enum
{
  ZB_CMD_SUCCESS,
  ZB_CMD_FAILED
} zb_status;

struct zigbee_obj_s
{
  uint32_t fd;
  uint16_t frameID;
  uint32_t frameSize;
  uint8_t* frame;
  uint32_t sizeOfFrameToSend;
  bool atReplyExpected;
  zigbee_decodedFrame decodedData;
  uint8_t modemStatus;
  void (*onDataFrameReception)(struct zigbee_obj_s* obj, zigbee_decodedFrame* frame);
};

typedef struct zigbee_obj_s zigbee_obj;

typedef struct
{
  zigbee_panID panID;
  uint16_t channelBitMask;
  uint8_t scanDuration;
  uint8_t stackProfile;
  bool encryption;
  zigbee_linkKey linkKey;
  zigbee_encryptionKey networkKey;
  uint16_t nbSleepPeriod;
  uint16_t sleepPeriod;
  bool writeData;
} zigbee_config;

typedef enum
{
  ZB_NO_REPLY,
  ZB_AT_REPLY_RECEIVED,
  ZB_RX_FRAME_RECEIVED,
} zb_handle_status;

extern void zigbee_protocol_initialize(zigbee_obj* obj, uint32_t fd, uint8_t* buffer, uint32_t bufferSize,
                                       void (*onDataCallBack)(struct zigbee_obj_s*, zigbee_decodedFrame*) );
extern zb_status zigbee_protocol_configure(zigbee_obj* obj, zigbee_config* config);
extern zb_status zigbee_protocol_configureIO(zigbee_obj* obj);

extern zb_status zigbee_protocol_retrieveHwVersion(zigbee_obj* obj, uint16_t* hwVersion);
extern zb_status zigbee_protocol_retrieveFwVersion(zigbee_obj* obj, uint16_t* fwVersion);
extern zb_status zigbee_protocol_retrieveSerial(zigbee_obj* obj, uint32_t* serialLow, uint32_t* serialHigh);
extern zb_status zigbee_protocol_setNodeIdentifier(zigbee_obj* obj, char* string);
extern zb_status zigbee_protocol_setSleepMode(zigbee_obj* obj, zigbee_sleepMode sleepMode);
extern zb_status zigbee_protocol_setNumberOfSleepPeriod(zigbee_obj* obj, uint16_t nbSleepPeriod);
extern zb_status zigbee_protocol_setSleepPeriod(zigbee_obj* obj, uint16_t sleepPeriod);

extern zb_status zigbee_protocol_startJoinNetwork(zigbee_obj* obj, uint8_t joinTime);
extern zb_status zigbee_protocol_waitEndOfAssociation(zigbee_obj* obj, uint8_t* indicationStatus);
extern zb_status zigbee_protocol_applyChanges(zigbee_obj* obj);
extern zb_status zigbee_protocol_write(zigbee_obj* obj);

extern zb_status zigbee_protocol_getPanID(zigbee_obj* obj, zigbee_panID* panID);
extern zb_status zigbee_protocol_getOperatingChannel(zigbee_obj* obj, uint8_t* operatingChannel);

extern zb_status zigbee_protocol_getMaxRFPayloadBytes(zigbee_obj* obj, uint16_t* maxRFPayloadBytes);
extern zb_status zigbee_protocol_getReceivedSignalStrength(zigbee_obj* obj, uint8_t* signalStrenght);
extern zb_status zigbee_protocol_sendData(zigbee_obj* obj, zigbee_64bDestAddr* destAddr64b, uint16_t destAddr16b,
    uint8_t* payload, uint8_t size);
extern zb_handle_status zigbee_handle(zigbee_obj* zigbee);

extern zb_status zigbee_protocol_nodeDiscover(zigbee_obj* obj);
extern zb_status zigbee_protocol_setBaudRate(zigbee_obj* obj, uint32_t baudRate);

#endif /* ZIGBEE_PROTOCOL_H */
