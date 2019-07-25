/*-----------------------------------------------------------------------------/
 * Module       : nt35510.c
 * Create       : 2019-05-23
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : NT35510
 TK043F1508, The is a 480(RGB)x800 dot-matrix TFT module.
/-----------------------------------------------------------------------------*/
#include "system.h"
#include "board.h"
#include "stm32f1xx.h"
#include "nt35510.h"
#include "cmsis_os.h"
#include "nanovna.h"
#include "Font.h"

#define BANK1_LCD_C    ((uint32_t)0x60000000)    // Disp Reg  ADDR
#define BANK1_LCD_D    ((uint32_t)0x60020000)    // Disp Data ADDR

uint16_t lcd_buffer[4096];

/*
=======================================
    延时函数
=======================================
*/
static void lcd_delay(int j)
{
  volatile uint16_t i;
  while (j--)
  {
    for(i=7200; i>0; i--);
  }
}

/*
=======================================
    LCD 复位
=======================================
*/
static void lcd_rst(void)
{
  LCD_RST_L();
  lcd_delay(10);
  LCD_RST_H();
  lcd_delay(10);
}

/*
=======================================
    写命令
=======================================
*/
void WriteComm(uint16_t cmd)
{
  *(__IO uint16_t *)(BANK1_LCD_C) = cmd;
}

/*
=======================================
    写数据
=======================================
*/
void WriteData(uint16_t dat)
{
  *(__IO uint16_t *)(BANK1_LCD_D) = dat;
}

/*
=======================================
    set_block
=======================================
*/
void set_block(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye) 
{
  WriteComm(0x2a00);
  WriteData(xs>>8);
  WriteComm(0x2a01);
  WriteData(xs);

  WriteComm(0x2a02);
  WriteData(xe>>8);
  WriteComm(0x2a03);
  WriteData(xe);

  WriteComm(0x2b00);
  WriteData(ys>>8);
  WriteComm(0x2b01);
  WriteData(ys);

  WriteComm(0x2b02);
  WriteData(ye>>8);
  WriteComm(0x2b03);
  WriteData(ye);
}

/*
=======================================
    get_point
=======================================
*/
uint16_t get_point( uint16_t x, uint16_t y)
{
  WriteComm(0x2a00);
  WriteData(x>>8);
  WriteComm(0x2a01);
  WriteData(x);

  WriteComm(0x2b00);
  WriteData(y>>8);
  WriteComm(0x2b01);
  WriteData(y);

  WriteComm(0x2e00); // RAMRD

  x = *(__IO uint16_t *)(BANK1_LCD_D);

  x = *(__IO uint16_t *)(BANK1_LCD_D);

  y = *(__IO uint16_t *)(BANK1_LCD_D);

  // return (x&0xf800)|((x&0x00fc)<<3)|(y>>11); // 返回正确点
  return 0; // 故意返回错误点，令其出现拖影
}

void nt35510_fill(int x, int y, int w, int h, int color)
{
  int i;

  set_block(x, x+w-1, y, y+h-1);

  WriteComm(0x2c00);    // RAMWR

  for (i=0; i<w*h; i++)
  {
    *(__IO uint16_t *)(BANK1_LCD_D) = color;
    // osDelay(1);
  }
}

