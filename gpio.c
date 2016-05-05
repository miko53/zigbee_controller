
#include "gpio.h"
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#define RESET_WAIT_TIME      (200000) //200ns is the minimal time of reset pulse for zb device
#define RESET_WAKE_TIME   (500000000) //200ns is the minimal time of reset pulse for zb device

void reset(const char* gpio_name)
{
  int32_t fd;
  struct timespec resetWaitTime;

  resetWaitTime.tv_sec = 0;
  resetWaitTime.tv_nsec = RESET_WAIT_TIME;

  fd = open(gpio_name, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "unable to reset zigbee controler, open '%s' failed", gpio_name);
  }
  else
  {
    write(fd, "0", 2);
    nanosleep(&resetWaitTime, NULL);
    write(fd, "1", 2);
    resetWaitTime.tv_sec = 0;
    resetWaitTime.tv_nsec = RESET_WAKE_TIME;
    nanosleep(&resetWaitTime, NULL);
    close(fd);
  }
}
