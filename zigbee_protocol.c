#include "zigbee_protocol.h"
#include "zigbee.h"
#include "serial.h"
#include <assert.h>
#include "display.h"
#include <sys/select.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

static void zigbee_handleTx(zigbee_obj* zb);
static zb_handle_status zigbee_handleRx(zigbee_obj* zb);

static void      zigbee_protocol_incrementFrameID(zigbee_obj* obj);
static zb_status zigbee_protocol_setPanID(zigbee_obj* obj, zigbee_panID* panID);
static zb_status zigbee_protocol_setScanChannelBitmask(zigbee_obj* obj, uint16_t chanBitmask);
static zb_status zigbee_protocol_setScanDurationExponent(zigbee_obj* obj, uint8_t duration);
static zb_status zigbee_protocol_setStackProfile(zigbee_obj* obj, uint8_t stackProfile);
static zb_status zigbee_protocol_disableEncryption(zigbee_obj* obj);
static zb_status zigbee_protocol_enableEncryption(zigbee_obj* obj, zigbee_encryptionKey* encryptKey,
    zigbee_linkKey* linkKey);
static bool      zigbee_protocol_waitAndcheckReply(uint32_t fd, uint8_t* receivedFrame, uint32_t sizeBuffer,
    zigbee_decodedFrame* decodedData);
static uint32_t  zigbee_protocol_getAssociationIndication(zigbee_obj* obj, uint8_t* associationIndication);

zb_handle_status zigbee_handle(zigbee_obj* zigbee)
{
  zb_handle_status status;
  zigbee_handleTx(zigbee);
  status = zigbee_handleRx(zigbee);
  zigbee->atReplyExpected = false;
  return status;
}

void zigbee_handleTx(zigbee_obj* zb)
{
  if (zb->sizeOfFrameToSend != 0)
  {
    serial_write(zb->fd, zb->frame, zb->sizeOfFrameToSend);
    display_frame("sent", zb->frame, zb->sizeOfFrameToSend);
  }
  zb->sizeOfFrameToSend = 0;
}

zb_handle_status zigbee_handleRx(zigbee_obj* zb)
{
  zb_handle_status status;
  bool bReplyCorrectyReceived;
  bool bContinue;

  status = ZB_NO_REPLY;
  do
  {
    bContinue = false;
    bReplyCorrectyReceived = zigbee_protocol_waitAndcheckReply(zb->fd, zb->frame, zb->frameSize, &zb->decodedData);
    if (bReplyCorrectyReceived)
    {
      display_decodedType(&zb->decodedData);
      switch (zb->decodedData.type)
      {
        case ZIGBEE_AT_COMMAND_RESPONSE:
          if ((zb->atReplyExpected == true) && (zb->decodedData.atCmd.frameID == zb->frameID))
          {
            status = ZB_AT_REPLY_RECEIVED;
          }
          break;

        case ZIGBEE_MODEM_STATUS:
          zb->modemStatus = zb->decodedData.modemStatus;
          break;

        case ZIGBEE_TRANSMIT_STATUS:
          break;

        case ZIGBEE_RECEIVE_PACKET:
          status = ZB_RX_FRAME_RECEIVED;
          if (zb->onDataFrameReception != NULL)
          {
            zb->onDataFrameReception(zb, &zb->decodedData);
          }
          break;

        default:
          syslog(LOG_WARNING, "unknow frame received (type = %x)", zb->decodedData.type);
          break;
      }
      if ((zb->atReplyExpected == true) && (status != ZB_AT_REPLY_RECEIVED))
      {
        bContinue = true;
      }
    }
    else
    {
      bContinue = false;
    }
  }
  while (bContinue == true);

  return status;
}

static bool zigbee_protocol_waitAndcheckReply(uint32_t fd, uint8_t* receivedFrame, uint32_t sizeBuffer,
    zigbee_decodedFrame* decodedData)
{
  fd_set rfs;
  struct timeval waitTime;
  bool bSuccess;
  uint16_t nextSizeToRead;
  FD_ZERO(&rfs);
  FD_SET(fd, &rfs);
  waitTime.tv_sec = 2;
  waitTime.tv_usec = 0;
  bSuccess = false;
  nextSizeToRead = 0;

  if (select(fd + 1, &rfs, NULL, NULL, &waitTime) > 0)
  {
    if (FD_ISSET(fd, &rfs))
    {
      bSuccess = serial_read(fd, receivedFrame, 3);
      if (bSuccess)
      {
        bSuccess = zigbee_decodeHeader(receivedFrame, 3, &nextSizeToRead);
      }

      if (bSuccess && ((uint16_t)(3 + nextSizeToRead + 1) <= sizeBuffer))
      {
        bSuccess = serial_read(fd, &receivedFrame[3], nextSizeToRead + 1);
      }
      else
      {
        bSuccess = false;
      }

      if (bSuccess)
      {
        bSuccess = zigbee_decodeFrame(&receivedFrame[3], nextSizeToRead + 1, decodedData);
        if (bSuccess == true)
        {
          display_frame("received (ok)", receivedFrame, nextSizeToRead + 1 + 3);
        }
        else
        {
          display_frame("received (ko)", receivedFrame, nextSizeToRead + 1 + 3);
        }
      }
    }
  }
  else
  {
#ifdef TRACE_ACTIVATED
    fprintf(stdout, "Timeout\n");
#endif // TRACE_ACTIVATED
  }

#ifdef TRACE_ACTIVATED
  fprintf(stdout, "bSuccess = %d nextSizeToRead = %d\n", bSuccess, nextSizeToRead);
#endif // TRACE_ACTIVATED

  return bSuccess;
}

