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

#define DEBUG_ON              (1)
#define CMD_LINE_SIZE         (1024)
#define MAIN_LOOP_WAIT_TIME   (60*5) //in s
#define I2C_ADDRESS           "i2c@76"
#define BME280_ID             0x60

#define I2C_SLAVE             (0x0703)  /* Use to set the slave address */

#define I2C_DEVICE            "/dev/i2c-1"
#define I2C_SLAVE_ADDR        (0x76)

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

  /* if ((config_scriptName == NULL) || (config_device == NULL))
   {
     fprintf(stderr, "config file is not complete\n");
     exit(EXIT_FAILURE);
   }*/


  if (bAsDaemon)
  {
    daemonize();
  }

  openlog("bme280", 0, LOG_USER);
  syslog(LOG_INFO, "starting...");

  mainLoop();

  closelog();
  return EXIT_SUCCESS;
}

ssize_t i2c_read_register(uint8_t address, uint8_t* data, uint8_t length)
{
  int fd;
  ssize_t r = -1;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (1)");
    return -1;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    r = write(fd, &address, 1);
    if (r != 1)
    {
      syslog(LOG_ERR, "Unable to write address on the device, errno = %d", errno);
      close(fd);
      return -1;
    }
    close(fd);
  }

  //now read the data
  fd = open(I2C_DEVICE, O_RDONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (2)");
    return -1;
  }
  else
  {
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);
    r = read(fd, data, length);
    if (r != length)
    {
      syslog(LOG_ERR, "Unable to read value on the device, r = %d, errno = %d", r, errno);
    }

    if (DEBUG_ON)
    {
      fprintf(stdout, "(R) 0x%.2x: ", address);
      for (uint32_t i = 0; i < length; i++)
      {
        fprintf(stdout, "0x%.2x ", data[i]);
      }
      fprintf(stdout, "\n");
    }

    close(fd);
    return r;
  }

  return r;
}

ssize_t i2c_write_register(uint8_t address, uint8_t data)
{
  int fd;
  ssize_t s;

  fd = open(I2C_DEVICE, O_WRONLY);
  if (fd == -1)
  {
    syslog(LOG_EMERG, "ERROR on open device (1)");
    return -1;
  }
  else
  {
    uint8_t buffer[2];
    ioctl(fd, I2C_SLAVE, I2C_SLAVE_ADDR);

    buffer[0] = address;
    buffer[1] = data;

    s = write(fd, buffer, 2);
    if (s != 2)
    {
      syslog(LOG_ERR, "uname to write on device");
      close(fd);
      return -1;
    }

    if (DEBUG_ON)
    {
      fprintf(stdout, "(W) 0x%.2x: 0x%.2x\n", address, data);
    }
    close(fd);
  }

  return 1;
}

typedef struct
{
  uint16_t dig_T1;
  int16_t dig_T2;
  int16_t dig_T3;
  uint16_t dig_P1;
  int16_t dig_P2;
  int16_t dig_P3;
  int16_t dig_P4;
  int16_t dig_P5;
  int16_t dig_P6;
  int16_t dig_P7;
  int16_t dig_P8;
  int16_t dig_P9;
  uint8_t dig_H1;
  int16_t dig_H2;
  uint8_t dig_H3;
  int16_t dig_H4;
  int16_t dig_H5;
  int8_t dig_H6;
} trimmingParameter;

trimmingParameter trimParam;

double bme280_getTemperature(trimmingParameter* trimParam, uint32_t temperatureRaw, int32_t* pTFine);
double bme280_getPressure(trimmingParameter* trimParam, int32_t t_fine, uint32_t pressureRaw);
double bme280_getHumidity(trimmingParameter* trimParam, int32_t t_fine, uint32_t humidityRaw);

