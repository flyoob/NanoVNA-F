/*-----------------------------------------------------------------------------/
 * Module       : system.h
 * Create       : 2019-05-23
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 
/-----------------------------------------------------------------------------*/
#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* 公用 */
#include "stm32f1xx_hal.h"
#include "main.h"

#include "usbd_cdc_if.h"

/* 延时 */
#define _delay_us(us)
#define _delay_ms(ms)              HAL_Delay(ms)

#define FALSE 0
#define TRUE  1

extern char *FreeRTOS_CLIGetOutputBuffer( void );

#define debug(format, ...)    // ...

#define dbprintf(format, ...) { \
    sprintf(FreeRTOS_CLIGetOutputBuffer(), format, ##__VA_ARGS__); \
    CDC_Transmit_FS((uint8_t* )FreeRTOS_CLIGetOutputBuffer(), strlen(FreeRTOS_CLIGetOutputBuffer()));\
}

#define chprintf(a, format, ...) { \
    sprintf(FreeRTOS_CLIGetOutputBuffer(), format, ##__VA_ARGS__); \
    CDC_Transmit_FS((uint8_t* )FreeRTOS_CLIGetOutputBuffer(), strlen(FreeRTOS_CLIGetOutputBuffer()));\
}

#define chsnprintf                snprintf

#define chThdSleepMilliseconds    osDelay

#define chVTGetSystemTime         HAL_GetTick

#endif
