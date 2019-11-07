
#include "webcmd.h"
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/select.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "unused.h"

static char* webcmd_fifo;
static int32_t webcmd_fd = -1;
static FILE* webcmd_file = NULL;

#define     MESSAGE_NB_MAX    (50)

typedef struct
{
  webmsg message[MESSAGE_NB_MAX];
  int indexRead;
  int indexWrite;
  int nbItems;
} webmsg_msg;

static webmsg_msg web_receivedMessage;

static void webcmd_readReceivedMessage(void);
static bool webcmd_decodeAddress(char message[], zigbee_64bDestAddr* zbAddress);
static bool webcmd_decodeMacAddress(char message[], zigbee_64bDestAddr* zbAddress);
static bool webcmd_insertFrame(webmsg* msg);

bool webcmd_init(char* fifo)
{
  bool initOk;
  initOk = true;
  webcmd_fifo = fifo;
  webcmd_fd = open(fifo, O_RDWR | O_NONBLOCK);
  if (webcmd_fd == -1)
  {
    syslog(LOG_EMERG, "unable to open '%s' ", fifo);
    initOk = false;
  }
  else
  {
    webcmd_file = fdopen(webcmd_fd, "r+");
    if (webcmd_file == NULL)
    {
      syslog(LOG_EMERG, "unable to create FIFO stream");
      initOk = false;
    }
  }

  web_receivedMessage.indexRead = 0;
  web_receivedMessage.indexWrite = 0;
  web_receivedMessage.nbItems = 0;

  return initOk;
}

bool webcmd_getMessage(webmsg* msg)
{
  bool hasMessage;
  hasMessage = false;

  if (web_receivedMessage.nbItems > 0)
  {
    hasMessage = true;
    *msg = web_receivedMessage.message[web_receivedMessage.indexRead];
    web_receivedMessage.nbItems--;
    web_receivedMessage.indexRead++;
    if (web_receivedMessage.indexRead > MESSAGE_NB_MAX)
    {
      web_receivedMessage.indexRead = 0;
    }
  }

  return hasMessage;
}

bool webcmd_checkMsg(webmsg* msg)
{
  struct timeval waitTime;
  fd_set rfs;
  int nb_fd;
  bool hasMsg;
  hasMsg = false;

  hasMsg = webcmd_getMessage(msg);
  if (hasMsg == false)
  {
    waitTime.tv_sec = 0;
    waitTime.tv_usec = 0;
    FD_ZERO(&rfs);
    FD_SET(webcmd_fd, &rfs);

    nb_fd = select(webcmd_fd + 1, &rfs, NULL, NULL, &waitTime);
    if (nb_fd > 0)
    {
      if (FD_ISSET(webcmd_fd, &rfs))
      {
        webcmd_readReceivedMessage();
      }
    }
    else if (nb_fd == 0)
    {
      //no data
    }
    else
    {
      // error
      syslog(LOG_INFO, "select error on FIFO, reopen it");
      close(webcmd_fd);
      webcmd_init(webcmd_fifo);
    }
    hasMsg = webcmd_getMessage(msg);
  }

  return hasMsg;
}

#define MESSAGE_MAX_SIZE        (512)

static void webcmd_readReceivedMessage(void)
{
  char message[MESSAGE_MAX_SIZE];
  webmsg msg;
  ssize_t nbRead;
  bool bReceivedOk;
  char* msgReceived;

  nbRead = 0;
  bReceivedOk = false;

  msgReceived = fgets(message, MESSAGE_MAX_SIZE - 2, webcmd_file);
  message[MESSAGE_MAX_SIZE - 1] = '\0';

  while (msgReceived != NULL)
  {
    syslog(LOG_INFO, "received from web : %s", message);
    bReceivedOk = webcmd_decodeFrame(message, nbRead, &msg);
    if (bReceivedOk == true)
    {
      bReceivedOk = webcmd_insertFrame(&msg);
      if (!bReceivedOk)
      {
        syslog(LOG_EMERG, "unable to insert frame in FIFO");
      }
    }
    else
    {
      syslog(LOG_INFO, "frame decoding error");
    }
    msgReceived = fgets(message, MESSAGE_MAX_SIZE - 2, webcmd_file);
  }
}

