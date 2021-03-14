

#ifndef __CONFIGFILE_H__
#define __CONFIGFILE_H__

#include <stdint.h>

extern char* config_scriptName;
extern uint8_t* config_panID;
extern uint32_t config_nbPanID;
extern char* config_ttydevice;
extern char* config_gpio_reset;
extern char* config_device;
extern double config_altitude;
extern char* config_fifo_name;
extern uint16_t config_scan_channel;
extern char* config_gpio_ctrl_name;
extern uint32_t config_gpio_line;

extern int32_t configfile_read(const char filename[]);




#endif /* __CONFIGFILE_H__ */