void mainLoop(void)
{
  uint8_t data[24];
  uint32_t r;

  //reset
  i2c_write_register(0xE0, 0xB6);

  r = i2c_read_register(0xD0, data, 1);
  if ((r != 1) && (data[0] != BME280_ID))
  {
    syslog(LOG_EMERG, "ERROR wrong device ID (0x%.2x != 0x%.2x", BME280_ID, data[0]);
    return;
  }

  //read trimming parameter
  r = i2c_read_register(0x88, data, 24);
  if (r != 24)
  {
    syslog(LOG_EMERG, "ERROR unable to read trimming register");
    return;
  }

  trimParam.dig_T1 = ((uint16_t) data[1]) << 8 | data[0];
  trimParam.dig_T2 = ((uint16_t) data[3]) << 8 | data[2];
  trimParam.dig_T3 = ((uint16_t) data[5]) << 8 | data[4];

  trimParam.dig_P1 = ((uint16_t) data[7]) << 8 | data[6];
  trimParam.dig_P2 = ((uint16_t) data[9]) << 8 | data[8];
  trimParam.dig_P3 = ((uint16_t) data[11]) << 8 | data[10];
  trimParam.dig_P4 = ((uint16_t) data[13]) << 8 | data[12];
  trimParam.dig_P5 = ((uint16_t) data[15]) << 8 | data[14];
  trimParam.dig_P6 = ((uint16_t) data[17]) << 8 | data[16];
  trimParam.dig_P7 = ((uint16_t) data[19]) << 8 | data[18];
  trimParam.dig_P8 = ((uint16_t) data[21]) << 8 | data[20];
  trimParam.dig_P9 = ((uint16_t) data[23]) << 8 | data[22];

  r = i2c_read_register(0xA1, data, 1);
  if (r != 1)
  {
    syslog(LOG_EMERG, "ERROR unable to read trimming register");
    return;
  }

  trimParam.dig_H1 = data[0];

  r = i2c_read_register(0xE1, data, 7);
  if (r != 7)
  {
    syslog(LOG_EMERG, "ERROR unable to read trimming register");
    return;
  }

  trimParam.dig_H2 = ((uint16_t) data[1]) << 8 | data[0]; //E2 .. E1
  trimParam.dig_H3 = data[2]; // E3
  trimParam.dig_H4 = (data[3] << 4) | (data[4] & 0x0F); // E4 .. E5
  trimParam.dig_H5 = (data[5] << 4) | ((data[4] & 0xF0) >> 4); // E5 .. E6
  trimParam.dig_H6 = data[6];

  if (DEBUG_ON)
  {
    fprintf(stdout, "trimParam.dig_T1 = %d (0x%.4x)\n", trimParam.dig_T1, trimParam.dig_T1);
    fprintf(stdout, "trimParam.dig_T2 = %d (0x%.4x)\n", trimParam.dig_T2, trimParam.dig_T2);
    fprintf(stdout, "trimParam.dig_T3 = %d (0x%.4x)\n", trimParam.dig_T3, trimParam.dig_T3);

    fprintf(stdout, "trimParam.dig_P1 = %d (0x%.4x)\n", trimParam.dig_P1, trimParam.dig_P1);
    fprintf(stdout, "trimParam.dig_P2 = %d (0x%.4x)\n", trimParam.dig_P2, trimParam.dig_P2);
    fprintf(stdout, "trimParam.dig_P3 = %d (0x%.4x)\n", trimParam.dig_P3, trimParam.dig_P3);
    fprintf(stdout, "trimParam.dig_P4 = %d (0x%.4x)\n", trimParam.dig_P4, trimParam.dig_P4);
    fprintf(stdout, "trimParam.dig_P5 = %d (0x%.4x)\n", trimParam.dig_P5, trimParam.dig_P5);
    fprintf(stdout, "trimParam.dig_P6 = %d (0x%.4x)\n", trimParam.dig_P6, trimParam.dig_P6);
    fprintf(stdout, "trimParam.dig_P7 = %d (0x%.4x)\n", trimParam.dig_P7, trimParam.dig_P7);
    fprintf(stdout, "trimParam.dig_P8 = %d (0x%.4x)\n", trimParam.dig_P8, trimParam.dig_P8);
    fprintf(stdout, "trimParam.dig_P9 = %d (0x%.4x)\n", trimParam.dig_P9, trimParam.dig_P9);

    fprintf(stdout, "trimParam.dig_H1 = %d (0x%.1x)\n", trimParam.dig_H1, trimParam.dig_H1);
    fprintf(stdout, "trimParam.dig_H2 = %d (0x%.2x)\n", trimParam.dig_H2, trimParam.dig_H2);
    fprintf(stdout, "trimParam.dig_H3 = %d (0x%.1x)\n", trimParam.dig_H3, trimParam.dig_H3);
    fprintf(stdout, "trimParam.dig_H4 = %d (0x%.2x)\n", trimParam.dig_H4, trimParam.dig_H4);
    fprintf(stdout, "trimParam.dig_H5 = %d (0x%.2x)\n", trimParam.dig_H5, trimParam.dig_H5);
    fprintf(stdout, "trimParam.dig_H6 = %d (0x%.1x)\n", trimParam.dig_H6, trimParam.dig_H6);
  }

  uint32_t pressureRaw;
  uint32_t temperatureRaw;
  uint32_t humidityRaw;
  int32_t t_fine;

  double temperature;
  double pressure;
  double humidity;

  struct timespec waitTime;
  while (1)
  {
    i2c_read_register(0xF3, data, 1);

    i2c_write_register(0xF5, 0x00);
    i2c_write_register(0xF2, 0x01);
    i2c_write_register(0xF4, 0x25);

    i2c_read_register(0xF3, data, 1);
    while ((data[0] & 0x09) != 0x00)
    {
      i2c_read_register(0xF3, data, 1);
    }

    i2c_read_register(0xF7, data, 8);
    pressureRaw = (((uint32_t) data[0]) << 12) | (((uint32_t) data[1]) << 4) | ((data[2] & 0xF0) >> 4) ;
    temperatureRaw = (((uint32_t) data[3]) << 12) | (((uint32_t) data[4]) << 4) | ((data[5] & 0xF0) >> 4) ;
    humidityRaw = (((uint32_t) data[6]) << 8) | data[7] ;

    fprintf(stdout, "p = 0x%x, t = 0x%x, h = 0x%x\n", pressureRaw, temperatureRaw, humidityRaw);

    temperature = bme280_getTemperature(&trimParam, temperatureRaw, &t_fine);
    pressure = bme280_getPressure(&trimParam, t_fine, pressureRaw);
    humidity = bme280_getHumidity(&trimParam, t_fine, humidityRaw);

    config_altitude = 96.;
    double a = 1 - (config_altitude / 44330.);
    double pressureAtSeaLevel = pressure / pow(a, 5.255);


    fprintf(stdout, "temperature = %f, humidity = %f, pressure = %f (at sea level: %f)\n", temperature, humidity, pressure,
            pressureAtSeaLevel);
    fprintf(stdout, "waiting for next cycle\n");
    waitTime.tv_sec = 1;
    waitTime.tv_nsec = 0;
    nanosleep(&waitTime, NULL);
  }
}


