/*-----------------------------------------------------------------------------/
 * Module       : board.c
 * Create       : 2019-05-23
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 
/-----------------------------------------------------------------------------*/
#include "system.h"
#include "board.h"

#define I2C_WR                   0    // 写控制 bit
#define I2C_RD                   1    // 读控制 bit

#define I2C_GPIO_CLK_ENABLE()    __HAL_RCC_GPIOB_CLK_ENABLE()
#define I2C_GPIO_PORT            GPIOB
#define I2C_SCL_PIN              GPIO_PIN_8
#define I2C_SDA_PIN              GPIO_PIN_9

#define I2C_SCL_HIGH()           HAL_GPIO_WritePin(I2C_GPIO_PORT, I2C_SCL_PIN, GPIO_PIN_SET)
#define I2C_SCL_LOW()            HAL_GPIO_WritePin(I2C_GPIO_PORT, I2C_SCL_PIN, GPIO_PIN_RESET)
#define I2C_SDA_HIGH()           HAL_GPIO_WritePin(I2C_GPIO_PORT, I2C_SDA_PIN, GPIO_PIN_SET)
#define I2C_SDA_LOW()            HAL_GPIO_WritePin(I2C_GPIO_PORT, I2C_SDA_PIN, GPIO_PIN_RESET)
#define I2C_SDA_READ()           HAL_GPIO_ReadPin(I2C_GPIO_PORT, I2C_SDA_PIN)

extern TIM_HandleTypeDef htim1;

void    I2C_InitGPIO(void);
uint8_t I2C_CheckDevice(uint8_t addr);

/*
=======================================
    系统中断
=======================================
*/
void systick_call(void)
{

}

/*
=======================================
    I2C 延时
=======================================
*/
static void I2C_Delay(void)
{
  uint8_t i;
  for (i = 0; i < 15; i++);  // STM32F103VET6 主频 72MHz MDK -O3 优化，实测 7us，I2C CLK = 142KHZ
  // for (i = 0; i < 20; i++);  // STM32F103VET6 主频 72MHz MDK -O3 优化，周期 9us，I2C CLK = 100KHZ
  // for (i = 0; i < 50; i++);  // STM32F103VET6 主频 72MHz MDK -O3 优化，周期 20us，I2C CLK = 50KHZ
}

/*
=======================================
    I2C 测试
=======================================
*/
void I2C_Test(void)
{
  // LED_TOG;
  I2C_Delay();
}

/*
=======================================
    CPU发起I2C总线启动信号
=======================================
*/
void I2C_Start(void)
{
  /* 当SCL高电平时，SDA出现一个下跳沿表示I2C总线启动信号 */
  I2C_SDA_HIGH();
  I2C_SCL_HIGH();
  I2C_Delay();
  I2C_SDA_LOW();
  I2C_Delay();
  I2C_SCL_LOW();
  I2C_Delay();
}

/*
=======================================
    CPU发起I2C总线停止信号
=======================================
*/
void I2C_Stop(void)
{
  /* 当SCL高电平时，SDA出现一个上跳沿表示I2C总线停止信号 */
  I2C_SDA_LOW();
  I2C_SCL_HIGH();
  I2C_Delay();
  I2C_SDA_HIGH();
}

/*
=======================================
    CPU产生一个ACK信号
=======================================
*/
void I2C_Ack(void)
{
  I2C_SCL_LOW();
  __NOP();
  I2C_SDA_LOW();     /* CPU驱动SDA = 0 */
  I2C_Delay();
  I2C_SCL_HIGH();    /* CPU产生1个时钟 */
  I2C_Delay();
  I2C_SCL_LOW();
  I2C_Delay();
  I2C_SDA_HIGH();    /* CPU释放SDA总线 */
}