/*
=======================================
    LCD 初始化
=======================================
*/
void ili9341_init(void)
{
  lcd_rst();

  WriteComm(0xf000);WriteData(0x0055);
  WriteComm(0xf001);WriteData(0x00aa);
  WriteComm(0xf002);WriteData(0x0052);
  WriteComm(0xf003);WriteData(0x0008);
  WriteComm(0xf004);WriteData(0x0001);
  // VGMP/VGSP
  WriteComm(0xbc01);WriteData(0x0086);
  WriteComm(0xbc02);WriteData(0x006a);
  // VGMN/VGSN
  WriteComm(0xbd01);WriteData(0x0086);
  WriteComm(0xbd02);WriteData(0x006a);
  // VCOM
  WriteComm(0xbe01);WriteData(0x0067);

  // Gamma Setting
  WriteComm(0xd100);WriteData(0x0000);
  WriteComm(0xd101);WriteData(0x005d);
  WriteComm(0xd102);WriteData(0x0000);
  WriteComm(0xd103);WriteData(0x006b);
  WriteComm(0xd104);WriteData(0x0000);
  WriteComm(0xd105);WriteData(0x0084);
  WriteComm(0xd106);WriteData(0x0000);
  WriteComm(0xd107);WriteData(0x009c);
  WriteComm(0xd108);WriteData(0x0000);
  WriteComm(0xd109);WriteData(0x00b1);
  WriteComm(0xd10a);WriteData(0x0000);
  WriteComm(0xd10b);WriteData(0x00d9);
  WriteComm(0xd10c);WriteData(0x0000);
  WriteComm(0xd10d);WriteData(0x00fd);
  WriteComm(0xd10e);WriteData(0x0001);
  WriteComm(0xd10f);WriteData(0x0038);
  WriteComm(0xd110);WriteData(0x0001);
  WriteComm(0xd111);WriteData(0x0068);
  WriteComm(0xd112);WriteData(0x0001);
  WriteComm(0xd113);WriteData(0x00b9);
  WriteComm(0xd114);WriteData(0x0001);
  WriteComm(0xd115);WriteData(0x00fb);
  WriteComm(0xd116);WriteData(0x0002);
  WriteComm(0xd117);WriteData(0x0063);
  WriteComm(0xd118);WriteData(0x0002);
  WriteComm(0xd119);WriteData(0x00b9);
  WriteComm(0xd11a);WriteData(0x0002);
  WriteComm(0xd11b);WriteData(0x00bb);
  WriteComm(0xd11c);WriteData(0x0003);
  WriteComm(0xd11d);WriteData(0x0003);
  WriteComm(0xd11e);WriteData(0x0003);
  WriteComm(0xd11f);WriteData(0x0046);
  WriteComm(0xd120);WriteData(0x0003);
  WriteComm(0xd121);WriteData(0x0069);
  WriteComm(0xd122);WriteData(0x0003);
  WriteComm(0xd123);WriteData(0x008f);
  WriteComm(0xd124);WriteData(0x0003);
  WriteComm(0xd125);WriteData(0x00a4);
  WriteComm(0xd126);WriteData(0x0003);
  WriteComm(0xd127);WriteData(0x00b9);
  WriteComm(0xd128);WriteData(0x0003);
  WriteComm(0xd129);WriteData(0x00c7);
  WriteComm(0xd12a);WriteData(0x0003);
  WriteComm(0xd12b);WriteData(0x00c9);
  WriteComm(0xd12c);WriteData(0x0003);
  WriteComm(0xd12d);WriteData(0x00cb);
  WriteComm(0xd12e);WriteData(0x0003);
  WriteComm(0xd12f);WriteData(0x00cb);
  WriteComm(0xd130);WriteData(0x0003);
  WriteComm(0xd131);WriteData(0x00cb);
  WriteComm(0xd132);WriteData(0x0003);
  WriteComm(0xd133);WriteData(0x00cc);

  WriteComm(0xd200);WriteData(0x0000);
  WriteComm(0xd201);WriteData(0x005d);
  WriteComm(0xd202);WriteData(0x0000);
  WriteComm(0xd203);WriteData(0x006b);
  WriteComm(0xd204);WriteData(0x0000);
  WriteComm(0xd205);WriteData(0x0084);
  WriteComm(0xd206);WriteData(0x0000);
  WriteComm(0xd207);WriteData(0x009c);
  WriteComm(0xd208);WriteData(0x0000);
  WriteComm(0xd209);WriteData(0x00b1);
  WriteComm(0xd20a);WriteData(0x0000);
  WriteComm(0xd20b);WriteData(0x00d9);
  WriteComm(0xd20c);WriteData(0x0000);
  WriteComm(0xd20d);WriteData(0x00fd);
  WriteComm(0xd20e);WriteData(0x0001);
  WriteComm(0xd20f);WriteData(0x0038);
  WriteComm(0xd210);WriteData(0x0001);
  WriteComm(0xd211);WriteData(0x0068);
  WriteComm(0xd212);WriteData(0x0001);
  WriteComm(0xd213);WriteData(0x00b9);
  WriteComm(0xd214);WriteData(0x0001);
  WriteComm(0xd215);WriteData(0x00fb);
  WriteComm(0xd216);WriteData(0x0002);
  WriteComm(0xd217);WriteData(0x0063);
  WriteComm(0xd218);WriteData(0x0002);
  WriteComm(0xd219);WriteData(0x00b9);
  WriteComm(0xd21a);WriteData(0x0002);
  WriteComm(0xd21b);WriteData(0x00bb);
  WriteComm(0xd21c);WriteData(0x0003);
  WriteComm(0xd21d);WriteData(0x0003);
  WriteComm(0xd21e);WriteData(0x0003);
  WriteComm(0xd21f);WriteData(0x0046);
  WriteComm(0xd220);WriteData(0x0003);
  WriteComm(0xd221);WriteData(0x0069);
  WriteComm(0xd222);WriteData(0x0003);
  WriteComm(0xd223);WriteData(0x008f);
  WriteComm(0xd224);WriteData(0x0003);
  WriteComm(0xd225);WriteData(0x00a4);
  WriteComm(0xd226);WriteData(0x0003);
  WriteComm(0xd227);WriteData(0x00b9);
  WriteComm(0xd228);WriteData(0x0003);
  WriteComm(0xd229);WriteData(0x00c7);
  WriteComm(0xd22a);WriteData(0x0003);
  WriteComm(0xd22b);WriteData(0x00c9);
  WriteComm(0xd22c);WriteData(0x0003);
  WriteComm(0xd22d);WriteData(0x00cb);
  WriteComm(0xd22e);WriteData(0x0003);
  WriteComm(0xd22f);WriteData(0x00cb);
  WriteComm(0xd230);WriteData(0x0003);
  WriteComm(0xd231);WriteData(0x00cb);
  WriteComm(0xd232);WriteData(0x0003);
  WriteComm(0xd233);WriteData(0x00cc);

  WriteComm(0xd300);WriteData(0x0000);
  WriteComm(0xd301);WriteData(0x005d);
  WriteComm(0xd302);WriteData(0x0000);
  WriteComm(0xd303);WriteData(0x006b);
  WriteComm(0xd304);WriteData(0x0000);
  WriteComm(0xd305);WriteData(0x0084);
  WriteComm(0xd306);WriteData(0x0000);
  WriteComm(0xd307);WriteData(0x009c);
  WriteComm(0xd308);WriteData(0x0000);
  WriteComm(0xd309);WriteData(0x00b1);
  WriteComm(0xd30a);WriteData(0x0000);
  WriteComm(0xd30b);WriteData(0x00d9);
  WriteComm(0xd30c);WriteData(0x0000);
  WriteComm(0xd30d);WriteData(0x00fd);
  WriteComm(0xd30e);WriteData(0x0001);
  WriteComm(0xd30f);WriteData(0x0038);
  WriteComm(0xd310);WriteData(0x0001);
  WriteComm(0xd311);WriteData(0x0068);
  WriteComm(0xd312);WriteData(0x0001);
  WriteComm(0xd313);WriteData(0x00b9);
  WriteComm(0xd314);WriteData(0x0001);
  WriteComm(0xd315);WriteData(0x00fb);
  WriteComm(0xd316);WriteData(0x0002);
  WriteComm(0xd317);WriteData(0x0063);
  WriteComm(0xd318);WriteData(0x0002);
  WriteComm(0xd319);WriteData(0x00b9);
  WriteComm(0xd31a);WriteData(0x0002);
  WriteComm(0xd31b);WriteData(0x00bb);
  WriteComm(0xd31c);WriteData(0x0003);
  WriteComm(0xd31d);WriteData(0x0003);
  WriteComm(0xd31e);WriteData(0x0003);
  WriteComm(0xd31f);WriteData(0x0046);
  WriteComm(0xd320);WriteData(0x0003);
  WriteComm(0xd321);WriteData(0x0069);
  WriteComm(0xd322);WriteData(0x0003);
  WriteComm(0xd323);WriteData(0x008f);
  WriteComm(0xd324);WriteData(0x0003);
  WriteComm(0xd325);WriteData(0x00a4);
  WriteComm(0xd326);WriteData(0x0003);
  WriteComm(0xd327);WriteData(0x00b9);
  WriteComm(0xd328);WriteData(0x0003);
  WriteComm(0xd329);WriteData(0x00c7);
  WriteComm(0xd32a);WriteData(0x0003);
  WriteComm(0xd32b);WriteData(0x00c9);
  WriteComm(0xd32c);WriteData(0x0003);
  WriteComm(0xd32d);WriteData(0x00cb);
  WriteComm(0xd32e);WriteData(0x0003);
  WriteComm(0xd32f);WriteData(0x00cb);
  WriteComm(0xd330);WriteData(0x0003);
  WriteComm(0xd331);WriteData(0x00cb);
  WriteComm(0xd332);WriteData(0x0003);
  WriteComm(0xd333);WriteData(0x00cc);

  WriteComm(0xd400);WriteData(0x0000);
  WriteComm(0xd401);WriteData(0x005d);
  WriteComm(0xd402);WriteData(0x0000);
  WriteComm(0xd403);WriteData(0x006b);
  WriteComm(0xd404);WriteData(0x0000);
  WriteComm(0xd405);WriteData(0x0084);
  WriteComm(0xd406);WriteData(0x0000);
  WriteComm(0xd407);WriteData(0x009c);
  WriteComm(0xd408);WriteData(0x0000);
  WriteComm(0xd409);WriteData(0x00b1);
  WriteComm(0xd40a);WriteData(0x0000);
  WriteComm(0xd40b);WriteData(0x00d9);
  WriteComm(0xd40c);WriteData(0x0000);
  WriteComm(0xd40d);WriteData(0x00fd);
  WriteComm(0xd40e);WriteData(0x0001);
  WriteComm(0xd40f);WriteData(0x0038);
  WriteComm(0xd410);WriteData(0x0001);
  WriteComm(0xd411);WriteData(0x0068);
  WriteComm(0xd412);WriteData(0x0001);
  WriteComm(0xd413);WriteData(0x00b9);
  WriteComm(0xd414);WriteData(0x0001);
  WriteComm(0xd415);WriteData(0x00fb);
  WriteComm(0xd416);WriteData(0x0002);
  WriteComm(0xd417);WriteData(0x0063);
  WriteComm(0xd418);WriteData(0x0002);
  WriteComm(0xd419);WriteData(0x00b9);
  WriteComm(0xd41a);WriteData(0x0002);
  WriteComm(0xd41b);WriteData(0x00bb);
  WriteComm(0xd41c);WriteData(0x0003);
  WriteComm(0xd41d);WriteData(0x0003);
  WriteComm(0xd41e);WriteData(0x0003);
  WriteComm(0xd41f);WriteData(0x0046);
  WriteComm(0xd420);WriteData(0x0003);
  WriteComm(0xd421);WriteData(0x0069);
  WriteComm(0xd422);WriteData(0x0003);
  WriteComm(0xd423);WriteData(0x008f);
  WriteComm(0xd424);WriteData(0x0003);
  WriteComm(0xd425);WriteData(0x00a4);
  WriteComm(0xd426);WriteData(0x0003);
  WriteComm(0xd427);WriteData(0x00b9);
  WriteComm(0xd428);WriteData(0x0003);
  WriteComm(0xd429);WriteData(0x00c7);
  WriteComm(0xd42a);WriteData(0x0003);
  WriteComm(0xd42b);WriteData(0x00c9);
  WriteComm(0xd42c);WriteData(0x0003);
  WriteComm(0xd42d);WriteData(0x00cb);
  WriteComm(0xd42e);WriteData(0x0003);
  WriteComm(0xd42f);WriteData(0x00cb);
  WriteComm(0xd430);WriteData(0x0003);
  WriteComm(0xd431);WriteData(0x00cb);
  WriteComm(0xd432);WriteData(0x0003);
  WriteComm(0xd433);WriteData(0x00cc);

  WriteComm(0xd500);WriteData(0x0000);
  WriteComm(0xd501);WriteData(0x005d);
  WriteComm(0xd502);WriteData(0x0000);
  WriteComm(0xd503);WriteData(0x006b);
  WriteComm(0xd504);WriteData(0x0000);
  WriteComm(0xd505);WriteData(0x0084);
  WriteComm(0xd506);WriteData(0x0000);
  WriteComm(0xd507);WriteData(0x009c);
  WriteComm(0xd508);WriteData(0x0000);
  WriteComm(0xd509);WriteData(0x00b1);
  WriteComm(0xd50a);WriteData(0x0000);
  WriteComm(0xd50b);WriteData(0x00D9);
  WriteComm(0xd50c);WriteData(0x0000);
  WriteComm(0xd50d);WriteData(0x00fd);
  WriteComm(0xd50e);WriteData(0x0001);
  WriteComm(0xd50f);WriteData(0x0038);
  WriteComm(0xd510);WriteData(0x0001);
  WriteComm(0xd511);WriteData(0x0068);
  WriteComm(0xd512);WriteData(0x0001);
  WriteComm(0xd513);WriteData(0x00b9);
  WriteComm(0xd514);WriteData(0x0001);
  WriteComm(0xd515);WriteData(0x00fb);
  WriteComm(0xd516);WriteData(0x0002);
  WriteComm(0xd517);WriteData(0x0063);
  WriteComm(0xd518);WriteData(0x0002);
  WriteComm(0xd519);WriteData(0x00b9);
  WriteComm(0xd51a);WriteData(0x0002);
  WriteComm(0xd51b);WriteData(0x00bb);
  WriteComm(0xd51c);WriteData(0x0003);
  WriteComm(0xd51d);WriteData(0x0003);
  WriteComm(0xd51e);WriteData(0x0003);
  WriteComm(0xd51f);WriteData(0x0046);
  WriteComm(0xd520);WriteData(0x0003);
  WriteComm(0xd521);WriteData(0x0069);
  WriteComm(0xd522);WriteData(0x0003);
  WriteComm(0xd523);WriteData(0x008f);
  WriteComm(0xd524);WriteData(0x0003);
  WriteComm(0xd525);WriteData(0x00a4);
  WriteComm(0xd526);WriteData(0x0003);
  WriteComm(0xd527);WriteData(0x00b9);
  WriteComm(0xd528);WriteData(0x0003);
  WriteComm(0xd529);WriteData(0x00c7);
  WriteComm(0xd52a);WriteData(0x0003);
  WriteComm(0xd52b);WriteData(0x00c9);
  WriteComm(0xd52c);WriteData(0x0003);
  WriteComm(0xd52d);WriteData(0x00cb);
  WriteComm(0xd52e);WriteData(0x0003);
  WriteComm(0xd52f);WriteData(0x00cb);
  WriteComm(0xd530);WriteData(0x0003);
  WriteComm(0xd531);WriteData(0x00cb);
  WriteComm(0xd532);WriteData(0x0003);
  WriteComm(0xd533);WriteData(0x00cc);

  WriteComm(0xd600);WriteData(0x0000);
  WriteComm(0xd601);WriteData(0x005d);
  WriteComm(0xd602);WriteData(0x0000);
  WriteComm(0xd603);WriteData(0x006b);
  WriteComm(0xd604);WriteData(0x0000);
  WriteComm(0xd605);WriteData(0x0084);
  WriteComm(0xd606);WriteData(0x0000);
  WriteComm(0xd607);WriteData(0x009c);
  WriteComm(0xd608);WriteData(0x0000);
  WriteComm(0xd609);WriteData(0x00b1);
  WriteComm(0xd60a);WriteData(0x0000);
  WriteComm(0xd60b);WriteData(0x00d9);
  WriteComm(0xd60c);WriteData(0x0000);
  WriteComm(0xd60d);WriteData(0x00fd);
  WriteComm(0xd60e);WriteData(0x0001);
  WriteComm(0xd60f);WriteData(0x0038);
  WriteComm(0xd610);WriteData(0x0001);
  WriteComm(0xd611);WriteData(0x0068);
  WriteComm(0xd612);WriteData(0x0001);
  WriteComm(0xd613);WriteData(0x00b9);
  WriteComm(0xd614);WriteData(0x0001);
  WriteComm(0xd615);WriteData(0x00fb);
  WriteComm(0xd616);WriteData(0x0002);
  WriteComm(0xd617);WriteData(0x0063);
  WriteComm(0xd618);WriteData(0x0002);
  WriteComm(0xd619);WriteData(0x00b9);
  WriteComm(0xd61a);WriteData(0x0002);
  WriteComm(0xd61b);WriteData(0x00bb);
  WriteComm(0xd61c);WriteData(0x0003);
  WriteComm(0xd61d);WriteData(0x0003);
  WriteComm(0xd61e);WriteData(0x0003);
  WriteComm(0xd61f);WriteData(0x0046);
  WriteComm(0xd620);WriteData(0x0003);
  WriteComm(0xd621);WriteData(0x0069);
  WriteComm(0xd622);WriteData(0x0003);
  WriteComm(0xd623);WriteData(0x008f);
  WriteComm(0xd624);WriteData(0x0003);
  WriteComm(0xd625);WriteData(0x00a4);
  WriteComm(0xd626);WriteData(0x0003);
  WriteComm(0xd627);WriteData(0x00b9);
  WriteComm(0xd628);WriteData(0x0003);
  WriteComm(0xd629);WriteData(0x00c7);
  WriteComm(0xd62a);WriteData(0x0003);
  WriteComm(0xd62b);WriteData(0x00c9);
  WriteComm(0xd62c);WriteData(0x0003);
  WriteComm(0xd62d);WriteData(0x00cb);
  WriteComm(0xd62e);WriteData(0x0003);
  WriteComm(0xd62f);WriteData(0x00cb);
  WriteComm(0xd630);WriteData(0x0003);
  WriteComm(0xd631);WriteData(0x00cb);
  WriteComm(0xd632);WriteData(0x0003);
  WriteComm(0xd633);WriteData(0x00cc);

  WriteComm(0xba00);WriteData(0x0024);
  WriteComm(0xba01);WriteData(0x0024);
  WriteComm(0xba02);WriteData(0x0024);

  WriteComm(0xb900);WriteData(0x0024);
  WriteComm(0xb901);WriteData(0x0024);
  WriteComm(0xb902);WriteData(0x0024);

  // LV2 Page
  WriteComm(0xf000);WriteData(0x0055);
  WriteComm(0xf001);WriteData(0x00aa);
  WriteComm(0xf002);WriteData(0x0052);
  WriteComm(0xf003);WriteData(0x0008);
  WriteComm(0xf004);WriteData(0x0000);

  // Display control
  WriteComm(0xb100);WriteData(0x00cc);
  // Inversion mode
  WriteComm(0xbc00);WriteData(0x0005);
  WriteComm(0xbc01);WriteData(0x0005);
  WriteComm(0xbc02);WriteData(0x0005);
  // Source EQ control
  WriteComm(0xb800);WriteData(0x0001);
  WriteComm(0xb801);WriteData(0x0003);
  WriteComm(0xb802);WriteData(0x0003);
  WriteComm(0xb803);WriteData(0x0003);
  // VGMN/VGSN
  WriteComm(0xbd02);WriteData(0x0007);
  WriteComm(0xbd03);WriteData(0x0031);
  // VCOM
  WriteComm(0xbe02);WriteData(0x0007);
  WriteComm(0xbe03);WriteData(0x0031);
  // VGH
  WriteComm(0xbf02);WriteData(0x0007);
  WriteComm(0xbf03);WriteData(0x0031);

  WriteComm(0xff00);WriteData(0x00aa);
  WriteComm(0xff01);WriteData(0x0055);
  WriteComm(0xff02);WriteData(0x0025);
  WriteComm(0xff03);WriteData(0x0001);

  WriteComm(0xf304);WriteData(0x0011);
  WriteComm(0xf306);WriteData(0x0010);
  WriteComm(0xf308);WriteData(0x0000);

  WriteComm(0x3500);WriteData(0x0000);  // Tearing effect mode set & on

  WriteComm(0x3A00);WriteData(0x0055);  // COLMOD Interface pixel format 16bit
  WriteComm(0x3600);WriteData(0x0060);  // MADCTL Memory data access control RGB-BGR Order

  WriteComm(0x2a00);WriteData(0x0000);  // Column address set Start 0
  WriteComm(0x2a01);WriteData(0x0000);
  WriteComm(0x2a02);WriteData(0x0003);  // Column address set End
  WriteComm(0x2a03);WriteData(0x001f);

  WriteComm(0x2b00);WriteData(0x0000);  // Row address set Start 0
  WriteComm(0x2b01);WriteData(0x0000);
  WriteComm(0x2b02);WriteData(0x0001);  // Row address set End
  WriteComm(0x2b03);WriteData(0x00df);

  WriteComm(0x1100);                    // Sleep out & booster on
  lcd_delay(50);

  WriteComm(0x2900);                    // Display on
  lcd_delay(50);

  nt35510_fill(0, 0, 800, 480, BLACK);

  // Add Logo
  // nt35510_fill();
}