double bme280_getTemperature(trimmingParameter* trimParam, uint32_t temperatureRaw, int32_t* pTFine)
{
  int32_t var1;
  int32_t var2;
  int32_t t;
  int32_t t_fine;

  var1 = ((((temperatureRaw >> 3) - ((int32_t) trimParam->dig_T1 << 1))) * ((int32_t) trimParam->dig_T2)) >> 11;
  var2 = ((((temperatureRaw >> 4) - ((int32_t) trimParam->dig_T1)) * ((temperatureRaw >> 4) - ((
             int32_t) trimParam->dig_T1))) >> 12 * ((int32_t) trimParam->dig_T3)) >> 14;

  t_fine = var1 + var2;
  t = (t_fine * 5 + 128) >> 8;

  *pTFine = t_fine;
  return t / 100.0;
}

double bme280_getPressure(trimmingParameter* trimParam, int32_t t_fine, uint32_t pressureRaw)
{
  int64_t var1;
  int64_t var2;
  int64_t p;

  var1 = ((int64_t) t_fine) - 128000;
  var2 = var1 * var1 * (int64_t) trimParam->dig_P6;
  var2 = var2 + ((var1 * (int64_t) trimParam->dig_P5) << 17);
  var2 = var2 + (((int64_t) trimParam->dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t) trimParam->dig_P3) >> 8) + ((var1 * (int64_t) trimParam->dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t) trimParam->dig_P1) >> 33;

  if (var1 == 0)
  {
    syslog(LOG_NOTICE, "var1 = 0, no pressure calculated");
    return 0; // avoid exception caused by division by zero
  }

  p = 1048576 - pressureRaw;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t) trimParam->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t) trimParam->dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t) trimParam->dig_P7) << 4);

  double pressure = p / 25600.;

  return pressure;
}


double bme280_getHumidity(trimmingParameter* trimParam, int32_t t_fine, uint32_t humidityRaw)
{
  int32_t v_x1_u32r;
  v_x1_u32r = (t_fine - ((int32_t)76800));
  v_x1_u32r = (((((humidityRaw << 14) - (((int32_t) trimParam->dig_H4) << 20) - (((int32_t) trimParam->dig_H5) *
                  v_x1_u32r)) +
                 ((int32_t)16384)) >> 15) *
               (((((((v_x1_u32r * ((int32_t) trimParam->dig_H6)) >> 10) *
                    (((v_x1_u32r * ((int32_t) trimParam->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((
                          int32_t) trimParam->dig_H2) + 8192) >> 14));

  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t) trimParam->dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);

  uint32_t h = (uint32_t)(v_x1_u32r >> 12);
  double humidity = h / 1024.;
  return humidity;
}



#if 0
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

#endif

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