/*
=======================================
    CPU产生1个NACK信号
=======================================
*/
void I2C_NAck(void)
{
  I2C_SCL_LOW();
  __NOP();
  I2C_SDA_HIGH();    /* CPU驱动SDA = 1 */
  I2C_Delay();
  I2C_SCL_HIGH();    /* CPU产生1个时钟 */
  I2C_Delay();
  I2C_SCL_LOW();
  I2C_Delay();
}

/*
=======================================
    CPU向I2C总线设备发送8bit数据
=======================================
*/
void I2C_SendByte(uint8_t Byte)
{
  uint8_t i;

  /* 先发送字节的高位bit7 */
  for (i = 0; i < 8; i++)
  {    
    if (Byte & 0x80)
    {
      I2C_SDA_HIGH();
    } else {
      I2C_SDA_LOW();
    }
    I2C_Delay();
    I2C_SCL_HIGH();
    I2C_Delay();    
    I2C_SCL_LOW();
    if (i == 7)
    {
      I2C_SDA_HIGH();  /* 释放总线 */
    }
    Byte <<= 1;    /* 左移一个bit */
    I2C_Delay();
  }
}

/*
=======================================
    CPU从I2C总线设备读取8bit数据
=======================================
*/
uint8_t I2C_ReadByte(uint8_t ack)
{
  uint8_t i;
  uint8_t value;

  /* 读到第1个bit为数据的bit7 */
  value = 0;
  for (i = 0; i < 8; i++)
  {
    value <<= 1;
    I2C_SCL_HIGH();
    I2C_Delay();
    if (I2C_SDA_READ())
    {
      value++;
    }
    I2C_SCL_LOW();
    I2C_Delay();
  }

  if (!ack)
    I2C_NAck();  // 发送 nACK
  else
    I2C_Ack();   // 发送 ACK

  return value;
}

/*
=======================================
    CPU产生一个时钟，并读取器件的ACK应答信号
    返回0表示正确应答，1表示无器件响应
=======================================
*/
uint8_t I2C_WaitAck(void)
{
  uint8_t re;

  I2C_SDA_HIGH();    /* CPU释放SDA总线 */
  I2C_Delay();

  I2C_SCL_HIGH();    /* CPU驱动SCL = 1, 此时器件会返回ACK应答 */
  I2C_Delay();
  
  if (I2C_SDA_READ())    /* CPU读取SDA口线状态 */
  {
    re = 1;
  } else {
    re = 0;
  }
  I2C_SCL_LOW();
  I2C_Delay();

  return re;
}

/*
=======================================
    I2C 初始化
=======================================
*/
void I2C_InitGPIO(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  /* 打开GPIO时钟 */
  I2C_GPIO_CLK_ENABLE();

  GPIO_InitStruct.Pin = I2C_SCL_PIN|I2C_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(I2C_GPIO_PORT, &GPIO_InitStruct);

  /* 给一个停止信号, 复位I2C总线上的所有设备到待机模式 */
  I2C_Stop();
}

/*
=======================================
    检测I2C总线设备，CPU向发送设备地址，然后读取设备应答来判断该设备是否存在
    addr：设备的I2C总线地址
    返回值 0 表示正确， 返回1表示未探测到
    在访问I2C设备前，请先调用 I2C_CheckDevice() 检测I2C设备是否正常
=======================================
*/
uint8_t I2C_CheckDevice(uint8_t addr)
{
  uint8_t ucAck;

  I2C_Start();              /* 发送启动信号 */
  /* 发送设备地址+读写控制bit（0 = w， 1 = r) bit7 先传 */
  I2C_SendByte((addr << 1) | 0);
  ucAck = I2C_WaitAck();    /* 检测设备的ACK应答 */
  I2C_Stop();               /* 发送停止信号 */

  return ucAck;
}

/*
=======================================
    I2C 写单个寄存器
=======================================
*/
int i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t dat)
{
  // CPU发送开始位
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定写模式
  I2C_SendByte(addr << 1);  // 0 = W
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // 发送要写的 reg
  I2C_SendByte(reg);
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU将要写入的数据写道指定的寄存器
  I2C_SendByte(dat);
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU发送停止位
  I2C_Stop();
  return 1;

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
  /* 发送I2C总线停止信号 */
  I2C_Stop();
  return 0;
}

