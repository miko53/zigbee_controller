#ifndef DISPLAY_H
#define DISPLAY_H

#include "zigbee.h"
#include <stdint.h>

extern void display_decodedType(zigbee_decodedFrame* decodedData);
extern void display_frame(char* displayTextHeader, uint8_t* frame, uint32_t size);



#endif /* DISPLAY_H */