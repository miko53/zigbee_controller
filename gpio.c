#include "gpio.h"
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#ifndef GPIO_OLD_API
#include <linux/gpio.h>
#endif /* GPIO_OLD_API */

#define RESET_WAIT_TIME      (200000) //200ns is the minimal time of reset pulse for zb device
#define RESET_WAKE_TIME   (500000000) //200ns is the minimal time of reset pulse for zb device

#ifdef GPIO_OLD_API

///\a deprecated
void gpio_reset(const char* gpio_name)
{
  int32_t fd;
  struct timespec resetWaitTime;

  resetWaitTime.tv_sec = 0;
  resetWaitTime.tv_nsec = RESET_WAIT_TIME;

  fd = open(gpio_name, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "unable to reset, open '%s' failed", gpio_name);
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

#else

typedef struct
{
  int32_t fd_ctrl;
  struct gpiohandle_request output_request;
} gpio_opened;

#define MAX_FD_OPENED   (10)
static gpio_opened gpio_list_fd[10];

int32_t gpio_init(void)
{
  for (int32_t i = 0; i < MAX_FD_OPENED; i++)
  {
    gpio_list_fd[i].fd_ctrl = -1;
  }
  return 0;
}


int32_t gpio_configure_output(const char* gpio_ctrl_name, int32_t no_line, uint8_t initial_value)
{
  int my_fd = -1;
  bool bFound = false;
  struct gpiohandle_data output_values;

  for (int32_t i = 0; i < MAX_FD_OPENED; i++)
  {
    if (gpio_list_fd[i].fd_ctrl == -1)
    {
      my_fd = i;
      bFound = true;
      break;
    }
  }

  if (!bFound)
  {
    syslog(LOG_ERR, "no more gpio fd available !");
    return my_fd;
  }

  gpio_list_fd[my_fd].fd_ctrl = open(gpio_ctrl_name, O_RDONLY);
  if (gpio_list_fd[my_fd].fd_ctrl < 0)
  {
    syslog(LOG_EMERG, "unable to open '%s' failed (errno = %d)", gpio_ctrl_name, errno);
    return -1;
  }

  memset(&gpio_list_fd[my_fd].output_request, 0, sizeof(struct gpiohandle_request));
  gpio_list_fd[my_fd].output_request.lineoffsets[0] = no_line;
  gpio_list_fd[my_fd].output_request.flags = GPIOHANDLE_REQUEST_OUTPUT;
  gpio_list_fd[my_fd].output_request.lines = 1;

  int rc;
  rc = ioctl(gpio_list_fd[my_fd].fd_ctrl, GPIO_GET_LINEHANDLE_IOCTL, &gpio_list_fd[my_fd].output_request);
  if (rc < 0)
  {
    syslog(LOG_EMERG, "unable to controle gpio linehandle for %d (errno = %d)", no_line, errno);
    close(gpio_list_fd[my_fd].fd_ctrl);
    gpio_list_fd[my_fd].fd_ctrl = -1;
    return -1;
  }

  memset(&output_values, 0, sizeof(struct gpiohandle_data));
  output_values.values[0] = initial_value;

  rc = ioctl(gpio_list_fd[my_fd].output_request.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &output_values);
  if (rc < 0)
  {
    syslog(LOG_EMERG, "unable to control gpio linehandlge for %d (errno = %d)", no_line, errno);
    close(gpio_list_fd[my_fd].fd_ctrl);
    close(gpio_list_fd[my_fd].output_request.fd);
    gpio_list_fd[my_fd].fd_ctrl = -1;
    return -1;
  }

  return my_fd;
}

int32_t gpio_perform_reset(int32_t fd_output)
{
  int32_t rc = 0;
  struct gpiohandle_data output_values;
  struct timespec resetWaitTime;

  memset(&output_values, 0, sizeof(struct gpiohandle_data));
  resetWaitTime.tv_sec = 0;
  resetWaitTime.tv_nsec = RESET_WAIT_TIME;

  output_values.values[0] = 0;
  rc |= ioctl(gpio_list_fd[fd_output].output_request.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &output_values);
  nanosleep(&resetWaitTime, NULL);

  output_values.values[0] = 1;
  rc |= ioctl(gpio_list_fd[fd_output].output_request.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &output_values);
  resetWaitTime.tv_sec = 0;
  resetWaitTime.tv_nsec = RESET_WAKE_TIME;
  nanosleep(&resetWaitTime, NULL);

  return rc;
}

void gpio_close_all(void)
{
  for (int32_t i = 0; i < MAX_FD_OPENED; i++)
  {
    if (gpio_list_fd[i].fd_ctrl != -1)
    {
      close(gpio_list_fd[i].output_request.fd);
      close(gpio_list_fd[i].fd_ctrl);
      gpio_list_fd[i].fd_ctrl = -1;
    }
  }
}

#endif /* GPIO_OLD_API */
