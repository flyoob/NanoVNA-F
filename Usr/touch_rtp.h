/*-----------------------------------------------------------------------------/
 * Module       : touch_rtp.h
 * Create       : 2019-07-17
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : XTP2046
/-----------------------------------------------------------------------------*/
#ifndef _TOUCH_RTP_H
#define _TOUCH_RTP_H

#include <stdint.h>

extern volatile int g_TP_Irq;

void rtp_init(void);

uint16_t TPReadX(void);
uint16_t TPReadY(void);

#endif
