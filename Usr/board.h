/*-----------------------------------------------------------------------------/
 * Module       : board.h
 * Create       : 2019-05-23
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 
/-----------------------------------------------------------------------------*/
#ifndef _BOARD_H
#define _BOARD_H

#include "system.h"

#define LED1_ON         HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
#define LED1_OFF        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
#define LED1_TOG        HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);

#define AIC_RESET_H     HAL_GPIO_WritePin(AIC_RST_GPIO_Port, AIC_RST_Pin, GPIO_PIN_SET);
#define AIC_RESET_L     HAL_GPIO_WritePin(AIC_RST_GPIO_Port, AIC_RST_Pin, GPIO_PIN_RESET);

#define SDIO_CD_IN()    HAL_GPIO_ReadPin(SDIO_CD_GPIO_Port, SDIO_CD_Pin)

#define LCD_RST_H()     HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
#define LCD_RST_L()     HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);

#define CTP_RST_H()     // HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET);
#define CTP_RST_L()     // HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET);
#define CTP_INT_IN()    HAL_GPIO_ReadPin(TP_IRQ_GPIO_Port, TP_IRQ_Pin)

#define RTP_CS_L()      HAL_GPIO_WritePin(SPI_CS0_GPIO_Port, SPI_CS0_Pin, GPIO_PIN_RESET);
#define RTP_CS_H()      HAL_GPIO_WritePin(SPI_CS0_GPIO_Port, SPI_CS0_Pin, GPIO_PIN_SET);

#define FLASH_CS_L()    HAL_GPIO_WritePin(SPI_CS1_GPIO_Port, SPI_CS1_Pin, GPIO_PIN_RESET);
#define FLASH_CS_H()    HAL_GPIO_WritePin(SPI_CS1_GPIO_Port, SPI_CS1_Pin, GPIO_PIN_SET);

extern void systick_call(void);

void    I2C_Start(void);
void    I2C_Stop(void);
void    I2C_SendByte(uint8_t Byte);
uint8_t I2C_ReadByte(uint8_t ack);
uint8_t I2C_WaitAck(void);
void    I2C_Ack(void);
void    I2C_NAck(void);

void    I2C_InitGPIO(void);
int     i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t dat);
int     i2c_write_regs(uint8_t addr, uint8_t reg, uint8_t *dat, uint8_t cnt);
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg);
int     i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *dat, uint8_t cnt);

int     str2hex(uint8_t *dst, char *src);

void    beep_open(int ms);
void    beep_tick(void);

#endif
