/*
 * Copyright (c) 2014-2015, TAKAHASHI Tomohiro (TTRFTECH) edy555@gmail.com
 * All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * The software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
#include "board.h"
#include "si5351.h"
#include "nanovna.h"
#include "cmsis_os.h"

#define SI5351_I2C_ADDR    (0x60)

extern void tlv320aic3204_set_gain(int lgain, int rgain);

/*
=======================================
    si5351 写单个寄存器
=======================================
*/
static void si5351_write(uint8_t reg, uint8_t dat)
{
  i2c_write_reg(SI5351_I2C_ADDR, reg, dat);
}

/*
=======================================
    si5351 写多个寄存器
=======================================
*/
static void si5351_bulk_write(const uint8_t *buf, int len)
{
  i2c_write_regs(SI5351_I2C_ADDR, buf[0], (uint8_t *)&buf[1], len-1);
}

/*
=======================================
    初始化写入的寄存器值 Floor 向下取整
    小数分频模式
    P3 20bit = c
    P2 20bit = 128 * b - c * floor(128*b/c)
    P1 18bit = 128 * a - c * floor(128*b/c) - 512
    P3 = 1
    P2 = 0;
    P1 = (32/2 - 2)*256 = 14*256, a = 32
    倍频和分频系数都为 a+b/c
    说明：
    VCO frequency in the range of 600 to 900 MHz.
    Either of these two VCO frequencies can be divided down  between 500 kHz and 200 MHz. 
    有效系数 4, 6, 8, and any fractional value between 8 + 1/1,048,575 and 900 + 0/1.
=======================================
*/
// length, register addr, data, ...
const uint8_t si5351_configs[] = {
  2, SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0xff,  // 关闭所有输出
  4, SI5351_REG_16_CLK0_CONTROL, SI5351_CLK_POWERDOWN, SI5351_CLK_POWERDOWN, SI5351_CLK_POWERDOWN,  // 关闭所有CLK电源
  2, SI5351_REG_183_CRYSTAL_LOAD, SI5351_CRYSTAL_LOAD_8PF,  // 为什么，修改后有问题
  // setup PLLA (26MHz * 32 = 832MHz, 32/2-2=14) a =32
  9, SI5351_REG_26_PLL_A, /*P3*/0, 1, /*P1*/0, 14, 0, /*P3/P2*/0, 0, 0,  // Multisynth NA => PLLA
  9, SI5351_REG_34_PLL_B, /*P3*/0, 1, /*P1*/0, 14, 0, /*P3/P2*/0, 0, 0,  // Multisynth NB => PLLB
  // RESET PLLA PLLB
  2, SI5351_REG_177_PLL_RESET, SI5351_PLL_RESET_A | SI5351_PLL_RESET_B,
  // setup multisynth (832MHz / 104 = 8MHz, 104/2-2=50) a = 104
  9, SI5351_REG_58_MULTISYNTH2, /*P3*/0, 1, /*P1*/0, 50, 0, /*P2|P3*/0, 0, 0,  
  // 只打开 CLK2 输出 8MHz PLLB Select MultiSynth 2 as the source for CLK2. 可选输出关闭=(1 << 7)
  2, SI5351_REG_18_CLK2_CONTROL, SI5351_CLK_DRIVE_STRENGTH_2MA | SI5351_CLK_INPUT_MULTISYNTH_N | SI5351_CLK_INTEGER_MODE | SI5351_CLK_PLL_SELECT_B,
  2, SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0,  // 使能输出所有时钟
  0 // sentinel
};

/*
=======================================
    si5351 初始化
    只打开 CLK2 输出 8MHz
=======================================
*/
void si5351_init(void)
{
  const uint8_t *p = si5351_configs;
  while (*p) {
    uint8_t len = *p++;
    si5351_bulk_write(p, len);
    p += len;
  }
}

