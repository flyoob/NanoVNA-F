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

#include "stm32f1xx.h"
// #error "Compiler generates FPU instructions for a device without an FPU (check __FPU_PRESENT)"

#include <arm_math.h>
#include "nanovna.h"

int16_t samp_buf[SAMPLE_LEN];  // 48
int16_t ref_buf[SAMPLE_LEN];  // 48
int16_t refiq_buf[AUDIO_BUFFER_LEN];  // 96
int16_t ref_state[STATE_LEN];  // 32

/*

傅立叶变换之后得到的每个点都是复数，如a+bi
幅度是：根号下（a^2+b^2）
相位是：arctan(b/a)
实部是：a
虚步是：b
幅度和相位结合在一起，就能完全表示傅立叶变换的结果；
实部和虚步结合在一起也能完全表示。

a+bi的表达方式确实不够友好，变换成等价的r∠θ的形式是不是就顺眼多了？
考虑最直观的状况，r就是某个物理信号的幅度，θ则是它的相位。

P1_pi=asin(10533/32767);  % P1_pi/2/pi*360 18.7507度
P2_pi=acos(31029/32767);  % P1_pi/2/pi*360 18.7449度

*/
/* 48000
const int16_t sincos_tbl[48][2] = {
  { 10533,  31029 }, { 27246,  18205 }, { 32698,  -2143 }, { 24636, -21605 },
  {  6393, -32138 }, {-14493, -29389 }, {-29389, -14493 }, {-32138,   6393 },
  {-21605,  24636 }, { -2143,  32698 }, { 18205,  27246 }, { 31029,  10533 },
  { 31029, -10533 }, { 18205, -27246 }, { -2143, -32698 }, {-21605, -24636 },
  {-32138,  -6393 }, {-29389,  14493 }, {-14493,  29389 }, {  6393,  32138 },
  { 24636,  21605 }, { 32698,   2143 }, { 27246, -18205 }, { 10533, -31029 },
  {-10533, -31029 }, {-27246, -18205 }, {-32698,   2143 }, {-24636,  21605 },
  { -6393,  32138 }, { 14493,  29389 }, { 29389,  14493 }, { 32138,  -6393 },
  { 21605, -24636 }, { 2143,  -32698 }, {-18205, -27246 }, {-31029, -10533 },
  {-31029,  10533 }, {-18205,  27246 }, {  2143,  32698 }, { 21605,  24636 },
  { 32138,   6393 }, { 29389, -14493 }, { 14493, -29389 }, { -6393, -32138 },
  {-24636, -21605 }, {-32698,  -2143 }, {-27246,  18205 }, {-10533,  31029 }
};*/
// 46875
const int16_t sincos_tbl[48][2] = {
  { 10533,  31029 }, { 27528,  17775 }, { 32614,  -3169 }, { 23591, -22742 },
  {  4362, -32476 }, {-16754, -28161 }, {-30622, -11663 }, {-31243,   9881 },
  {-18347,  27150 }, {  2485,  32674 }, { 22243,  24062 }, { 32378,   5041 },
  { 28506, -16161 }, { 12302, -30371 }, { -9224, -31443 }, {-26760, -18912 },
  {-32718,   1801 }, {-24523,  21734 }, { -5718,  32265 }, { 15560,  28838 },
  { 30107,  12935 }, { 31629,  -8564 }, { 19468, -26358 }, { -1115, -32749 },
  {-21216, -24973 }, {-32138,  -6393 }, {-29157,  14953 }, {-13563,  29829 },
  {  7900,  31802 }, { 25944,  20016 }, { 32765,   -429 }, { 25411, -20688 },
  {  7064, -31997 }, {-14339, -29464 }, {-29539, -14184 }, {-31960,   7232 },
  {-20555,  25519 }, {  -257,  32767 }, { 20151,  25839 }, { 31842,   7733 },
  { 29758, -13719 }, { 14800, -29235 }, { -6561, -32104 }, {-25083, -21085 },
  {-32754,   -944 }, {-26255,  19606 }, { -8398,  31674 }, { 13092,  30039 }
};

int32_t acc_samp_s;
int32_t acc_samp_c;
int32_t acc_ref_s;
int32_t acc_ref_c;

void
dsp_process(int16_t *capture, size_t length)  // length = 96
{
  uint32_t *p = (uint32_t*)capture;
  uint32_t len = length / 2;
  uint32_t i;
  int32_t samp_s = 0;
  int32_t samp_c = 0;
  int32_t ref_s = 0;
  int32_t ref_c = 0;

  for (i = 0; i < len; i++) {  // 48次
    uint32_t sr = *p++;
    int16_t ref = sr & 0xffff;  // 左声道
    int16_t smp = (sr>>16) & 0xffff;  // 右声道
    ref_buf[i] = ref;
    samp_buf[i] = smp;
    int32_t s = sincos_tbl[i][0];
    int32_t c = sincos_tbl[i][1];
    samp_s += smp * s / 16;
    samp_c += smp * c / 16;
    ref_s += ref * s / 16;
    ref_c += ref * c / 16;
#if 0
    uint32_t sc = *(uint32_t)&sincos_tbl[i];
    samp_s = __SMLABB(sr, sc, samp_s);
    samp_c = __SMLABT(sr, sc, samp_c);
    ref_s = __SMLATB(sr, sc, ref_s);
    ref_c = __SMLATT(sr, sc, ref_c);
#endif
  }
  acc_samp_s = samp_s;  // Accumulate 累加 I路
  acc_samp_c = samp_c;  // Accumulate 累加 Q路
  acc_ref_s = ref_s;
  acc_ref_c = ref_c;
}

// Gamma源于CRT(显示器/电视机)的响应曲线,即其亮度与输入电压的非线性关系
void
calculate_gamma(float gamma[2])  // gamma 就是系数的意思
{
  float rs = acc_ref_s;  // 参考 sin
  float rc = acc_ref_c;  // 参考 cos
  float rr = rs * rs + rc * rc;
  //rr = sqrtf(rr) * 1e8;
  float ss = acc_samp_s;  // 信号 sin
  float sc = acc_samp_c;  // 信号 cos
  gamma[0] =  (sc * rc + ss * rs) / rr;  // 实部？
  gamma[1] =  (ss * rc - sc * rs) / rr;  // 虚部？
}

void
reset_dsp_accumerator(void)
{
  acc_ref_s = 0;
  acc_ref_c = 0;
  acc_samp_s = 0;
  acc_samp_c = 0;
}