/*
=======================================
    画一个像素点
=======================================
*/
void ili9341_pixel(int x, int y, int color)
{
  int i;

  set_block(x*2, x*2+1, y*2, y*2+1);  // 扩展为2倍
  WriteComm(0x2c00);    // RAMWR

  for (i=0; i<4; i++)
  {
    *(__IO uint16_t *)(BANK1_LCD_D) = color;
  }
}

/*
=======================================
    x x方向的起始点
    y y方向的终止点
    w 要选定矩形的x方向长度
    h 要选定矩形的y方向长度
=======================================
*/
void ili9341_fill(int x, int y, int w, int h, int color)
{
  int i;

  set_block(x*2, x*2+w*2-1, y*2, y*2+h*2-1);  // 扩展为2倍
  WriteComm(0x2c00);    // RAMWR

  for (i=0; i<w*2*h*2; i++)
  {
    *(__IO uint16_t *)(BANK1_LCD_D) = color;
  }
}

/*
=======================================
    填充缓存
=======================================
*/
#if 0 // NO DMA
void ili9341_bulk(int x, int y, int w, int h)
{
  uint8_t xx[4] = { x >> 8, x, (x+w-1) >> 8, (x+w-1) };
  uint8_t yy[4] = { y >> 8, y, (y+h-1) >> 8, (y+h-1) };
  uint16_t *buf = lcd_buffer;
  int len = w * h;
  send_command(0x2A, 4, xx);
  send_command(0x2B, 4, yy);
  send_command(0x2C, 0, NULL);
  while (len-- > 0)
    ssp_senddata16(*buf++);
}
#else
void ili9341_bulk(int x, int y, int w, int h)
{
  int i,j;
  uint16_t *buf = lcd_buffer;

  set_block(x*2, x*2+w*2-1, y*2, y*2+h*2-1);  // 扩展为2倍
  WriteComm(0x2c00);    // RAMWR

  for (i=0; i<h; i++)
  {
    for (j=0; j<w; j++)
    {
      *(__IO uint16_t *)(BANK1_LCD_D) = *buf;
      *(__IO uint16_t *)(BANK1_LCD_D) = *buf;
      buf++;
    }

    buf = buf-w;

    for (j=0; j<w; j++)
    {
      *(__IO uint16_t *)(BANK1_LCD_D) = *buf;
      *(__IO uint16_t *)(BANK1_LCD_D) = *buf;
      buf++;
    }
  }
}
#endif