void zigbee_protocol_initialize(zigbee_obj* obj, uint32_t fd, uint8_t* buffer, uint32_t bufferSize,
                                void (*onDataCallBack)(struct zigbee_obj_s* obj, zigbee_decodedFrame* frame))
{
  assert(obj != NULL);
  obj->fd = fd;
  obj->frameID = 1;
  obj->frameSize = bufferSize;
  obj->frame = buffer;
  obj->sizeOfFrameToSend = 0; // 0 when no data are to be sent
  obj->modemStatus = 0;//TODO
  obj->atReplyExpected = false;
  obj->onDataFrameReception = onDataCallBack;
}

static void zigbee_protocol_incrementFrameID(zigbee_obj* obj)
{
  obj->frameID++;
  if (obj->frameID == 0)
  {
    obj->frameID++;
  }
}

zb_status zigbee_protocol_retrieveHwVersion(zigbee_obj* obj, uint16_t* hwVersion)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(hwVersion != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getHardwareVersion(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0) && (obj->decodedData.atCmd.size == 2))
  {
    *hwVersion = ((uint16_t) obj->decodedData.atCmd.data[0]) << 8 | obj->decodedData.atCmd.data[1];
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

zb_status zigbee_protocol_retrieveFwVersion(zigbee_obj* obj, uint16_t* fwVersion)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(fwVersion != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getFirmwareVersion(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0) && (obj->decodedData.atCmd.size == 2))
  {
    *fwVersion = ((uint16_t) obj->decodedData.atCmd.data[0]) << 8 | obj->decodedData.atCmd.data[1];
    //fprintf(stdout, "firwmare version = %.2x\n", firmwareVersion);
    status = ZB_CMD_SUCCESS;
  }

  return status;
}


zb_status zigbee_protocol_retrieveSerial(zigbee_obj* obj, uint32_t* serialLow, uint32_t* serialHigh)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(serialLow != NULL);
  assert(serialHigh != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getSerialNumberHigh(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0) && (obj->decodedData.atCmd.size == 4))
  {
    *serialHigh =      ((uint32_t) obj->decodedData.atCmd.data[0]) << 24 |
                       ((uint32_t) obj->decodedData.atCmd.data[1]) << 16 |
                       ((uint32_t) obj->decodedData.atCmd.data[2]) << 8  |
                       obj->decodedData.atCmd.data[3];
    //    fprintf(stdout, "serial high = %.4x\n", serialHigh);
  }

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getSerialNumberLow(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0) && (obj->decodedData.atCmd.size == 4))
  {
    *serialLow =       ((uint32_t) obj->decodedData.atCmd.data[0]) << 24 |
                       ((uint32_t) obj->decodedData.atCmd.data[1]) << 16 |
                       ((uint32_t) obj->decodedData.atCmd.data[2]) << 8  |
                       obj->decodedData.atCmd.data[3];
    //    fprintf(stdout, "serial low = %.4x\n", serialLow);
    status = ZB_CMD_SUCCESS;
  }
  return status;
}


