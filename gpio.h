#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdint.h>

#ifdef GPIO_OLD_API

extern void gpio_reset(const char* gpio_name);

#else

extern int32_t gpio_init(void);
extern int32_t gpio_configure_output(const char* gpio_ctrl_name, int32_t no_line, uint8_t initial_value);
extern int32_t gpio_perform_reset(int32_t fd_output);
extern void gpio_close_all(void);

#endif /* GPIO_OLD_API */

#endif /* __GPIO_H__ */