/*
=======================================
    si5351 关闭输出
=======================================
*/
void si5351_disable_output(void)
{
  uint8_t reg[4];
  si5351_write(SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0xff);
  reg[0] = SI5351_REG_16_CLK0_CONTROL;
  reg[1] = SI5351_CLK_POWERDOWN;
  reg[2] = SI5351_CLK_POWERDOWN;
  reg[3] = SI5351_CLK_POWERDOWN;
  si5351_bulk_write(reg, 4);
}

/*
=======================================
    si5351 打开输出
=======================================
*/
void si5351_enable_output(void)
{
  // si5351_write(SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0x00);
  si5351_write(SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0xFC);
}

/*
=======================================
    si5351 PLL 复位
=======================================
*/
void si5351_reset_pll(void)
{
  // si5351_write(SI5351_REG_177_PLL_RESET, SI5351_PLL_RESET_A | SI5351_PLL_RESET_B);
  si5351_write(SI5351_REG_177_PLL_RESET, 0xAC);
}

/*
=======================================
    si5351 配置 PLL
=======================================
*/
void si5351_setupPLL(uint8_t pll, /* SI5351_PLL_A or SI5351_PLL_B */
                     uint8_t     mult,  // a
                     uint32_t    num,   // b
                     uint32_t    denom) // c
{
  /* Get the appropriate starting point for the PLL registers */
  const uint8_t pllreg_base[] = {
    SI5351_REG_26_PLL_A,
    SI5351_REG_34_PLL_B
  };
  uint32_t P1;
  uint32_t P2;
  uint32_t P3;

  /* Feedback Multisynth Divider Equation
   * where: a = mult, b = num and c = denom
   * P1 register is an 18-bit value using following formula:
   * 	P1[17:0] = 128 * mult + floor(128*(num/denom)) - 512
   * P2 register is a 20-bit value using the following formula:
   * 	P2[19:0] = 128 * num - denom * floor(128*(num/denom))
   * P3 register is a 20-bit value using the following formula:
   * 	P3[19:0] = denom
   */

  /* Set the main PLL config registers */
  if (num == 0)
  {
    /* Integer mode */
    P1 = 128 * mult - 512;
    P2 = 0;
    P3 = 1;
  }
  else
  {
    /* Fractional mode */
    //P1 = (uint32_t)(128 * mult + floor(128 * ((float)num/(float)denom)) - 512);
    P1 = 128 * mult + ((128 * num) / denom) - 512;
    //P2 = (uint32_t)(128 * num - denom * floor(128 * ((float)num/(float)denom)));
    P2 = 128 * num - denom * ((128 * num) / denom);
    P3 = denom;
  }

  /* The datasheet is a nightmare of typos and inconsistencies here! */
  uint8_t reg[9];
  reg[0] = pllreg_base[pll];
  reg[1] = (P3 & 0x0000FF00) >> 8;
  reg[2] = (P3 & 0x000000FF);
  reg[3] = (P1 & 0x00030000) >> 16;
  reg[4] = (P1 & 0x0000FF00) >> 8;
  reg[5] = (P1 & 0x000000FF);
  reg[6] = ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16);
  reg[7] = (P2 & 0x0000FF00) >> 8;
  reg[8] = (P2 & 0x000000FF);
  si5351_bulk_write(reg, 9);
}

