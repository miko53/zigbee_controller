
#ifndef __WEBCMD_H__
#define __WEBCMD_H__

#include <stdbool.h>
#include <unistd.h>
#include "zigbee.h"

typedef struct
{
  //for the moment only for zb device
  union
  {
    zigbee_64bDestAddr zbAddress;
  };
  uint32_t sensor_id;
  uint32_t command;
} webmsg;


typedef enum
{
  CONFORT,
  ECO,
  HG,
  STOP
} heatcmd;

extern bool webcmd_init(char* fifo);
extern bool webcmd_checkMsg(webmsg* msg);

/**
 * Function public only for unit tests
 */
extern bool webcmd_decodeFrame(char message[], ssize_t size, webmsg* msg);


#endif /* __WEBCMD_H__ */
