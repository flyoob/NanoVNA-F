/*-----------------------------------------------------------------------------/
 * Module       : touch_ctp.h
 * Create       : 2019-06-06
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 电容屏 FT5316 芯片驱动（实际使用）
/-----------------------------------------------------------------------------*/
#ifndef _TOUCH_CTP_H
#define _TOUCH_CTP_H

#include <stdint.h>

#define FT_ADDR                (0x70 >> 1)
#define FT_DEVIDE_MODE          0x00        // 0 Normal operating Mode
#define FT_GEST_ID              0x01        // 触摸手势
#define FT_TD_STATUS            0x02        // Touch Data status register. TD[3:0]的取值范围是：1~5，表示有多少个有效触摸点

#define FT_ID_G_THGROUP         0x80        // 120/4 Valid touching detect threshold. 越小越灵敏
#define FT_ID_G_PERIODACTIVE    0x88        // 6 60HZ
#define FT_ID_G_MODE            0xA4        // 1 Trigger mode

extern volatile int g_TP_Irq;

void ctp_init(void);

int ctp_writereg(uint8_t reg, uint8_t *pbuf, uint8_t len);
int ctp_readreg(uint8_t reg, uint8_t *pbuf, uint8_t len);

int GUI_TOUCH_X_MeasureX(void); 
int GUI_TOUCH_X_MeasureY(void);

#endif