void ili9341_drawchar_5x7(uint8_t ch, int x, int y, uint16_t fg, uint16_t bg)
{
  uint16_t *buf = lcd_buffer;
  uint16_t bits;
  int c, r;
  for(c = 0; c < 7; c++) {
    bits = x5x7_bits[(ch * 7) + c];
    for (r = 0; r < 5; r++) {
      *buf++ = (0x8000 & bits) ? fg : bg;
      bits <<= 1;
    }
  }
  ili9341_bulk(x, y, 5, 7);
}

void ili9341_drawstring_5x7(const char *str, int x, int y, uint16_t fg, uint16_t bg)
{
  while (*str) {
    ili9341_drawchar_5x7(*str, x, y, fg, bg);
    x += 5;
    str++;
  }
}

#define SWAP(x,y) do { int z=x; x = y; y = z; } while(0)

void ili9341_line(int x0, int y0, int x1, int y1, uint16_t fg)
{
  if (x0 > x1) {
    SWAP(x0, x1);
    SWAP(y0, y1);
  }

  while (x0 <= x1) {
    int dx = x1 - x0 + 1;
    int dy = y1 - y0;
    if (dy >= 0) {
      dy++;
      if (dy > dx) {
        dy /= dx; dx = 1;
      } else {
        dx /= dy; dy = 1;
      }
    } else {
      dy--;
      if (-dy > dx) {
        dy /= dx; dx = 1;
      } else {
        dx /= -dy; dy = -1;
      }
    }
    if (dy > 0)
      ili9341_fill(x0, y0, dx, dy, fg);
    else
      ili9341_fill(x0, y0+dy, dx, -dy, fg);
    x0 += dx;
    y0 += dy;
  }
}