/*
=======================================
    I2C 写多个寄存器
=======================================
*/
int i2c_write_regs(uint8_t addr, uint8_t reg, uint8_t *dat, uint8_t cnt)
{
  // CPU发送开始位
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定写模式
  I2C_SendByte(addr << 1);  // 0 = W
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // 发送要写的 reg
  I2C_SendByte(reg);
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU将要写入的数据写道指定的寄存器
  while (cnt --)
  {
    I2C_SendByte(*(dat ++));
    if (I2C_WaitAck() != 0) {
      goto cmd_fail;
    }
  }

  // CPU发送停止位
  I2C_Stop();
  return 1;

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
  /* 发送I2C总线停止信号 */
  I2C_Stop();
  return 0;
}

/*
=======================================
    I2C 读单个寄存器
=======================================
*/
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg)
{
  uint8_t dat = 0x00;

  // CPU发送开始位
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定写模式
  I2C_SendByte(addr << 1);  // 0 = W
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // 发送要读的 reg
  I2C_SendByte(reg);
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU发送RESTART条件
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定读模式
  I2C_SendByte((addr << 1) | 0x01);  // 1 = R
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU读取指定的寄存器内容
  dat = I2C_ReadByte(0);
  // CPU发送停止位
  I2C_Stop();
  return dat;

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
  /* 发送I2C总线停止信号 */
  I2C_Stop();
  return 0;
}

/*
=======================================
    I2C 写多个寄存器
=======================================
*/
int i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *dat, uint8_t cnt)
{
  // CPU发送开始位
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定写模式
  I2C_SendByte(addr << 1);  // 0 = W
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // 发送要读的 reg
  I2C_SendByte(reg);
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU发送RESTART条件
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定读模式
  I2C_SendByte((addr << 1) | 0x01);  // 1 = R
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU读取指定的寄存器内容
  while (cnt --)
  {
    *(dat ++) = I2C_ReadByte(0);
  }
  // CPU发送停止位
  I2C_Stop();
  return 1;

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
  /* 发送I2C总线停止信号 */
  I2C_Stop();
  return 0;
}

/*
=======================================
    16进制字符转数字
=======================================
*/
uint8_t c2i(char ch)
{
  /* 如果是数字，则用数字的 ASCII 码减去 48 */
  if ((ch <= '9') && (ch >= '0'))
    return (ch - 48);
  /* 如果是大写字母，则用数字的 ASCII 码减去 55 */
  if ((ch <= 'F') && (ch >= 'A'))
    return (ch - 55);
  /* 如果是小写字母，则用数字的 ASCII 码减去 87 */
  if ((ch <= 'f') && (ch >= 'a'))
    return (ch - 87);
  return 0;
}

/*
=======================================
    16进制字符串转数组，返回转换后数组长度
=======================================
*/
int str2hex(uint8_t *dst, char *src)
{
  int cnt, len;
  len = strlen(src);
  if (len % 2)
    return 0;
  for (cnt=0; cnt<(len/2); cnt++)
  {
    dst[cnt] = c2i(src[cnt*2])*16 + c2i(src[cnt*2+1]);
  }
  return cnt;
}

int g_BeepMs = 0;
/*
=======================================
    蜂鸣器打开时间
=======================================
*/
void beep_open(int ms)
{
  g_BeepMs = ms;
  if (g_BeepMs > 0) {
    BEEP_ON();
  }
}

/*
=======================================
    蜂鸣器打开时间
=======================================
*/
void beep_tick(void)
{
  if (g_BeepMs > 0) {
    g_BeepMs --;
    if (g_BeepMs == 0) {
      BEEP_OFF();
    }
  }
}