/*
=======================================
    si5351 配置分频比
=======================================
*/
void 
si5351_setupMultisynth(uint8_t     output,
                       uint8_t     pllSource,
                       uint32_t    div, // a 4,6,8, 8+ ~ 900
                       uint32_t    num, // b
                       uint32_t    denom, // c
                       uint32_t    rdiv, // SI5351_R_DIV_1~128
                       uint8_t     drive_strength)
{
  /* Get the appropriate starting point for the PLL registers */
  const uint8_t msreg_base[] = {
    SI5351_REG_42_MULTISYNTH0,
    SI5351_REG_50_MULTISYNTH1,
    SI5351_REG_58_MULTISYNTH2,
  };
  const uint8_t clkctrl[] = {
    SI5351_REG_16_CLK0_CONTROL,
    SI5351_REG_17_CLK1_CONTROL,
    SI5351_REG_18_CLK2_CONTROL
  };
  uint8_t dat;

  uint32_t P1;
  uint32_t P2;
  uint32_t P3;
  uint32_t div4 = 0;

  /* Output Multisynth Divider Equations
   * where: a = div, b = num and c = denom
   * P1 register is an 18-bit value using following formula:
   *    P1[17:0] = 128 * a + floor(128*(b/c)) - 512
   * P2 register is a 20-bit value using the following formula:
   *    P2[19:0] = 128 * b - c * floor(128*(b/c))
   * P3 register is a 20-bit value using the following formula:
   *    P3[19:0] = c
   */
  /* Set the main PLL config registers */
  if (div == 4) {
    div4 = SI5351_DIVBY4;
    P1 = P2 = 0;
    P3 = 1;
  } else if (num == 0) {
    /* Integer mode */
    P1 = 128 * div - 512;
    P2 = 0;
    P3 = 1;
  } else {
    /* Fractional mode */
    P1 = 128 * div + ((128 * num) / denom) - 512;
    P2 = 128 * num - denom * ((128 * num) / denom);
    P3 = denom;
  }

  /* Set the MSx config registers */
  uint8_t reg[9];
  reg[0] = msreg_base[output];
  reg[1] = (P3 & 0x0000FF00) >> 8;
  reg[2] = (P3 & 0x000000FF);
  reg[3] = ((P1 & 0x00030000) >> 16) | div4 | rdiv;
  reg[4] = (P1 & 0x0000FF00) >> 8;
  reg[5] = (P1 & 0x000000FF);
  reg[6] = ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16);
  reg[7] = (P2 & 0x0000FF00) >> 8;
  reg[8] = (P2 & 0x000000FF);
  si5351_bulk_write(reg, 9);

  /* Configure the clk control and enable the output */
  dat = drive_strength | SI5351_CLK_INPUT_MULTISYNTH_N;
  if (pllSource == SI5351_PLL_B)
    dat |= SI5351_CLK_PLL_SELECT_B;
  if (num == 0)
    dat |= SI5351_CLK_INTEGER_MODE;
  si5351_write(clkctrl[output], dat);
}

/*
=======================================
    
=======================================
*/
static uint32_t gcd(uint32_t x, uint32_t y)
{
  uint32_t z;
  while (y != 0) {
    z = x % y;
    x = y;
    y = z;
  }
  return x;
}

#define XTALFREQ 26000000L
#define PLL_N 32
#define PLLFREQ (XTALFREQ * PLL_N)

/*
=======================================
    si5351 配置输出频率，倍频不变
=======================================
*/
void
si5351_set_frequency_fixedpll(int channel, int pll, int pllfreq, int freq,
                              uint32_t rdiv, uint8_t drive_strength)
{
  int32_t div = pllfreq / freq; // range: 8 ~ 1800
  int32_t num = pllfreq - freq * div;
  int32_t denom = freq;
  // int32_t k = freq / (1<<20) + 1;
  int32_t k = gcd(num, denom);
  num /= k;
  denom /= k;
  while (denom >= (1<<20)) {
    num >>= 1;
    denom >>= 1;
  }
  // div = a, num 分子, denom 分母
  si5351_setupMultisynth(channel, pll, div, num, denom, rdiv, drive_strength);
}

/*
=======================================
    si5351 配置输出频率，分频不变
=======================================
*/
void
si5351_set_frequency_fixeddiv(int channel, int pll, int freq, int div,
                              uint8_t     drive_strength)
{
  int32_t pllfreq = freq * div;
  int32_t multi = pllfreq / XTALFREQ;
  int32_t num = pllfreq - multi * XTALFREQ;
  int32_t denom = XTALFREQ;
  int32_t k = gcd(num, denom);
  num /= k;
  denom /= k;
  while (denom >= (1<<20)) {
    num >>= 1;
    denom >>= 1;
  }
  si5351_setupPLL(pll, multi, num, denom);
  si5351_setupMultisynth(channel, pll, div, 0, 1, SI5351_R_DIV_1, drive_strength);
}