const font_t NF20x24 = { 20, 24, 1, 24, (const uint32_t *)numfont20x24 };
//const font_t NF32x24 = { 32, 24, 1, 24, (const uint32_t *)numfont32x24 };
//const font_t NF32x48 = { 32, 48, 2, 24, (const uint32_t *)numfont32x24 };

void ili9341_drawfont(uint8_t ch, const font_t *font, int x, int y, uint16_t fg, uint16_t bg)
{
  uint16_t *buf = lcd_buffer;
  uint32_t bits;
  const uint32_t *bitmap = &font->bitmap[font->slide * ch];
  int c, r, j;

  for (c = 0; c < font->slide; c++) {
    for (j = 0; j < font->scaley; j++) {
      bits = bitmap[c];
      for (r = 0; r < font->width; r++) {
        *buf++ = (0x80000000UL & bits) ? fg : bg;
        bits <<= 1;
      }
    }
  }
  ili9341_bulk(x, y, font->width, font->height);
}

#if 1
const uint16_t colormap[] = {
  BRG556(255,0,0), BRG556(0,255,0), BRG556(0,0,255),  // 蓝 红 绿
  BRG556(255,255,0), BRG556(0,255,255), BRG556(255,0,255)  // 紫 黄 青蓝
};

void ili9341_test(int mode)
{
  int x, y;
  int i;
  switch (mode) {
  default:
#if 1
    ili9341_fill(0, 0, LCD_WIDTH, LCD_HEIGHT, 0);
    for (y = 0; y < LCD_HEIGHT; y++) {
      ili9341_fill(0, y, LCD_WIDTH, 1, BRG556(y, (y + 120) % 256, LCD_HEIGHT-y));
    }
    break;
  case 1:
    ili9341_fill(0, 0, LCD_WIDTH, LCD_HEIGHT, 0);
    for (y = 0; y < LCD_HEIGHT; y++) {
      for (x = 0; x < LCD_WIDTH; x++) {
        ili9341_pixel(x, y, (y<<8)|x);
      }
    }
    break;
  case 2:
    //send_command16(0x55, 0xff00);
    ili9341_pixel(64, 64, 0xaa55);
    break;
#endif
#if 1
    case 3:
      for (i = 0; i < 10; i++)
        ili9341_drawfont(i, &NF20x24, i*20, 120, colormap[i%6], 0x0000);
      break;
#endif
#if 0
    case 4:
      draw_grid(10, 8, 29, 29, 15, 0, 0xffff, 0);
      break;
#endif
    case 4:
      ili9341_line(0, 0, 399, 0, 0xffff);
      ili9341_line(0, 0, 0, 239, 0xffff);
      ili9341_line(0, 239, 399, 239, 0xffff);
      ili9341_line(399, 0, 399, 239, 0xffff);
      break;
    }
}
#endif