zb_status zigbee_protocol_configure(zigbee_obj* obj, zigbee_config* config)
{
  zb_status status;
  //status = ZB_CMD_SUCCESS;
  status = zigbee_protocol_setBaudRate(obj, 115200);

  if (status == ZB_CMD_SUCCESS)
  {
    status = zigbee_protocol_setPanID(obj, &config->panID);
  }

  if (status == ZB_CMD_SUCCESS)
  {
    status = zigbee_protocol_setScanChannelBitmask(obj, config->channelBitMask);
  }

  if (status == ZB_CMD_SUCCESS)
  {
    status = zigbee_protocol_setScanDurationExponent(obj, config->scanDuration);
  }

  if (status == ZB_CMD_SUCCESS)
  {
    status = zigbee_protocol_setStackProfile(obj, config->stackProfile);
  }

  if (status == ZB_CMD_SUCCESS)
  {
    if (config->encryption == false)
    {
      status = zigbee_protocol_disableEncryption(obj);
    }
    else
    {
      status = zigbee_protocol_enableEncryption(obj, &config->networkKey, &config->linkKey);
    }
  }

  if ((status == ZB_CMD_SUCCESS) && (config->sleepPeriod != 0))
  {
    status = zigbee_protocol_setSleepPeriod(obj, config->sleepPeriod);
  }


  if ((status == ZB_CMD_SUCCESS) && (config->nbSleepPeriod != 0))
  {
    status = zigbee_protocol_setNumberOfSleepPeriod(obj, config->nbSleepPeriod);
  }

  /*
    if (status == ZB_CMD_SUCCESS)
    {
      status = zigbee_protocol_applyChanges(obj);
    }
  */

  if ((status == ZB_CMD_SUCCESS) && (config->writeData == true))
  {
    status = zigbee_protocol_write(obj);
    syslog(LOG_INFO, "write data into ZB (status = %d)", status);
  }

  return status;
}

static zb_status zigbee_protocol_setPWM0RRSI(zigbee_obj* obj, uint8_t value);
static zb_status zigbee_protocol_setPWM0RRSI_Timer(zigbee_obj* obj, uint8_t state);
static zb_status zigbee_protocol_setD5State(zigbee_obj* obj, uint8_t state);

zb_status zigbee_protocol_configureIO(zigbee_obj* obj)
{
  zb_status status;

  status = zigbee_protocol_setD5State(obj, 0);
  if (status == ZB_CMD_SUCCESS)
  {
    status = zigbee_protocol_setPWM0RRSI(obj, 1);
  }
  if (status == ZB_CMD_SUCCESS)
  {
    status = zigbee_protocol_setPWM0RRSI_Timer(obj, 5);
  }
  return status;
}

zb_status zigbee_protocol_setBaudRate(zigbee_obj* obj, uint32_t baudRate)
{
  zb_handle_status handle_status;
  zb_status status;
  zigbee_baudrate zbBaudRate;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);

  switch (baudRate)
  {
    case 1200:
      zbBaudRate = ZB_BD_1200;
      break;

    case 2400:
      zbBaudRate = ZB_BD_2400;
      break;

    case 4800:
      zbBaudRate = ZB_BD_4800;
      break;

    case 9600:
    default:
      zbBaudRate = ZB_BD_9600;
      break;

    case 19200:
      zbBaudRate = ZB_BD_19200;
      break;

    case 38400:
      zbBaudRate = ZB_BD_38400;
      break;

    case 57600:
      zbBaudRate = ZB_BD_57600;
      break;

    case 115200:
      zbBaudRate = ZB_BD_115200;
      break;
  }

  obj->sizeOfFrameToSend = zigbee_encode_setBaudRate(obj->frame, 50, obj->frameID, zbBaudRate);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  serial_set_baudrate(obj->fd, baudRate);

  return status;
}

static zb_status zigbee_protocol_setD5State(zigbee_obj* obj, uint8_t state)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_D5(obj->frame, 50, obj->frameID, state);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

static zb_status zigbee_protocol_setPWM0RRSI(zigbee_obj* obj, uint8_t state)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_P0(obj->frame, 50, obj->frameID, state);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

static zb_status zigbee_protocol_setPWM0RRSI_Timer(zigbee_obj* obj, uint8_t value)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_RSSI_PWM_Timer(obj->frame, 50, obj->frameID, value);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

zb_status zigbee_protocol_nodeDiscover(zigbee_obj* obj)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_nodeDiscover(obj->frame, 50, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

static zb_status zigbee_protocol_setPanID(zigbee_obj* obj, zigbee_panID* panID)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(panID != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_SetPanID(obj->frame, 50, obj->frameID, panID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

//TODO return the pan ID
zb_status zigbee_protocol_getPanID(zigbee_obj* obj, zigbee_panID* panID)
{
  zb_handle_status handle_status;
  zb_status status;
  uint32_t index;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getPanID(obj->frame, 50, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
    for (index = 0; index < 8; index++)
    {
      (*panID)[index] = obj->decodedData.atCmd.data[index];
    }
  }

  return status;
}

zb_status zigbee_protocol_getMaxRFPayloadBytes(zigbee_obj* obj, uint16_t* maxRFPayloadBytes)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getRFPayloadBytes(obj->frame, 50, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
    *maxRFPayloadBytes = ((uint16_t) obj->decodedData.atCmd.data[0]) << 8  |
                         obj->decodedData.atCmd.data[1];
  }

  return status;
}

zb_status zigbee_protocol_getReceivedSignalStrength(zigbee_obj* obj, uint8_t* signalStrenght)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getReceivedSignalStrenght(obj->frame, 50, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
    *signalStrenght = obj->decodedData.atCmd.data[0];
  }

  return status;
}



static zb_status zigbee_protocol_setScanChannelBitmask(zigbee_obj* obj, uint16_t chanBitmask)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setScanChannelBitmask(obj->frame, obj->frameSize, obj->frameID, chanBitmask);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

static zb_status zigbee_protocol_setScanDurationExponent(zigbee_obj* obj, uint8_t duration)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setScanDurationExponent(obj->frame, obj->frameSize, obj->frameID, duration);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

static zb_status zigbee_protocol_setStackProfile(zigbee_obj* obj, uint8_t stackProfile)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setStackProfile(obj->frame, obj->frameSize, obj->frameID, stackProfile);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}


