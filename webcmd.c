
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

static bool webcmd_readAndPrepareReply(webmsg* msg);
static bool webcmd_decodeAddress(char message[], zigbee_64bDestAddr* zbAddress);
static bool webcmd_decodeMacAddress(char message[], zigbee_64bDestAddr* zbAddress);

bool webcmd_init(char* fifo)
{
  bool initOk;
  initOk = true;
  webcmd_fifo = fifo;
  webcmd_fd = open(fifo, O_RDWR);
  if (webcmd_fd == -1)
  {
    syslog(LOG_EMERG, "unable to open '%s' ", fifo);
    initOk = false;
  }

  return initOk;
}

bool webcmd_checkMsg(webmsg* msg)
{
  struct timeval waitTime;
  fd_set rfs;
  int nb_fd;
  bool hasMsg;
  hasMsg = false;

  waitTime.tv_sec = 0;
  waitTime.tv_usec = 0;
  FD_ZERO(&rfs);
  FD_SET(webcmd_fd, &rfs);

  nb_fd = select(webcmd_fd + 1, &rfs, NULL, NULL, &waitTime);
  if (nb_fd > 0)
  {
    if (FD_ISSET(webcmd_fd, &rfs))
    {
      hasMsg = webcmd_readAndPrepareReply(msg);
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

  return hasMsg;
}


static bool webcmd_readAndPrepareReply(webmsg* msg)
{
  char message[256];
  ssize_t nbRead;
  bool bCorrectlyDecoded;
  bCorrectlyDecoded = false;
  nbRead = read(webcmd_fd, message, 255);
  if (nbRead > 0)
  {
    bCorrectlyDecoded = webcmd_decodeFrame(message, nbRead, msg);
    if (bCorrectlyDecoded == false)
    {
      syslog(LOG_INFO, "frame decoding error");
    }
  }
  else if (nbRead < 0)
  {
    syslog(LOG_INFO, "read error on FIFO");
  }
  else
  {
    //not possible
    ;
  }

  return bCorrectlyDecoded;
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