/*
=======================================
    区块刷新
=======================================
*/
void nt35510_bulk(int x, int y, int w, int h)
{
  int i,j;
  uint16_t *buf = lcd_buffer;

  set_block(x, x+w-1, y, y+h-1);
  WriteComm(0x2c00);    // RAMWR

  for (i=0; i<h; i++)
  {
    for (j=0; j<w; j++)
    {
      *(__IO uint16_t *)(BANK1_LCD_D) = *buf;
      buf++;
    }
  }
}

/*
=======================================
    显示字符
=======================================
*/
void nt35510_drawchar(MWCFONT *font, uint8_t ch, int x, int y, uint16_t fg, uint16_t bg)
{
  uint16_t *buf = lcd_buffer;
  uint16_t bits;
  int c, r;
  for(c = 0; c < font->height; c++) {
    bits = font->bits[(ch * font->height) + c];
    for (r = 0; r < font->maxwidth; r++) {
      *buf++ = (0x8000 & bits) ? fg : bg;
      bits <<= 1;
    }
  }
  nt35510_bulk(x, y, font->maxwidth, font->height);
}

/*
=======================================
    显示字符串
=======================================
*/
void nt35510_drawstring(MWCFONT *font, const char *str, int x, int y, uint16_t fg, uint16_t bg)
{
  while (*str) {
    nt35510_drawchar(font, *str, x, y, fg, bg);
    x += font->maxwidth;
    str++;
  }
}

