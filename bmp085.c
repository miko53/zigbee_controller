#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "unused.h"
#include <syslog.h>
#include <getopt.h>
#include "configfile.h"
#include "daemonize.h"
#include "gpio.h"
#include <string.h>
#include <errno.h>

#define CMD_LINE_SIZE         (1024)
#define MAIN_LOOP_WAIT_TIME   (60*5) //in s
#define I2C_ADDRESS           "i2c@77"

// #include <i2c-dev.h>

#define I2C_SLAVE             (0x0703)  /* Use to set the slave address */

#define I2C_DEVICE            "/dev/i2c-1"
#define I2C_SLAVE_ADDR        (0x77)

#define OSS             (3)  // [0 to 3] 3 : max resolution

static void mainLoop(void);
static bool bmp085_read_eeprom(uint8_t address, uint16_t* value);
static bool bmp085_read_rawTemperature(uint16_t* value);
static bool bmp085_read_pressure(uint32_t* value);
static bool bmp085_read_rawPressure(uint32_t* value);
static void build_commandline(double pressureAtSeaLevel, int32_t temperature);

int main(int argc, char* argv[])
{
  int opt;
  char* configFile;
  bool bAsDaemon;
  uint32_t status;

  bAsDaemon = false;
  configFile = NULL;

  while ((opt = getopt(argc, argv, "hdc:")) != -1)
  {
    switch (opt)
    {
      case 'd':
        bAsDaemon = true;
        break;

      case 'c':
        configFile = optarg;
        break;

      case 'h':
      default:
        fprintf(stderr, "usage : %s -c <config file> (-d -> for daemonize)\n", argv[0]);
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

  if ((config_scriptName == NULL) || (config_device == NULL) || (config_gpio_reset == NULL))
  {
    fprintf(stderr, "config file is not complete\n");
    exit(EXIT_FAILURE);
  }


  if (bAsDaemon)
  {
    daemonize();
  }

  //first, start by reset the component
#ifdef GPIO_OLD_API
  gpio_reset(config_gpio_reset);
#else
  gpio_init();
  int32_t fd;
  fd = gpio_configure_output(config_gpio_ctrl_name, config_gpio_line, 0);
  gpio_perform_reset(fd);
#endif

  openlog("bmp085", 0, LOG_USER);
  syslog(LOG_INFO, "starting...");

  mainLoop();
  closelog();

#ifndef GPIO_OLD_API
  gpio_close_all();
#endif

  return EXIT_SUCCESS;
}

void mainLoop(void)
{
  int16_t ac1;
  int16_t ac2;
  int16_t ac3;
  uint16_t ac4;
  uint16_t ac5;
  uint16_t ac6;
  int16_t b1;
  int16_t b2;
  int16_t mb;
  int16_t mc;
  int16_t md;

  bool bIsOk = false;

  bIsOk = bmp085_read_eeprom(0xAA, (uint16_t*) &ac1);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xAC, (uint16_t*) &ac2);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xAE, (uint16_t*) &ac3);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xB0, &ac4);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xB2, &ac5);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xB4, &ac6);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xB6, (uint16_t*) &b1);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xB8, (uint16_t*) &b2);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xBA, (uint16_t*) &mb);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xBC, (uint16_t*) &mc);
  if (!bIsOk)
  {
    return;
  }

  bIsOk = bmp085_read_eeprom(0xBE, (uint16_t*) &md);
  if (!bIsOk)
  {
    return;
  }

  uint16_t rawTemp;
  uint32_t rawPressure;
  bool bIsOkPressure;

  while (1)
  {
    bIsOk = bmp085_read_rawTemperature(&rawTemp);
    if (!bIsOk)
    {
      syslog(LOG_INFO, "Unable to read temperature");
    }

    bIsOkPressure = bmp085_read_rawPressure(&rawPressure);
    if (!bIsOkPressure)
    {
      syslog(LOG_INFO, "Unable to read pressure");
    }


    if ((bIsOk == true) && (bIsOkPressure == true))
    {
      /*
      ac1 = 408;
      ac2 = -72;
      ac3 = -14383;
      ac4 = 32741;
      ac5 = 32757;
      ac6 = 23153;
      b1 = 6190;
      b2 = 4;
      mb = -32768;
      mc = -8711;
      md = 2868;

      rawTemp = 27898;
      rawPressure = 23843;
      should find temperature = 150 and pressure = 69964
      */

      int32_t x1, x2;
      int32_t b5;

      x1 = (((int32_t)rawTemp - (int32_t)ac6) * (int32_t)ac5) >> 15;
      x2 = ((int32_t)mc << 11) / (x1 + md);
      b5 = x1 + x2;

      //   fprintf(stdout, "x1 = %d\n", x1);
      //   fprintf(stdout, "x2 = %d\n", x2);
      //   fprintf(stdout, "b 5= %d\n", b5);

      int32_t temperature = (b5 + 8) >> 4;

      int32_t b3;
      uint32_t b4;
      int32_t b6;
      uint32_t b7;
      int32_t x3;
      int32_t pressure;

      b6 = b5 - 4000;
      x1 = (b2 * (b6 * b6) >> 12) >> 11;
      x2 = (ac2 * b6) >> 11;
      x3 = x1 + x2;
      b3 = (((((long)ac1) * 4 + x3) << OSS) + 2) >> 2;

      // Calculate B4
      x1 = (ac3 * b6) >> 13;
      x2 = (b1 * ((b6 * b6) >> 12)) >> 16;
      x3 = ((x1 + x2) + 2) >> 2;
      b4 = (ac4 * (unsigned long)(x3 + 32768)) >> 15;

      b7 = ((unsigned long)(rawPressure - b3) * (50000 >> OSS));
      if (b7 < 0x80000000)
      {
        pressure = (b7 << 1) / b4;
      }
      else
      {
        pressure = (b7 / b4) << 1;
      }

      x1 = (pressure >> 8) * (pressure >> 8);
      x1 = (x1 * 3038) >> 16;
      x2 = (-7357 * pressure) >> 16;
      pressure += (x1 + x2 + 3791) >> 4;

      //fprintf(stdout, "raw pressure = %d\n", pressure);

      double a = 1 - (config_altitude / 44330.);
      double pressureAtSeaLevel = pressure / pow(a, 5.255);

      build_commandline(pressureAtSeaLevel, temperature);
    }

    struct timespec waitTime;
    waitTime.tv_sec = MAIN_LOOP_WAIT_TIME;
    waitTime.tv_nsec = 0;
    nanosleep(&waitTime, NULL);
  }

}

