
#include "configfile.h"
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

char* config_scriptName;
char* config_ttydevice;
char* config_gpio_reset;
char* config_device;
uint8_t* config_panID;
uint32_t config_nbPanID;
double config_altitude;

static int32_t configfile_doRead(FILE* f);
static int32_t configfile_decodeLine(char line[]);
static int32_t configfile_createConfig(char key[], char value[]);

int32_t configfile_read(const char filename[])
{
  FILE* f;
  int32_t rc;

  f = fopen(filename, "r");
  if (f == NULL)
  {
    fprintf(stderr, "unable to open %s\n", filename);
  }
  else
  {
    rc = configfile_doRead(f);
    fclose(f);
  }

  return rc;
}

#define SIZE_CURRENT_LINE   (256)


static int32_t configfile_doRead(FILE* f)
{
  char currentLine[SIZE_CURRENT_LINE];
  char* endRead;
  int32_t rc;
  uint32_t lineNo;

  lineNo = 0;
  do
  {
    endRead = fgets(currentLine, SIZE_CURRENT_LINE, f);
    if (endRead != NULL)
    {
      rc = configfile_decodeLine(currentLine);
      if (rc != 0)
      {
        fprintf(stderr, "syntax error near line %d\n", lineNo);
      }
    }
    lineNo++;
  }
  while (!feof(f) && (rc == 0));

  return rc;
}

enum
{
  IDLE,
  DECODE_KEY,
  WAIT_EQUAL,
  VALUE_WAIT_VALUE,
  VALUE,
  VALUE_STRING
};

static int32_t configfile_decodeLine(char line[])
{
  int32_t rc;
  uint32_t index;
  char* key;
  char* value;
  int64_t state;
  bool bEndDecoding;
  char currentChar;

  rc = 0;
  state = IDLE;
  bEndDecoding = false;
  index = 0;
  while (bEndDecoding == false)
  {
    currentChar = line[index];
    if (currentChar == '#')
    {
      bEndDecoding = true;
      line[index] = '\0';
    }
    else
    {
      switch (state)
      {
        case IDLE:
          if ((currentChar == '\0') || (currentChar == '\n'))
          {
            bEndDecoding = true;
          }
          else if (!isspace(currentChar))
          {
            state = DECODE_KEY;
            key = &line[index];
          }
          break;

        case DECODE_KEY:
          if ((currentChar == '\0') || (currentChar == '\n'))
          {
            bEndDecoding = true;
          }
          else if (!isalnum(currentChar) && (currentChar != '_') && (currentChar != '=') )
          {
            state = WAIT_EQUAL;
            line[index] = '\0';
          }
          else if (currentChar == '=')
          {
            state = VALUE_WAIT_VALUE;
            line[index] = '\0';
          }
          break;

        case WAIT_EQUAL:
          if ((currentChar == '\0') || (currentChar == '\n'))
          {
            bEndDecoding = true;
          }
          else if (currentChar == '=')
          {
            state = VALUE_WAIT_VALUE;
          }
          else if (!isspace(currentChar))
          {
            bEndDecoding = true;
          }
          break;

        case VALUE_WAIT_VALUE:
          if ((currentChar == '\0') || (currentChar == '\n'))
          {
            bEndDecoding = true;
          }
          else if (!isspace(currentChar) && (currentChar != '"'))
          {
            state = VALUE;
            value = &line[index];
          }
          else if (currentChar == '"')
          {
            state = VALUE_STRING;
            value = &line[index + 1];
          }
          break;

        case VALUE_STRING:
          if (currentChar == '"')
          {
            bEndDecoding = true;
            line[index] = '\0';
          }
          break;

        case VALUE:
          if ((currentChar == '\0') || (currentChar == '\n'))
          {
            line[index] = '\0';
            bEndDecoding = true;
          }
          else if (isspace(currentChar))
          {
            line[index] = '\0';
            bEndDecoding = true;
          }
          break;

        default:
          assert(false);
          break;
      }
    }
    index++;
  }

  if ((state == VALUE) || (state == VALUE_STRING))
  {
    //      fprintf(stdout, "key = '%s', value = '%s'\n", key, value);
    rc = configfile_createConfig( key, value);
  }
  else if (state == IDLE)
  {
    //not in error, ignore
  }
  else
  {
    rc = -1;
  }

  return rc;
}

static int32_t configfile_createConfig(char key[], char value[])
{
  int32_t rc;
  char* token;
  char* endConversion;
  uint32_t v;
  uint32_t i;
  uint32_t max;

  i = 0;
  max = 8;
  rc = 0;

  if (strcmp(key, "panID") == 0)
  {
    if (config_panID == NULL)
    {
      config_panID = malloc(sizeof(uint8_t) * max);
      assert(config_panID != NULL);
    }

    token = strtok(value, ", ");
    while (token != NULL)
    {
      //       fprintf(stdout, "token = %s\n", token);
      v = strtoul(token, &endConversion, 0);
      if (*endConversion == '\0')
      {
        config_panID[i] = v;
        if (i == max)
        {
          uint8_t* n;
          n = realloc(config_panID, max * 2);
          assert(n != NULL);
          config_panID = n;
          max = max * 2;
        }
        i++;
      }
      token = strtok(NULL, ", ");
    }
    config_nbPanID = i;
  }
  else if (strcmp(key, "script") == 0)
  {
    config_scriptName = malloc(strlen(value) + 1);
    assert(config_scriptName != NULL);
    strcpy(config_scriptName, value);
  }
  else if (strcmp(key, "ttydevice") == 0)
  {
    config_ttydevice = malloc(strlen(value) + 1);
    assert(config_ttydevice != NULL);
    strcpy(config_ttydevice, value);
  }
  else if (strcmp(key, "gpio_reset") == 0)
  {
    config_gpio_reset = malloc(strlen(value) + 1);
    assert(config_gpio_reset != NULL);
    strcpy(config_gpio_reset, value);
  }
  else if (strcmp(key, "device") == 0)
  {
    config_device = malloc(strlen(value) + 1);
    assert(config_device != NULL);
    strcpy(config_device, value);
  }
  else if (strcmp(key, "altitude") == 0)
  {
    double v;
    v = strtod(value, &endConversion);
    if (*endConversion == '\0')
    {
      config_altitude = v;
    }
  }
  else
  {
    rc = -1;
  }

  return rc;
}
