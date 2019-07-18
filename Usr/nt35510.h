/*-----------------------------------------------------------------------------/
 * Module       : nt35510.h
 * Create       : 2019-05-23
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : NT35510
 TK043F1508, The is a 480(RGB)x800 dot-matrix TFT module.
/-----------------------------------------------------------------------------*/
#ifndef _NT35510_H
#define _NT35510_H

#define   BLACK                0x0000                // 黑色：    0,   0,   0 //
// #define   BLUE                 0x001F                // 蓝色：    0,   0, 255 //
// #define   GREEN                0x07E0                // 绿色：    0, 255,   0 //
// #define   CYAN                 0x07FF                // 青色：    0, 255, 255 //
// #define   RED                  0xF800                // 红色：  255,   0,   0 //
// #define   MAGENTA              0xF81F                // 品红：  255,   0, 255 //
// #define   YELLOW               0xFFE0                // 黄色：  255, 255, 0   //
#define   WHITE                0xFFFF                // 白色：  255, 255, 255 //
// #define   NAVY                 0x000F                // 深蓝色：  0,   0, 128 //
// #define   DGREEN               0x03E0                // 深绿色：  0, 128,   0 //
// #define   DCYAN                0x03EF                // 深青色：  0, 128, 128 //
// #define   MAROON               0x7800                // 深红色：128,   0,   0 //
// #define   PURPLE               0x780F                // 紫色：  128,   0, 128 //
// #define   OLIVE                0x7BE0                // 橄榄绿：128, 128,   0 //
// #define   LGRAY                0xC618                // 灰白色：192, 192, 192 //
// #define   DGRAY                0x7BEF                // 深灰色：128, 128, 128 //

void nt35510_fill(int x, int y, int w, int h, int color);

#endif