void build_commandline(double pressureAtSeaLevel, int32_t temperature)
{
  char commandline[CMD_LINE_SIZE];

  //average ==> 1013.25hPa
  //   fprintf(stdout, "temperature = %d\n", temperature);
  //   fprintf(stdout, "notice: average pressure 1013.25hPa\n");
  //   fprintf(stdout, "pressure seen at sea level = %f hPa\n", pressureAtSeaLevel / 100);

  snprintf(commandline, CMD_LINE_SIZE, "%s address=%s id=1 temp=%.1f id=2 press=%.2f",
           config_scriptName,
           I2C_ADDRESS,
           (double) temperature / 10,
           pressureAtSeaLevel / 100
          );
  syslog(LOG_DEBUG, "commandline: %s", commandline);
  //   fprintf(stdout, "%s\n", commandline);
  system(commandline);
}



static bool bmp085_read_eeprom(uint8_t address, uint16_t* value)
{
  int fd;
  ssize_t r;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (1)");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    r = write(fd, &address, 1);
    if (r != 1)
    {
      syslog(LOG_ERR, "Unable to write address on the device, errno = %d", errno);
      close(fd);
      return false;
    }
    close(fd);
  }

  //now read the data
  fd = open(I2C_DEVICE, O_RDONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (2)");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    uint8_t buffer[2];
    r = read(fd, buffer, 2);
    if (r != 2)
    {
      syslog(LOG_ERR, "Unable to read value on the device, errno = %d", errno);
      close(fd);
      return false;
    }
    *value = ((uint16_t) buffer[0]) << 8 | buffer[1];
    //fprintf(stdout, " at : 0x%x: 0x%x 0x%x (0x%x)\n", address, buffer[0], buffer[1], *value);
    close(fd);
  }

  return true;
}


static bool bmp085_read_rawTemperature(uint16_t* value)
{
  int fd;
  bool r;
  ssize_t s;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (1)");
    return false;
  }
  else
  {
    uint8_t buffer[2];
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);

    buffer[0] = 0xF4;
    buffer[1] = 0x2E;

    s = write(fd, buffer, 2);
    if (s != 2)
    {
      syslog(LOG_ERR, "uname to write on device to read temperature");
      close(fd);
      return false;
    }
    close(fd);
  }

  struct timespec waitTime;
  waitTime.tv_sec = 0;
  waitTime.tv_nsec = 10000000; // according to spec 4.5ms --> take 10ms to be sure :)
  nanosleep(&waitTime, NULL);

  //now read the result
  r = bmp085_read_eeprom(0xF6, value);
  return r;
}


static bool bmp085_read_rawPressure(uint32_t* value)
{
  int fd;
  bool r;
  ssize_t s;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (1)");
    return false;
  }
  else
  {
    uint8_t buffer[2];
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);

    buffer[0] = 0xF4;
    buffer[1] = 0x34 + (OSS << 6);

    s = write(fd, buffer, 2);
    if (s != 2)
    {
      syslog(LOG_ERR, "uname to write on device to read pressure");
      close(fd);
      return false;
    }
    close(fd);
  }

  struct timespec waitTime;
  waitTime.tv_sec = 0;
  waitTime.tv_nsec = (2 + (3 << OSS)) * 2000000; // 2 * converstion in ms
  nanosleep(&waitTime, NULL);

  //now read the result
  r = bmp085_read_pressure(value);
  return r;
}



static bool bmp085_read_pressure(uint32_t* value)
{
  int fd;
  ssize_t s;
  uint8_t address;
  address = 0xF6;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (1)");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    s = write(fd, &address, 1);
    if (s != 1)
    {
      syslog(LOG_ERR, "uname to write on device to read pressure");
      close(fd);
      return false;
    }
    close(fd);
  }

  //now read the data
  fd = open(I2C_DEVICE, O_RDONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (2)");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    uint8_t buffer[3];
    s = read(fd, buffer, 3);
    if (s != 3)
    {
      syslog(LOG_ERR, "uname to read pressure");
      close(fd);
      return false;
    }
    *value = ((uint32_t) buffer[0]) << 16 | ((uint32_t) buffer[1]) << 8 | buffer[0];
    *value = *value >> (8 - OSS);
    close(fd);
  }

  return true;
}