static zb_status zigbee_protocol_disableEncryption(zigbee_obj* obj)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setEncryptionEnabled(obj->frame, obj->frameSize, obj->frameID, false);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}

static zb_status zigbee_protocol_enableEncryption(zigbee_obj* obj, zigbee_encryptionKey* encryptKey,
    zigbee_linkKey* linkKey)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setEncryptionEnabled(obj->frame, obj->frameSize, obj->frameID, true);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  if (status != ZB_CMD_SUCCESS)
  {
    return status;
  }

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setNetworkEncryptionKey(obj->frame, obj->frameSize, obj->frameID, encryptKey);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  if (status != ZB_CMD_SUCCESS)
  {
    return status;
  }

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setLinkKey(obj->frame, obj->frameSize, obj->frameID, linkKey);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }

  return status;
}

zb_status zigbee_protocol_setSleepMode(zigbee_obj* obj, zigbee_sleepMode sleepMode)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setSleepMode(obj->frame, obj->frameSize, obj->frameID, sleepMode);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}


zb_status zigbee_protocol_setNumberOfSleepPeriod(zigbee_obj* obj, uint16_t nbSleepPeriod)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setNumberOfSleepPeriod(obj->frame, obj->frameSize, obj->frameID, nbSleepPeriod);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}

zb_status zigbee_protocol_setSleepPeriod(zigbee_obj* obj, uint16_t sleepPeriod)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_setSleepPeriod(obj->frame, obj->frameSize, obj->frameID, sleepPeriod);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}

zb_status zigbee_protocol_applyChanges(zigbee_obj* obj)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_applyChanges(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}

zb_status zigbee_protocol_write(zigbee_obj* obj)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_write(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}


zb_status zigbee_protocol_startJoinNetwork(zigbee_obj* obj, uint8_t joinTime)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_SetJoinTime(obj->frame, obj->frameSize, obj->frameID, joinTime);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}


zb_status zigbee_protocol_waitEndOfAssociation(zigbee_obj* obj, uint8_t* indicationStatus)
{
  zb_status status;

  assert(obj != NULL);
  assert(indicationStatus != NULL);
  status = ZB_CMD_FAILED;

  *indicationStatus = -1;
  do
  {
    status = zigbee_protocol_getAssociationIndication(obj, indicationStatus);
    if ((status == ZB_CMD_SUCCESS) && (*indicationStatus == 0xFF))
    {
      sleep(1);
    }

  }
  while ((status == 0) && (*indicationStatus == 0xFF));

  return status;
}


static uint32_t zigbee_protocol_getAssociationIndication(zigbee_obj* obj, uint8_t* associationIndication)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(associationIndication != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_getAssociationIndication(obj->frame, obj->frameSize, obj->frameID);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
    *associationIndication = obj->decodedData.atCmd.data[0];
  }

  return status;
}


zb_status zigbee_protocol_setNodeIdentifier(zigbee_obj* obj, char* string)
{
  zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(string != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_SetNodeIdentifier(obj->frame, 50, obj->frameID, string);
  obj->atReplyExpected = true;

  handle_status = zigbee_handle(obj);
  if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
      (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}

zb_status zigbee_protocol_sendData(zigbee_obj* obj, zigbee_64bDestAddr* destAddr64b, uint16_t destAddr16b,
                                   uint8_t* payload, uint8_t size)
{
  //   zb_handle_status handle_status;
  zb_status status;

  assert(obj != NULL);
  assert(destAddr64b != NULL);
  assert(payload != NULL);
  status = ZB_CMD_FAILED;

  zigbee_protocol_incrementFrameID(obj);
  obj->sizeOfFrameToSend = zigbee_encode_transmitRequest(obj->frame, obj->frameSize, obj->frameID, destAddr64b,
                           destAddr16b, payload, size);

  /*handle_status =*/ zigbee_handle(obj);
  // if ((handle_status == ZB_AT_REPLY_RECEIVED) &&
  //     (obj->decodedData.atCmd.status == 0))
  {
    status = ZB_CMD_SUCCESS;
  }
  return status;
}


