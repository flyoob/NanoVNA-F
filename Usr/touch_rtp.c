/*-----------------------------------------------------------------------------/
 * Module       : touch_rtp.c
 * Create       : 2019-07-17
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : XTP2046 IQR 具有按下和抬起中断
/-----------------------------------------------------------------------------*/
#include "touch_rtp.h"
#include "system.h"
#include "board.h"
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;
extern volatile int g_TP_Irq;

void rtp_init(void)
{
  // RTP_CS_L();
}

void spi_delay(unsigned int DelayCnt)
{
  unsigned int i;
  for(i=0;i<DelayCnt;i++);
}

uint8_t rtp_writebyte(uint8_t dat)
{
  uint8_t rcv = 0;
  HAL_SPI_TransmitReceive(&hspi1, &dat, &rcv, 1, 1000);
  return rcv;
}

uint16_t TPReadX(void)
{ 
  uint16_t adc = 0;
  RTP_CS_L();
  spi_delay(10);
  rtp_writebyte(0xD0);
  spi_delay(10);
  adc = rtp_writebyte(0x00);
  adc <<= 8;
  adc += rtp_writebyte(0x00);
  spi_delay(10);
  RTP_CS_H();
  adc = adc>>3;
  return (4095 - adc);
}

uint16_t TPReadY(void)
{ 
  uint16_t adc = 0;
  RTP_CS_L();
  spi_delay(10);
  rtp_writebyte(0x90);
  spi_delay(10);
  adc = rtp_writebyte(0x00);
  adc <<= 8;
  adc += rtp_writebyte(0x00);
  spi_delay(10);
  RTP_CS_H();
  adc = adc>>3;
  return (adc);
}