/* 
 * 1~100MHz fixed PLL 900MHz, fractional divider
 * 100~150MHz fractional PLL 600-900MHz, fixed divider 6
 * 150~BASE_MAX fractional PLL (150-BASE_MAX)*4, fixed divider 4
 */
void
si5351_set_frequency(int channel, int freq, uint8_t drive_strength)
{
  if (freq <= 100000000) {
    si5351_setupPLL(SI5351_PLL_B, 32, 0, 1);
    si5351_set_frequency_fixedpll(channel, SI5351_PLL_B, PLLFREQ, freq, SI5351_R_DIV_1, drive_strength);
  } else if (freq < 150000000) {
    si5351_set_frequency_fixeddiv(channel, SI5351_PLL_B, freq, 6, drive_strength);
  } else {
    si5351_set_frequency_fixeddiv(channel, SI5351_PLL_B, freq, 4, drive_strength);
  }
}

int current_band = -1;
int current_band_c0 = -1;
int current_band_c1 = -1;

/*
 * configure output as follows:
 * CLK0: frequency + offset
 * CLK1: frequency
 * CLK2: fixed 8MHz
 */
#define CLK2_FREQUENCY 8000000L
int
si5351_set_frequency_with_offset(int freq, int offset, uint8_t drive_strength)
{
  int band;
  int delay = 5;
  uint32_t ofreq = freq + offset;
  uint32_t rdiv = SI5351_R_DIV_1;
  if (freq <= 100000000) {  // [50k,100M]
    band = 0;
  } else if (freq < 150000000) {  // (100M,150M)
    band = 1;
  } else {
    band = 2;  // [150M,BASE_MAX]
  }
  if (freq <= 500000) {
    rdiv = SI5351_R_DIV_64;
  } else if (freq <= 4000000) {  // 4M
    rdiv = SI5351_R_DIV_8;
  }

#if 1
  if (current_band != band)
    si5351_disable_output();
#endif

  switch (band) {
  case 0:
  // fractional divider mode. only PLL A is used.
  if (current_band == 1 || current_band == 2)
    si5351_setupPLL(SI5351_PLL_A, 32, 0, 1);
  // Set PLL twice on changing from band 2
  if (current_band == 2) 
    si5351_setupPLL(SI5351_PLL_A, 32, 0, 1);

  if (rdiv == SI5351_R_DIV_8) {
    freq *= 8;
    ofreq *= 8;
  } else if (rdiv == SI5351_R_DIV_64) {
    freq *= 64;
    ofreq *= 64;
  }

  si5351_set_frequency_fixedpll(0, SI5351_PLL_A, PLLFREQ, ofreq,
                                rdiv, SI5351_CLK_DRIVE_STRENGTH_2MA);
  si5351_set_frequency_fixedpll(1, SI5351_PLL_A, PLLFREQ, freq,
                                rdiv, drive_strength);
  //if (current_band != 0)
  si5351_set_frequency_fixedpll(2, SI5351_PLL_A, PLLFREQ, CLK2_FREQUENCY,
                                SI5351_R_DIV_1, SI5351_CLK_DRIVE_STRENGTH_2MA);
  break;

  case 1:
  // Set PLL twice on changing from band 2
  if (current_band == 2) {
    si5351_set_frequency_fixeddiv(0, SI5351_PLL_A, freq + offset, 6,
                                SI5351_CLK_DRIVE_STRENGTH_2MA);
    si5351_set_frequency_fixeddiv(1, SI5351_PLL_B, freq, 6, drive_strength);
  }

  // div by 6 mode. both PLL A and B are dedicated for CLK0, CLK1
  si5351_set_frequency_fixeddiv(0, SI5351_PLL_A, freq + offset, 6,
                                SI5351_CLK_DRIVE_STRENGTH_2MA);
  si5351_set_frequency_fixeddiv(1, SI5351_PLL_B, freq, 6, drive_strength);
  si5351_set_frequency_fixedpll(2, SI5351_PLL_B, freq * 6, CLK2_FREQUENCY,
                                SI5351_R_DIV_1, SI5351_CLK_DRIVE_STRENGTH_2MA);
  break;

  case 2:
  // div by 4 mode. both PLL A and B are dedicated for CLK0, CLK1
  si5351_set_frequency_fixeddiv(1, SI5351_PLL_B, freq, 4, drive_strength);
  si5351_set_frequency_fixedpll(2, SI5351_PLL_B, freq * 4, CLK2_FREQUENCY,
                                SI5351_R_DIV_1, SI5351_CLK_DRIVE_STRENGTH_2MA);
  si5351_set_frequency_fixeddiv(0, SI5351_PLL_A, freq + offset, 4,
                                SI5351_CLK_DRIVE_STRENGTH_2MA);
  break;
  }

  if (current_band != band) {
    si5351_reset_pll();
#if 1
    si5351_enable_output();
#endif
    delay += 0;
    osDelay(30);
  }

  current_band = band;
  return delay;
}

