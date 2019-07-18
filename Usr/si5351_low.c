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

// register addr, length, data, ...
const uint8_t si5351_configs_low[] = {
  2, SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0xff,
  4, SI5351_REG_16_CLK0_CONTROL, SI5351_CLK_POWERDOWN, SI5351_CLK_POWERDOWN, SI5351_CLK_POWERDOWN,
  2, SI5351_REG_183_CRYSTAL_LOAD, SI5351_CRYSTAL_LOAD_8PF,
  // setup PLL (26MHz * 32 = 832MHz : 32/2-2=14)
  9, SI5351_REG_26_PLL_A, /*P3*/0, 1, /*P1*/0, 14, 0, /*P3/P2*/0, 0, 0,
  // RESET PLL
  2, SI5351_REG_177_PLL_RESET, SI5351_PLL_RESET_A | SI5351_PLL_RESET_B,
  // setup multisynth (832MHz/8MHz=104,104/2-2=50)
  9, SI5351_REG_58_MULTISYNTH2, /*P3*/0, 1, /*P1*/0, 50, 0, /*P2|P3*/0, 0, 0,
  2, SI5351_REG_18_CLK2_CONTROL, SI5351_CLK_DRIVE_STRENGTH_2MA | SI5351_CLK_INPUT_MULTISYNTH_N | SI5351_CLK_INTEGER_MODE,
  2, SI5351_REG_3_OUTPUT_ENABLE_CONTROL, 0,
  0 // sentinel
};

void
si5351_init_bulk(void)
{
  const uint8_t *p = si5351_configs_low;
  while (*p) {
    uint8_t len = *p++;
    // i2cSendByte(I2C1, SI5351_I2C_ADDR, p, len);
    p += len;
  }
}

void
si5351_setup(void)
{
  // rcc_gpio_init();
  // i2c_init(I2C1);
  si5351_init_bulk();
}
