/*-----------------------------------------------------------------------------/
 * Module       : touch_ctp.c
 * Create       : 2019-06-06
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 电容屏 FT5316 芯片驱动
  True Multi-touch with up to 5 Points of Absolution X and Y Coordinates
  FT5216 supports up to 16 TX lines + 10 RX lines
  FT5316 supports up to 21 TX lines + 12 RX lines
  High Report Rate: More than 100Hz
  Touch Resolution of 100 Dots per Inch (dpi) or above depending on the Panel Size
  Optimal Sensing Mutual Capacitor: 1pF~4pF
  3 Operating Modes
    Active
    Monitor
    Hibernate
/-----------------------------------------------------------------------------*/
#include "touch_ctp.h"
#include "system.h"
#include "board.h"
#include "cmsis_os.h"

typedef struct
{
  uint16_t cx1; // CTP_X1
  uint16_t cy1; // CTP_Y1
  uint16_t cx2; // CTP_X2
  uint16_t cy2; // CTP_Y2
} CTP_Stru;

CTP_Stru CTP_Dat;

typedef struct 
{
  uint8_t packet_id;
  uint8_t xh_yh;
  uint8_t xl;
  uint8_t yl;
  uint8_t dxh_dyh;
  uint8_t dxl;
  uint8_t dyl;
  uint8_t checksum;
} TpdTouchDataS;

volatile int g_TP_Irq = 0;

// void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
// {
    // if (GPIO_Pin == GPIO_PIN_7) {
        // g_TP_Irq = 1;
    // }
// }

uint8_t CheckSum(uint8_t *buf)
{
  uint8_t i;
  uint16_t sum = 0;

  for(i=0; i<7; i++)
  {
    sum += buf[i];
  }

  sum &= 0xff;
  sum = 0x0100-sum;
  sum &= 0xff;

  return (sum == buf[7]);
}