/*
 * configure output as follows:
 * CLK0: frequency + offset          参考/本振
 * CLK1: frequency                   发射
 * CLK2: fixed 8MHz
 * 扩展量程：
 * CLK0=(60+0.005/5, 180+0.005/5]    参考/本振
 * CLK1=(100, BASE_MAX]              发射
 */
#define CLK2_FREQUENCY 8000000L
int
si5351_set_frequency_with_offset_expand(int freq, int offset, uint8_t drive_strength)
{
  int band_c0, band_c1;
  int delay = 5;
  uint32_t rdiv = SI5351_R_DIV_1;

  uint32_t freq_c0 = freq + offset;
  uint32_t freq_c1 = freq;

  if (freq_c1 <= 500000) {  // 500K
    rdiv = SI5351_R_DIV_64;
  } else if (freq_c1 <= 4000000) {  // 4M
    rdiv = SI5351_R_DIV_8;
  }

  if (freq_c1 > BASE_MAX)
  {
    if (freq_c1 <= BASE_MAX*3) {
      freq_c0 = freq_c0/5;    // CLK0=参考/本振
      freq_c1 = freq_c1/3;    // CLK1=发射
    } else {
      freq_c0 = freq_c0/7;    // CLK0=参考/本振
      freq_c1 = freq_c1/5;    // CLK1=发射
    }
  }

  // CLK0: frequency + offset          参考/本振
  if (freq_c0 <= 100000000) {  // [50k,100M]
    band_c0 = 0;
  } else if (freq_c0 < 150000000) {  // (100M,150M)
    band_c0 = 1;
  } else {  // [150M,BASE_MAX]
    band_c0 = 2;
  }

  // CLK1: frequency                   发射
  if (freq_c1 <= 100000000) {  // [50k,100M]
    band_c1 = 0;
  } else if (freq_c1 < 150000000) {  // (100M,150M)
    band_c1 = 1;
  } else {  // [150M,BASE_MAX]
    band_c1 = 2;
  }

#if 1
  if ((current_band_c0 != band_c0) || (current_band_c1 != band_c1))
    si5351_disable_output();
#endif

  switch (band_c0) {
  case 0:  // [50k,100M]  DIV range: 8.32~
  {
    // fractional divider mode.    MAX=832/8=104MHz
    if (current_band_c0 == 1 || current_band_c0 == 2)
      si5351_setupPLL(SI5351_PLL_A, 32, 0, 1);  // PLLA=832M
    if (current_band_c0 == 2) 
      si5351_setupPLL(SI5351_PLL_A, 32, 0, 1);

    if (rdiv == SI5351_R_DIV_8) {
      freq_c0 *= 8;
    } else if (rdiv == SI5351_R_DIV_64) {
      freq_c0 *= 64;
    }
    // PLLFREQ = 832M
    si5351_set_frequency_fixedpll(0, SI5351_PLL_A, PLLFREQ, freq_c0,
                                  rdiv, drive_strength);  // 参考/本振
  }
  break;

  case 1:  // (100M,150M)  PLL range: 600-900M PLL=23.08~34.62
  {
    // Set PLL twice on changing from band 2
    if (current_band_c0 == 2) {
      si5351_set_frequency_fixeddiv(0, SI5351_PLL_A, freq_c0, 6, drive_strength);
  }

    // div by 6 mode.
    si5351_set_frequency_fixeddiv(0, SI5351_PLL_A, freq_c0, 6, drive_strength);
  }
  break;

  case 2:  // [150M,BASE_MAX] PLL range: [150M,BASE_MAX]*4 PLL=23.08~46.15
  {
    // div by 4 mode.
    si5351_set_frequency_fixeddiv(0, SI5351_PLL_A, freq_c0, 4, drive_strength);
  }
  break;
  }

  switch (band_c1) {
  case 0:  // [50k,100M]  DIV range: 8.32~
  {
    // fractional divider mode.    MAX=832/8=104MHz
    if (current_band_c1 == 1 || current_band_c1 == 2)
        si5351_setupPLL(SI5351_PLL_B, 32, 0, 1);  // PLLA=832M
    if (current_band_c1 == 2) 
        si5351_setupPLL(SI5351_PLL_B, 32, 0, 1);

    if (rdiv == SI5351_R_DIV_8) {
        freq_c1 *= 8;
    } else if (rdiv == SI5351_R_DIV_64) {
        freq_c1 *= 64;
    }
    // PLLFREQ = 832M
    si5351_set_frequency_fixedpll(1, SI5351_PLL_B, PLLFREQ, freq_c1, rdiv, drive_strength);  // 发射
    si5351_set_frequency_fixedpll(2, SI5351_PLL_B, PLLFREQ, CLK2_FREQUENCY,
                                  SI5351_R_DIV_1, SI5351_CLK_DRIVE_STRENGTH_2MA);
  }
  break;

  case 1:  // (100M,150M)  PLL range: 600-900M PLL=23.08~34.62
  {
    // Set PLL twice on changing from band 2
    if (current_band_c1 == 2) {
        si5351_set_frequency_fixeddiv(1, SI5351_PLL_B, freq_c1, 6, drive_strength);
  }

    // div by 6 mode.
    si5351_set_frequency_fixeddiv(1, SI5351_PLL_B, freq_c1, 6, drive_strength);
    si5351_set_frequency_fixedpll(2, SI5351_PLL_B, freq_c1 * 6, CLK2_FREQUENCY,
                                  SI5351_R_DIV_1, SI5351_CLK_DRIVE_STRENGTH_2MA);
  }
  break;

  case 2:  // [150M,BASE_MAX] PLL range: [150M,BASE_MAX]*4 PLL=23.08~46.15
  {
    // div by 4 mode.
    si5351_set_frequency_fixeddiv(1, SI5351_PLL_B, freq_c1, 4, drive_strength);
    si5351_set_frequency_fixedpll(2, SI5351_PLL_B, freq_c1 * 4, CLK2_FREQUENCY,
                                  SI5351_R_DIV_1, SI5351_CLK_DRIVE_STRENGTH_2MA);
  }
  break;
  }

  if ((current_band_c0 != band_c0) || (current_band_c1 != band_c1)) {
    si5351_reset_pll();
#if 1
    si5351_enable_output();
#endif
    delay += 0;
    osDelay(30);
  }

  current_band_c0 = band_c0;
  current_band_c1 = band_c1;
  return delay;
}
