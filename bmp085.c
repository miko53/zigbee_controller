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
// #include <i2c-dev.h>

#define I2C_SLAVE       0x0703  /* Use this slave address */

#define I2C_DEVICE  "/dev/i2c-1"
#define I2C_SLAVE_ADDR 0x77

#define OSS             (3)  // [0 to 3] 3 : max resolution

static bool bmp085_read_eeprom(uint8_t address, uint16_t* value);
static bool bmp085_read_rawTemperature(uint16_t* value);
static bool bmp085_read_pressure(uint32_t* value);
static bool bmp085_read_rawPressure(uint32_t* value);

int main(int argc, char* argv[])
{
  UNUSED(argc);
  UNUSED(argv);

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

  bmp085_read_eeprom(0xAA, (uint16_t*) &ac1);
  bmp085_read_eeprom(0xAC, (uint16_t*) &ac2);
  bmp085_read_eeprom(0xAE, (uint16_t*) &ac3);
  bmp085_read_eeprom(0xB0, &ac4);
  bmp085_read_eeprom(0xB2, &ac5);
  bmp085_read_eeprom(0xB4, &ac6);
  bmp085_read_eeprom(0xB6, (uint16_t*) &b1);
  bmp085_read_eeprom(0xB8, (uint16_t*) &b2);
  bmp085_read_eeprom(0xBA, (uint16_t*) &mb);
  bmp085_read_eeprom(0xBC, (uint16_t*) &mc);
  bmp085_read_eeprom(0xBE, (uint16_t*) &md);

  uint16_t rawTemp;
  uint32_t rawPressure;
  bmp085_read_rawTemperature(&rawTemp);
  bmp085_read_rawPressure(&rawPressure);


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

  fprintf(stdout, "temperature = %d\n", (b5 + 8) >> 4);


  int32_t b3;
  uint32_t b4;
  int32_t b6;
  uint32_t b7;
  int32_t x3;
  int32_t p;

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
    p = (b7 << 1) / b4;
  }
  else
  {
    p = (b7 / b4) << 1;
  }

  x1 = (p >> 8) * (p >> 8);
  x1 = (x1 * 3038) >> 16;
  x2 = (-7357 * p) >> 16;
  p += (x1 + x2 + 3791) >> 4;

  fprintf(stdout, "pressure = %d\n", p);

  double a = 1 - (150. / 44330.);
  double pressureAtSeaLevel = p / pow(a, 5.255);

  //average ==> 1013.25hPa
  fprintf(stdout, "notice: average pressure 1013.25hPa\n", pressureAtSeaLevel / 100);
  fprintf(stdout, "pressure seen at sea level = %f hPa\n", pressureAtSeaLevel / 100);


  return EXIT_SUCCESS;
}


static bool bmp085_read_eeprom(uint8_t address, uint16_t* value)
{
  int fd;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    fprintf(stdout, "ERROR on open device (1)\n");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    write(fd, &address, 1);
    close(fd);
  }

  //now read the data
  fd = open(I2C_DEVICE, O_RDONLY);
  if (fd == -1)
  {
    fprintf(stdout, "ERROR on open device (2)\n");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    uint8_t buffer[2];
    read(fd, buffer, 2);
    *value = ((uint16_t) buffer[0]) << 8 | buffer[1];
    fprintf(stdout, " at : 0x%x: 0x%x 0x%x (0x%x)\n", address, buffer[0], buffer[1], *value);
    close(fd);
  }

  return true;
}


static bool bmp085_read_rawTemperature(uint16_t* value)
{
  int fd;
  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    fprintf(stdout, "ERROR on open device (1)\n");
    return false;
  }
  else
  {
    uint8_t buffer[2];
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);

    buffer[0] = 0xF4;
    buffer[1] = 0x2E;

    write(fd, buffer, 2);
    close(fd);
  }

  struct timespec waitTime;
  waitTime.tv_sec = 0;
  waitTime.tv_nsec = 10000000; // according to spec 4.5ms --> take 10ms to be sure :)
  nanosleep(&waitTime, NULL);

  //now read the result
  bmp085_read_eeprom(0xF6, value);

  return true;
}


static bool bmp085_read_rawPressure(uint32_t* value)
{
  int fd;
  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    fprintf(stdout, "ERROR on open device (1)\n");
    return false;
  }
  else
  {
    uint8_t buffer[2];
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);

    buffer[0] = 0xF4;
    buffer[1] = 0x34 + (OSS << 6);

    write(fd, buffer, 2);
    close(fd);
  }

  struct timespec waitTime;
  waitTime.tv_sec = 0;
  waitTime.tv_nsec = (2 + (3 << OSS)) * 2000000; // 2 * converstion in ms
  nanosleep(&waitTime, NULL);

  //now read the result
  bmp085_read_pressure(value);

  return true;
}



static bool bmp085_read_pressure(uint32_t* value)
{
  int fd;
  uint8_t address;
  address = 0xF6;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    fprintf(stdout, "ERROR on open device (1)\n");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    write(fd, &address, 1);
    close(fd);
  }

  //now read the data
  fd = open(I2C_DEVICE, O_RDONLY);
  if (fd == -1)
  {
    fprintf(stdout, "ERROR on open device (2)\n");
    return false;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    uint8_t buffer[3];
    read(fd, buffer, 3);
    *value = ((uint32_t) buffer[0]) << 16 | ((uint32_t) buffer[1]) << 8 | buffer[0];
    *value = *value >> (8 - OSS);
    close(fd);
  }

  return true;
}