/*
=======================================
    显示汉字 24x24
=======================================
*/
extern const uint8_t hanzi_24x24_bits[];
void nt35510_drawhz24x24(const char *str, int x, int y, uint16_t fg, uint16_t bg)
{
  x = x*2;
  y = y*2;
  uint16_t *buf;
  uint8_t bits;
  int c, r;
  while (*str) {
    buf = lcd_buffer;
    for(c = 0; c < 24*3; c++) {
      bits = hanzi_24x24_bits[(*str)*72 + c];
      for (r = 0; r < 8; r++) {
        *buf++ = (0x80 & bits) ? fg : bg;
        bits <<= 1;
      }
    }
    nt35510_bulk(x, y, 24, 24);
    x += 24;
    str++;
  }
}

/*
=======================================
    显示字符
=======================================
*/
void nt35510_drawchar_x2(MWCFONT *font, uint8_t ch, int x, int y, uint16_t fg, uint16_t bg)
{
  uint16_t *buf = lcd_buffer;
  uint16_t bits;
  int c, r;
  for(c = 0; c < font->height; c++) {
    bits = font->bits[(ch * font->height) + c];
    for (r = 0; r < font->maxwidth; r++) {
      *buf++ = (0x8000 & bits) ? fg : bg;
      bits <<= 1;
    }
  }
  ili9341_bulk(x, y, font->maxwidth, font->height);
}

/*
=======================================
    显示字符串
=======================================
*/
void nt35510_drawstring_x2(MWCFONT *font, const char *str, int x, int y, uint16_t fg, uint16_t bg)
{
  while (*str) {
    nt35510_drawchar_x2(font, *str, x, y, fg, bg);
    x += font->maxwidth;
    str++;
  }
}

void nt35510_drawchar_5x7(uint8_t ch, int x, int y, uint16_t fg, uint16_t bg)
{
  uint16_t *buf = lcd_buffer;
  uint16_t bits;
  int c, r;
  for(c = 0; c < 14; c++) {  // 经过调整
    bits = _10x20_bits[(ch * 20) + c + 2];  // 经过调整
    for (r = 0; r < 10; r++) {
      *buf++ = (0x8000 & bits) ? fg : bg;
      bits <<= 1;
    }
  }
  nt35510_bulk(x, y, 10, 14);
}

void nt35510_drawstring_5x7(const char *str, int x, int y, uint16_t fg, uint16_t bg)
{
  x = x*2;
  y = y*2;
  while (*str) {
    nt35510_drawchar_5x7(*str, x, y, fg, bg);
    x += 10;
    str++;
  }
}