int ctp_writereg(uint8_t reg, uint8_t *pbuf, uint8_t len)
{
  uint8_t i;
  // CPU发送开始位
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定写模式
  I2C_SendByte(FT_ADDR << 1);  // 0 = W
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
  for(i=0; i<len; i++)
  {
    I2C_SendByte(pbuf[i]);
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

int ctp_readreg(uint8_t reg, uint8_t *pbuf, uint8_t len)
{
  uint8_t i;
  // CPU发送开始位
  I2C_Start();
  // 传输从地址，用R/W位设定写模式位设定写模式
  I2C_SendByte(FT_ADDR << 1);  // 0 = W
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
  I2C_SendByte((FT_ADDR << 1) | 0x01);  // 1 = R
  // 等待器件应答
  if (I2C_WaitAck() != 0) {
    goto cmd_fail;
  }
  // CPU读取指定的寄存器内容
  for (i=0; i<len; i++)
  {
    pbuf[i]=I2C_ReadByte(i==(len-1)?0:1);
  }
  // CPU发送停止位
  I2C_Stop();
  return 1;

cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
  /* 发送I2C总线停止信号 */
  I2C_Stop();
  return 0;
}

void ctp_init(void)
{
  uint8_t val = 0;
  ctp_writereg(FT_DEVIDE_MODE, &val, 1);
  ctp_writereg(FT_ID_G_MODE, &val, 1);
  // val = 10;
  // ctp_writereg(FT_ID_G_PERIODACTIVE, &val, 1);
}

uint8_t CTP_Read(uint8_t flag)
{
  uint16_t DCX = 0, DCY = 0;

  TpdTouchDataS TpdTouchData;
  // memset((uint8_t*)&TpdTouchData, 0, sizeof(TpdTouchData));

  // Read the CTP
  if (ctp_readreg(0, (uint8_t *)&TpdTouchData, sizeof(TpdTouchData)))
  {
    // printf("CTP Read Fail!\r\n");
    return 0;
  }

  // Check The ID of CTP
  if(TpdTouchData.packet_id != 0x52)
  {
    // printf("The ID of CTP is False!\r\n");
    return 0;
  }

  // CheckSum
  if(!CheckSum((uint8_t*)(&TpdTouchData)))
  {
    // printf("Checksum is False!\r\n");
    return 0;
  }
  
  // The Key Of TP
  if(TpdTouchData.xh_yh == 0xff && TpdTouchData.xl == 0xff
      && TpdTouchData.yl == 0xff && TpdTouchData.dxh_dyh == 0xff
      && TpdTouchData.dyl == 0xff)
  {
    /*
    switch(TpdTouchData.dxl)
    {
        case 0:    return 0;
        case 1: printf("R-KEY\r\n"); break; // Right Key
        case 2: printf("M-KEY\r\n"); break; // Middle Key
        case 4: printf("L-KEY\r\n"); break; // Left Key
        default:;
    }
    */
  }
  else 
  {
    // The First Touch Point
    CTP_Dat.cx1 = (TpdTouchData.xh_yh&0xf0)<<4 | TpdTouchData.xl;
    CTP_Dat.cy1 = (TpdTouchData.xh_yh&0x0f)<<8 | TpdTouchData.yl;

    // The Second Touch Point
    if (TpdTouchData.dxh_dyh != 0 || TpdTouchData.dxl != 0 || TpdTouchData.dyl != 0)
    {    
      DCX = (TpdTouchData.dxh_dyh&0xf0)<<4 | TpdTouchData.dxl;
      DCY = (TpdTouchData.dxh_dyh&0x0f)<<8 | TpdTouchData.dyl;

      DCX <<= 4;
      DCX >>= 4;
      DCY <<= 4;
      DCY >>= 4;

      if (DCX >= 2048)
        DCX -= 4096;
      if (DCY >= 2048)
        DCY -= 4096;

      CTP_Dat.cx2 = CTP_Dat.cx1 + DCX;
      CTP_Dat.cy2 = CTP_Dat.cy1 + DCY;
    }
  }

  if (CTP_Dat.cx1 == 0 && CTP_Dat.cy1 == 0 && CTP_Dat.cx2 == 0 && CTP_Dat.cy2 == 0)
  {
    return 0;
  }
  
  // if (flag)
  // {    
      // printf("#CP%04d,%04d!%04d,%04d;%04d,%04d\r\n",0,0,CTP_Dat.cx1,CTP_Dat.cy1,CTP_Dat.cx2,CTP_Dat.cy2);
      // memset((uint8_t*)&CTP_Dat, 0, sizeof(CTP_Dat));
  // }

  return 1;
}

static uint16_t touchX=0, touchY=0;

int GUI_TOUCH_X_MeasureX(void)
{
  uint8_t buf[7];

  ctp_readreg(0, (uint8_t *)&buf, 7);

  if ((buf[2]&0x0F) == 1) {
    touchX = (uint16_t)(buf[5] & 0x0F)<<8 | (uint16_t)buf[6];
    touchY = (uint16_t)(buf[3] & 0x0F)<<8 | (uint16_t)buf[4];
  } else {
    touchX = 0;
    touchY = 0;
  }
  return touchX;
}

int GUI_TOUCH_X_MeasureY(void)
{
  return touchY;
}

void Touch_Test(void)
{
  uint8_t buf[7];
  int16_t x1, y1;

  // GUI_SetColor(GUI_BLUE);
  // GUI_SetFont(&GUI_Font32B_1);
  // GUI_DispStringAt("x =",60,0);
  // GUI_DispStringAt("y =",160,0);

  while(1)
  {
    // ctp_writereg1(0,0);
    // a = ctp_readreg1(0);
    // GUI_DispDecAt(a, 0, 50, 4);
    
    // a = ctp_readreg1(0xa3);
    // GUI_DispDecAt(a, 0, 0, 4);
    // a = ctp_readreg1(0xa6);
    // GUI_DispDecAt(a, 100, 0, 4);
    // a = ctp_readreg1(0xa8);
    // GUI_DispDecAt(a, 200, 0, 4);

    // a = ctp_readreg1(0xa7);
    // GUI_DispDecAt(a, 300, 0, 4);
    
    if (1)
    {
      // keyId = 0;
      ctp_readreg(0, (uint8_t*)&buf, 7);

      if ((buf[2]&0x0F) == 1)
      {
        // 读出的数据位480*800
        x1 = (int16_t)(buf[3] & 0x0F)<<8 | (int16_t)buf[4];
        y1 = (int16_t)(buf[5] & 0x0F)<<8 | (int16_t)buf[6];
      }
      else
      {
        x1 = 0xFFFF;
        y1 = 0xFFFF;
      }

      if ((x1>0)||(y1>0))
      {
        // GUI_DispDecAt(x1, 100, 0, 3);
        // GUI_DispDecAt(y1, 200, 0, 3);
      }
    }
  }
}