static bool webcmd_insertFrame(webmsg* msg)
{
  bool bOk;
  bOk = true;

  if (web_receivedMessage.nbItems < MESSAGE_NB_MAX)
  {
    web_receivedMessage.message[web_receivedMessage.indexWrite] = *msg;
    web_receivedMessage.indexWrite++;
    web_receivedMessage.nbItems++;
    if (web_receivedMessage.indexWrite > MESSAGE_NB_MAX)
    {
      web_receivedMessage.indexWrite = 0;
    }
  }
  else
  {
    bOk = false;
  }

  return bOk;
}

bool webcmd_decodeFrame(char message[], ssize_t size, webmsg* msg)
{
  UNUSED(size);
  //format shall be the following one
  // 'device addresse;sensor_id;order'
  // 'xb@00:13:a2:00:40:d9:68:9c;3;ECO'
  bool bCorrectlyDecoded;
  char* ptr;
  char* token;
  uint32_t currentTokenIndex;

  currentTokenIndex = 0;
  bCorrectlyDecoded = false;
  token = strtok_r(message, ";", &ptr);
  while (token != NULL)
  {
    switch (currentTokenIndex)
    {
      case 0:
        bCorrectlyDecoded = webcmd_decodeAddress(token, &msg->zbAddress);
        break;

      case 1:
        {
          char* endPtr;
          msg->sensor_id = strtoul(token, &endPtr, 0);
          if (*endPtr != '\0')
          {
            bCorrectlyDecoded = false;
          }
          else
          {
            bCorrectlyDecoded = true;
          }
        }
        break;

      case 2:
        {
          uint32_t i = strcspn(token, "\n");
          token[i] = '\0';
          bCorrectlyDecoded = true;
          if (strcmp(token, "CONFORT") == 0)
          {
            msg->command = CONFORT;
          }
          else if (strcmp(token, "CONFORT_M1") == 0)
          {
            msg->command = CONFORT_M1;
          }
          else if (strcmp(token, "CONFORT_M2") == 0)
          {
            msg->command = CONFORT_M2;
          }
          else if (strcmp(token, "ECO") == 0)
          {
            msg->command = ECO;
          }
          else if (strcmp(token, "HG") == 0)
          {
            msg->command = HG;
          }
          else if (strcmp(token, "STOP") == 0)
          {
            msg->command = STOP;
          }
          else
          {
            bCorrectlyDecoded = false;
          }
        }
        break;

      default:
        bCorrectlyDecoded = false;
    }
    token = strtok_r(NULL, ";", &ptr);
    currentTokenIndex++;
  }

  return bCorrectlyDecoded;
}


static bool webcmd_decodeAddress(char message[], zigbee_64bDestAddr* zbAddress)
{
  char* pFounded;
  char* nextChar;
  bool bCorrectlyDecoded = false;

  pFounded = strchr(message, '@');
  if (pFounded != NULL)
  {
    *pFounded = '\0';

    if (strcmp(message, "xb") == 0)
    {
      nextChar = pFounded + 1;
      //now decode mac address
      bCorrectlyDecoded = webcmd_decodeMacAddress(nextChar, zbAddress);
    }
  }
  else
  {
    bCorrectlyDecoded = false;
  }

  return bCorrectlyDecoded;
}


static bool webcmd_decodeMacAddress(char message[], zigbee_64bDestAddr* zbAddress)
{
  char* pFounded;
  char* nextChar;
  bool bCorrectlyDecoded = false;
  int32_t charNumber;
  char* endptr;

  charNumber = 0;
  nextChar = message;
  pFounded = strchr(message, ':');
  while ((pFounded != NULL) && (charNumber < (ZIGBEE_MAX_MAC_ADDRESS_NUMBER - 1)))
  {
    *pFounded = '\0';
    (*zbAddress)[charNumber] = strtoul(nextChar, &endptr, 16);
    if (*endptr != '\0')
    {
      return false;
    }
    charNumber++;
    nextChar = pFounded + 1;
    pFounded = strchr(nextChar, ':');
  }

  if (charNumber == (ZIGBEE_MAX_MAC_ADDRESS_NUMBER - 1))
  {
    //decode last one
    (*zbAddress)[charNumber] = strtoul(nextChar, &endptr, 16);
    if (*endptr != '\0')
    {
      return false;
    }
  }
  else
  {
    return false;
  }

  bCorrectlyDecoded = true;
  return bCorrectlyDecoded;
}
