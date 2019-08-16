/*-----------------------------------------------------------------------------/
 * Module       : appvna.c
 * Create       : 2019-05-24
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 
/-----------------------------------------------------------------------------*/
#include "system.h"
#include "board.h"
#include "si5351.h"
#include "nanovna.h"
#include "usbd_cdc_if.h"
#include "nt35510.h"
#include "touch_ctp.h"
#include "touch_rtp.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"

#include "fatfs.h"
#include "fs_funs.h"

#include <stdio.h>
#include <math.h>
#include <ctype.h>

extern osThreadId Task001Handle;

extern int g_HDStatus;
extern I2S_HandleTypeDef hi2s2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern void vUARTCommandConsoleStart( uint16_t usStackSize, UBaseType_t uxPriority );
extern void touch_position(int *x, int *y);

#define BaseSequentialStream char

int16_t rx_copy[AUDIO_BUFFER_LEN];
int32_t acquire = 0;

#define ENABLED_DUMP

// static void apply_error_term(void);
static void apply_error_term_at(int i);
static void apply_edelay_at(int i);
static void cal_interpolate(int s);

void sweep(void);

static SemaphoreHandle_t mutex = NULL;
/*
 * @param xBlockTime The time in ticks to wait for the semaphore to become
 * available.  The macro portTICK_PERIOD_MS can be used to convert this to a
 * real time.  A block time of zero can be used to poll the semaphore.  A block
 * time of portMAX_DELAY can be used to block indefinitely (provided
 * INCLUDE_vTaskSuspend is set to 1 in FreeRTOSConfig.h).
 */
// #define chMtxLock(a)    xSemaphoreTake(mutex, portMAX_DELAY)
// #define chMtxUnlock(a)  xSemaphoreGive(mutex)

#define chMtxLock(a)    osRecursiveMutexWait(mutex, osWaitForever)
#define chMtxUnlock(a)  osRecursiveMutexRelease(mutex)

int32_t frequency_offset = 5000;
int32_t frequency = 10000000;
uint8_t drive_strength = SI5351_CLK_DRIVE_STRENGTH_2MA;
int8_t frequency_updated = FALSE;
int8_t sweep_enabled = TRUE;
int8_t cal_auto_interpolate = TRUE;
int8_t redraw_requested = FALSE;

/* 文件读写测试 */
#define  FILE_SIZE   (4*1024)
uint8_t  file_buf[FILE_SIZE];

void bat_adc_display(void);

/*
=======================================
    APP 死循环
=======================================
*/
void app_loop(void)
{
  while (1)
  {
    if (sweep_enabled)
    {
      chMtxLock(&mutex);
      sweep();
      chMtxUnlock(&mutex);
    } else {
      ui_process();
    }
    /* calculate trace coordinates：坐标 */
    plot_into_index(measured); // 标记要画的点
    /* plot trace as raster */
    draw_all_cells();

    bat_adc_display();
  }
}

void pause_sweep(void)
{
  sweep_enabled = FALSE;
}

void resume_sweep(void)
{
  sweep_enabled = TRUE;
}

void toggle_sweep(void)
{
  sweep_enabled = !sweep_enabled;
}

/*
=======================================
    扫频暂停
=======================================
*/
static void cmd_pause(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  pause_sweep();
}
static const CLI_Command_Definition_t x_cmd_pause = {
"pause", "usage: pause\r\n", (shellcmd_t)cmd_pause, 0};

/*
=======================================
    扫频恢复
=======================================
*/
static void cmd_resume(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;
  resume_sweep();
}
static const CLI_Command_Definition_t x_cmd_resume = {
"resume", "usage: resume\r\n", (shellcmd_t)cmd_resume, 0};

/*
=======================================
    系统复位
=======================================
*/
static void cmd_reset(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;

  chprintf(chp, "Performing reset\r\n");
  osDelay(200);
  HAL_NVIC_SystemReset();

  while(1);
}
static const CLI_Command_Definition_t x_cmd_reset = {
"reset", "usage: reset\r\n", (shellcmd_t)cmd_reset, 0};

/*
=======================================
    设置频率
=======================================
*/
int set_frequency(int freq)
{
  int delay = 0;
  if (frequency != freq) {
    if (freq <= BASE_MAX) {
      drive_strength = SI5351_CLK_DRIVE_STRENGTH_2MA;
    } else {
      drive_strength = SI5351_CLK_DRIVE_STRENGTH_8MA;
    }
    delay = si5351_set_frequency_with_offset_expand(freq, frequency_offset, drive_strength);
    frequency = freq;
  }
  return delay;
}

/*
=======================================
    命令：设置频差
=======================================
*/
static void cmd_offset(BaseSequentialStream *chp, int argc, char *argv[])
{
  if (argc != 1) {
    chprintf(chp, "usage: offset {frequency offset(Hz)}\r\n");
    return;
  }
  frequency_offset = atoi(argv[0]);
  set_frequency(frequency);
}
static const CLI_Command_Definition_t x_cmd_offset = {
"offset", "usage: offset {frequency offset(Hz)}\r\n", (shellcmd_t)cmd_offset, -1};

/*
=======================================
    命令：设置频率
=======================================
*/
static void cmd_freq(BaseSequentialStream *chp, int argc, char *argv[])
{
  int freq;
  if (argc != 1) {
    chprintf(chp, "usage: freq {frequency(Hz)}\r\n");
    return;
  }
  pause_sweep();
  chMtxLock(&mutex);
  freq = atoi(argv[0]);
  set_frequency(freq);
  chMtxUnlock(&mutex);
}
static const CLI_Command_Definition_t x_cmd_freq = {
"freq", "usage: freq {frequency(Hz)}\r\n", (shellcmd_t)cmd_freq, -1};

/*
=======================================
    命令：设置功率
=======================================
*/
static void cmd_power(BaseSequentialStream *chp, int argc, char *argv[])
{
  if (argc != 1) {
    chprintf(chp, "usage: power {0-3}\r\n");
    return;
  }
  drive_strength = atoi(argv[0]);
  set_frequency(frequency);
}
static const CLI_Command_Definition_t x_cmd_power = {
"power", "usage: power {0-3}\r\n", (shellcmd_t)cmd_power, -1};

/*
=======================================
    命令：获取时间
=======================================
*/
static void cmd_time(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  // get time
  // chprintf(chp, "%d/%d/%d %d\r\n", timespec.year+1980, timespec.month, timespec.day, timespec.millisecond);
}
static const CLI_Command_Definition_t x_cmd_time = {
"time", "usage: time\r\n", (shellcmd_t)cmd_time, -1};

/*
=======================================
    命令：设置 DAC
=======================================
*/
static void cmd_dac(BaseSequentialStream *chp, int argc, char *argv[])
{
  int value;
  if (argc != 1) {
    chprintf(chp, "usage: dac {value(0-4095)}\r\n");
    chprintf(chp, "current value: %d\r\n", config.dac_value);
    return;
  }
  value = atoi(argv[0]);
  config.dac_value = value;
  // dacPutChannelX(&DACD2, 0, value);
}
static const CLI_Command_Definition_t x_cmd_dac = {
"dac", "usage: dac {value(0-4095)}\r\n", (shellcmd_t)cmd_dac, -1};

/*
=======================================
    命令：保存配置
=======================================
*/
static void cmd_saveconfig(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  config_save();
  chprintf(chp, "Config saved.\r\n");
}
static const CLI_Command_Definition_t x_cmd_saveconfig = {
"saveconfig", "usage: saveconfig\r\n", (shellcmd_t)cmd_saveconfig, 0};

/*
=======================================
    命令：清除配置
=======================================
*/
static void cmd_clearconfig(BaseSequentialStream *chp, int argc, char *argv[])
{
  if (argc != 1) {
    chprintf(chp, "usage: clearconfig {protection key}\r\n");
  return;
  }

  if (strcmp(argv[0], "1234") != 0) {
    chprintf(chp, "Key unmatched.\r\n");
    return;
  }

  // clear_all_config_prop_data();
  chprintf(chp, "Config and all cal data cleared.\r\n");
}
static const CLI_Command_Definition_t x_cmd_clearconfig = {
"clearconfig", "usage: clearconfig {protection key}\r\n", (shellcmd_t)cmd_clearconfig, -1};

/*
=======================================
    有效值、平均值
=======================================
*/
static struct {
  int16_t ave[2];
  int16_t min[2];
  int16_t max[2];
  int16_t rms[2];
  int callback_count;

#if 0
  int32_t last_counter_value;
  int32_t interval_cycles;
  int32_t busy_cycles;
#endif
} stat;

int16_t rx_buffer[AUDIO_BUFFER_LEN * 2];

#ifdef ENABLED_DUMP
int16_t dump_buffer[AUDIO_BUFFER_LEN];
int16_t dump_selection = 0;
#endif

volatile int16_t wait_count = 0;

float measured[2][SWEEP_POINTS][2];

/*
=======================================
    等待几个 I2S 中断即等待几包数据后再处理
=======================================
*/
static void wait_dsp(int count)
{
  wait_count = count;
  while (wait_count);
}

#ifdef ENABLED_DUMP
static void duplicate_buffer_to_dump(int16_t *p)  // duplicate : 重复
{
  if (dump_selection != 0) {
    memcpy(dump_buffer, p, AUDIO_BUFFER_LEN*2);
    __NOP();
  }
}
#endif

void i2s_end_callback(size_t offset, size_t n)
{
#if PORT_SUPPORTS_RT
  int32_t cnt_s = port_rt_get_counter_value();
  int32_t cnt_e;
#endif
  int16_t *p = &rx_buffer[offset];
  (void)n;

  if (wait_count > 0)
  {
    if (wait_count == 1)
    {
      dsp_process(p, n);
#ifdef ENABLED_DUMP
      duplicate_buffer_to_dump(p);
#endif
    }
    -- wait_count;
  }

#if PORT_SUPPORTS_RT
  cnt_e = port_rt_get_counter_value();
  stat.interval_cycles = cnt_s - stat.last_counter_value;
  stat.busy_cycles = cnt_e - cnt_s;
  stat.last_counter_value = cnt_s;
#endif
  stat.callback_count ++;
}

/*
=======================================
    I2S 半中断
=======================================
*/
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s2) {
    // LED1_ON;
    #if 0
    if (acquire == 1) {
      memcpy(rx_copy, rx_buffer, AUDIO_BUFFER_LEN);
      acquire = 2;
    }
    #endif
    i2s_end_callback(0, AUDIO_BUFFER_LEN);
  }
}

/*
=======================================
    I2S 全中断
=======================================
*/
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (hi2s == &hi2s2) {
    // LED1_OFF;
    #if 0
    if (acquire == 2) {
      memcpy(rx_copy+AUDIO_BUFFER_LEN/2, rx_buffer+AUDIO_BUFFER_LEN/2, AUDIO_BUFFER_LEN);
      acquire = 0;
    }
    #endif
    i2s_end_callback(AUDIO_BUFFER_LEN, AUDIO_BUFFER_LEN);
  }
}

/*
=======================================
    命令：获取数据
    data 0-1 上传测量的数据
    data 2-7 上传校准的数据
    0 反射
    1 传输
    2 LOAD  ED
    3 OPEN  RS
    4 SHORT ER
    5 THRU  ET
    6 ISOLN EX
=======================================
*/
static void cmd_data(BaseSequentialStream *chp, int argc, char *argv[])
{
  int i;
  int sel = 0;

  if (argc == 1)
    sel = atoi(argv[0]);

  if (sel == 0 || sel == 1) {
    chMtxLock(&mutex);
    for (i = 0; i < sweep_points; i++) {
      chprintf(chp, "%f %f\r\n", measured[sel][i][0], measured[sel][i][1]);
    }
    chMtxUnlock(&mutex);
  } else if (sel >= 2 && sel < 7) {
    chMtxLock(&mutex);
    for (i = 0; i < sweep_points; i++) {
      chprintf(chp, "%f %f\r\n", cal_data[sel-2][i][0], cal_data[sel-2][i][1]);
    }
    chMtxUnlock(&mutex);
  } else {
    chprintf(chp, "usage: data [array]\r\n");
  }
}
static const CLI_Command_Definition_t x_cmd_data = {
"data", "usage: data [array]\r\n", (shellcmd_t)cmd_data, -1};

#ifdef ENABLED_DUMP
static void cmd_dump(BaseSequentialStream *chp, int argc, char *argv[])
{
  int i, j;
  int16_t *p = &dump_buffer[0];
  int64_t acc0, acc1;
  int32_t ave0, ave1;
  int32_t count = AUDIO_BUFFER_LEN/2;
  int16_t min0, min1;
  int16_t max0, max1;

  if (argc == 1)
    dump_selection = atoi(argv[0]);

  wait_dsp(4);

  dump_selection = 0;

  /* 反射或传输 */
  for (i = 0; i < AUDIO_BUFFER_LEN/2; ) {
    for (j = 0; j < 16; j++, i++) {
      chprintf(chp, "%6d ", p[i*2+1]);
    }
    chprintf(chp, "\r\n");
  }
  /* 参考 */
  for (i = 0; i < AUDIO_BUFFER_LEN/2; ) {
    for (j = 0; j < 16; j++, i++) {
      chprintf(chp, "%6d ", p[i*2]);
    }
    chprintf(chp, "\r\n");
  }

    /* 求均值，极值 */
  acc0 = 0; acc1 = 0;
  min0 = 0; min1 = 0;
  max0 = 0; max1 = 0;
  for (i = 0; i < AUDIO_BUFFER_LEN/2; i ++) {
    acc0 += p[i*2+1];
    if (p[i*2+1] < min0) min0 = p[i*2+1];
    if (p[i*2+1] > max0) max0 = p[i*2+1];
    acc1 += p[i*2];
    if (p[i*2] < min1) min1 = p[i*2];
    if (p[i*2] > max1) max1 = p[i*2];
  }
  ave0 = acc0 / count;
  ave1 = acc1 / count;

  /* 求 RMS */
  acc0 = 0;
  acc1 = 0;
  for (i = 0; i < AUDIO_BUFFER_LEN/2; i ++) {
    acc0 += (int)(p[i*2+1] - ave0)*(int)(p[i*2+1] - ave0);
    acc1 += (int)(p[i*2]   - ave1)*(int)(p[i*2]   - ave1);
  }
  stat.ave[0] = ave0; stat.ave[1] = ave1;
  stat.min[0] = min0; stat.min[1] = min1;
  stat.max[0] = max0; stat.max[1] = max1;
  stat.rms[0] = (int16_t)sqrtf((float)(acc0 / count));
  stat.rms[1] = (int16_t)sqrtf((float)(acc1 / count));

  chprintf(chp, "ave: %6d %6d\r\n", stat.ave[0], stat.ave[1]);  // 反射或传输，参考
  chprintf(chp, "min: %6d %6d\r\n", stat.min[0], stat.min[1]);  // 反射或传输，参考
  chprintf(chp, "max: %6d %6d\r\n", stat.max[0], stat.max[1]);  // 反射或传输，参考
  chprintf(chp, "rms: %6d %6d\r\n", stat.rms[0], stat.rms[1]);  // 反射或传输，参考
  // chprintf(chp, "callback count: %d\r\n", stat.callback_count);
  // chprintf(chp, "interval cycle: %d\r\n", stat.interval_cycles);
  // chprintf(chp, "busy cycle: %d\r\n", stat.busy_cycles);
  // chprintf(chp, "load: %d\r\n", stat.busy_cycles * 100 / stat.interval_cycles);
  // extern int awd_count;
  // chprintf(chp, "awd: %d\r\n", awd_count);
}
static const CLI_Command_Definition_t x_cmd_dump = {
"dump", "usage: dump 1\r\n", (shellcmd_t)cmd_dump, -1};
#endif

#if 1
static void cmd_gamma(BaseSequentialStream *chp, int argc, char *argv[])
{
  float gamma[2];
  (void)argc;
  (void)argv;

  pause_sweep();
  chMtxLock(&mutex);
  wait_dsp(4);
  calculate_gamma(gamma);
  chMtxUnlock(&mutex);

  chprintf(chp, "%f %f\r\n", gamma[0], gamma[1]);
}
static const CLI_Command_Definition_t x_cmd_gamma = {
"gamma", "usage: gamma\r\n", (shellcmd_t)cmd_gamma, -1};
#endif

#if 0
int32_t frequency0 = 1000000;
int32_t frequency1 = BASE_MAX;
int16_t sweep_points = SWEEP_POINTS;

uint32_t frequencies[SWEEP_POINTS];
uint16_t cal_status;
float cal_data[5][SWEEP_POINTS][2];
#endif

config_t config = {  // 默认配置
/* magic */   CONFIG_MAGIC,
/* dac_value */ 1922,
/* grid_color */ BRG556(128,128,128),
/* menu_normal_color */ 0xffff,
/* menu_active_color */ 0x7777,
// S11-LOGMAG S21-LOGMAG S11-SMITH S21-PHASE 黄 蓝 绿 紫
/* trace_colors[4] */ { BRG556(0,255,255), BRG556(255,0,255), BRG556(0,0,255), BRG556(233,233,0) },
/* touch_cal[4] */ { 440, 656, 158, 259 },
/* default_loadcal */    0,
/* language */ LANG_CN,
/* checksum */           0
};

properties_t current_props = {  // 默认属性
/* magic */   CONFIG_MAGIC,
/* frequency0 */     50000, // start = 50kHz
/* frequency1 */  STOP_MAX, // end
/* sweep_points */     SWEEP_POINTS,
/* cal_status */         0,
/* frequencies */       {0},
/* cal_data */          {0},
/* electrical_delay */   0,
/* trace[4] */
{/*enable, type, channel, polar, scale*/
  { 1, TRC_LOGMAG, 0, 0, 1.0, 7.0 },
  { 1, TRC_LOGMAG, 1, 0, 1.0, 7.0 },
  { 1, TRC_SMITH,  0, 1, 1.0, 0.0 },
  { 0, TRC_PHASE,  0, 0, 1.0, 4.0 }
},
/* markers[4] */ {
  { 1, 30, 0 }, { 0, 40, 0 }, { 0, 60, 0 }, { 0, 80, 0 }
},
/* active_marker */      0,
/* checksum */           0
};
properties_t *active_props = &current_props;

/*
=======================================
    切换到指向内存的参数
=======================================
*/
void ensure_edit_config(void)
{
  if (active_props == &current_props)
    return;

  //memcpy(&current_props, active_props, sizeof(config_t));
  active_props = &current_props;
  // move to uncal state
  cal_status = 0;
}

#if 0
static void cmd_scan(BaseSequentialStream *chp, int argc, char *argv[])
{
  float gamma[2];
  int i;
  int32_t freq, step;
  int delay;
  (void)argc;
  (void)argv;

  pause_sweep();
  chMtxLock(&mutex);
  freq = frequency0;
  step = (frequency1 - frequency0) / (sweep_points-1);
  set_frequency(freq);
  delay = 4;
  for (i = 0; i < sweep_points; i++) {
    freq = freq + step;
    wait_dsp(delay);
    delay = set_frequency(freq);
    palClearPad(GPIOC, GPIOC_LED);
    calculate_gamma(gamma);
    palSetPad(GPIOC, GPIOC_LED);
    chprintf(chp, "%d %d\r\n", gamma[0], gamma[1]);
  }
  chMtxUnlock(&mutex);
}
#endif

// main loop for measurement
void sweep(void)
{
  int i;
  int delay1, delay2;

rewind:
  frequency_updated = FALSE;
  delay1 = 3;
  delay2 = 5;

  LED1_ON;

  for (i = 0; i < sweep_points; i++)  // SWEEP_POINTS
  {
    set_frequency(frequencies[i]);
    if (frequencies[i] > BASE_MAX*4) {
      tlv320aic3204_set_gain(72, 92);
    } else if (frequencies[i] > BASE_MAX*3) {
      tlv320aic3204_set_gain(68, 82);
    } else if (frequencies[i] > BASE_MAX*2) {
      tlv320aic3204_set_gain(48, 58);
    } else if (frequencies[i] > BASE_MAX) {
      tlv320aic3204_set_gain(40, 50);
    } else {
      tlv320aic3204_set_gain(0, 10);
    }

    tlv320aic3204_select_in3(); // S11:REFLECT
    wait_dsp(delay1);  // 扔掉两块数据

    /* calculate reflection coeficient 计算反射系数 */
    calculate_gamma(measured[0][i]);
    // dbprintf("%5d %5d\r\n", acc_samp_s, acc_samp_c);

    tlv320aic3204_select_in1(); // S21:TRANSMISSION
    wait_dsp(delay2);  // 扔掉两块数据

    /* calculate transmission coeficient 计算传输系数 */
    calculate_gamma(measured[1][i]);

    // 应用校准数据
    if (cal_status & CALSTAT_APPLY)
      apply_error_term_at(i);  // 校准 error term 误差项 ED ES ER ET EX

    if (electrical_delay != 0)
      apply_edelay_at(i);  // 校准电延时

    redraw_requested = FALSE;
    // request_to_draw_cells_behind_menu
    // request_to_draw_cells_behind_numeric_input
    ui_process();
    if (redraw_requested)  // 重画（redraw）
      return; // return to redraw screen asap. 尽快重新绘制屏幕

    if (frequency_updated)  // 修改了扫频参数，重新开始扫频
      goto rewind;
  }
  /*
  set_frequency(frequencies[0]);
  if (frequencies[i] > BASE_MAX*4) {
    tlv320aic3204_set_gain(72, 92);
  } else if (frequencies[i] > BASE_MAX*3) {
    tlv320aic3204_set_gain(68, 82);
  } else if (frequencies[i] > BASE_MAX*2) {
    tlv320aic3204_set_gain(48, 58);
  } else if (frequencies[i] > BASE_MAX) {
    tlv320aic3204_set_gain(40, 50);
  } else {
    tlv320aic3204_set_gain(0, 10);
  } */

  LED1_OFF;

  // if (cal_status & CALSTAT_APPLY)
      // apply_error_term();
}

/*
=======================================
    更新 Mark 点位置
=======================================
*/
static void update_marker_index(void)
{
  int m;
  int i;
  for (m = 0; m < 4; m++) {
    if (!markers[m].enabled)
      continue;
      uint32_t f = markers[m].frequency;
      if (f < frequencies[0]) {
        markers[m].index = 0;
        markers[m].frequency = frequencies[0];
      } else if (f >= frequencies[sweep_points-1]) {
        markers[m].index = sweep_points-1;
        markers[m].frequency = frequencies[sweep_points-1];
      } else {
      for (i = 0; i < sweep_points-1; i++) {
        if (frequencies[i] <= f && f < frequencies[i+1]) {
          uint32_t mid = (frequencies[i] + frequencies[i+1])/2;
          if (f < mid) {
            markers[m].index = i;
          } else {
            markers[m].index = i + 1;
          }
          break;
        }
      }
    }
  }
}

/*
=======================================
    更新起止频率信息
=======================================
*/
void update_frequencies(void)
{
  int i;
  int32_t span;
  int32_t start;
  if (frequency1 > 0) {
    start = frequency0;
    span = (frequency1 - frequency0)/100;
  } else {
    int center = frequency0;
    span = -frequency1;
    start = center - span/2;
    span /= 100;
  }

  for (i = 0; i < sweep_points; i++)
    frequencies[i] = start + span * i / (sweep_points - 1) * 100;

  if (cal_auto_interpolate) // 计算插值？
    cal_interpolate(0);

  update_marker_index(); // 更新 Mark 点位置

  frequency_updated = TRUE;
  // set grid layout
  update_grid();
}

void freq_mode_startstop(void)
{
  if (frequency1 <= 0) {
    int center = frequency0;
    int span = -frequency1;
    ensure_edit_config();
    frequency0 = center - span/2;
    frequency1 = center + span/2;
  }
}

void freq_mode_centerspan(void)
{
  if (frequency1 > 0) {
    int start = frequency0;
    int stop = frequency1;
    ensure_edit_config();
    frequency0 = (start + stop)/2; // center
    frequency1 = -(stop - start); // span
  }
}

void set_sweep_frequency(int type, int frequency)
{
  int32_t freq = frequency;
  switch (type) {
  case ST_START:
    freq_mode_startstop();
    if (frequency < START_MIN)
      freq = START_MIN;
    if (frequency > STOP_MAX)
      freq = STOP_MAX;
    if (frequency0 != freq) {
      ensure_edit_config();
      frequency0 = freq;
      // if start > stop then make start = stop
      if (frequency1 < freq)
        frequency1 = freq;
      update_frequencies();
    }
    break;
  case ST_STOP:
    freq_mode_startstop();
    if (frequency > STOP_MAX)
      freq = STOP_MAX;
    if (frequency < START_MIN)
      freq = START_MIN;
    if (frequency1 != freq) {
      ensure_edit_config();
      frequency1 = freq;
      // if start > stop then make start = stop
      if (frequency0 > freq)
        frequency0 = freq;
      update_frequencies();
    }
    break;
  case ST_CENTER:
    ensure_edit_config();
    freq_mode_centerspan();
    if (frequency0 != freq) {
      ensure_edit_config();
      frequency0 = freq;
      int center = frequency0;
      int span = -frequency1;
      if (center-span/2 < START_MIN) {
        span = (center - START_MIN) * 2;
        frequency1 = -span;
      }
      if (center+span/2 > STOP_MAX) {
        span = (STOP_MAX - center) * 2;
        frequency1 = -span;
      }
      update_frequencies();
    }
    break;
  case ST_SPAN:
    freq_mode_centerspan();
    if (frequency1 != -freq) {
      ensure_edit_config();
      frequency1 = -freq;
      int center = frequency0;
      int span = -frequency1;
      if (center-span/2 < START_MIN) {
        center = START_MIN + span/2;
        frequency0 = center;
      }
      if (center+span/2 > STOP_MAX) {
        center = STOP_MAX - span/2;
        frequency0 = center;
      }
      update_frequencies();
    }
    break;
  case ST_CW:
    freq_mode_centerspan();
    if (frequency > STOP_MAX)
      freq = STOP_MAX;
    if (frequency < START_MIN)
      freq = START_MIN;
    if (frequency0 != freq || frequency1 != 0) {
      ensure_edit_config();
      frequency0 = freq;
      frequency1 = 0;
      update_frequencies();
    }
    break;
  }
}

uint32_t get_sweep_frequency(int type)
{
  if (frequency1 >= 0) {
    switch (type) {
    case ST_START: return frequency0;
    case ST_STOP: return frequency1;
    case ST_CENTER: return (frequency0 + frequency1)/2;
    case ST_SPAN: return frequency1 - frequency0;
    case ST_CW: return (frequency0 + frequency1)/2;
    }
  } else {
    switch (type) {
    case ST_START: return frequency0 + frequency1/2;
    case ST_STOP: return frequency0 - frequency1/2;
    case ST_CENTER: return frequency0;
    case ST_SPAN: return -frequency1;
    case ST_CW: return frequency0;
    }
  }
  return 0;
}

/*
=======================================
    命令：扫频
=======================================
*/
static void cmd_sweep(BaseSequentialStream *chp, int argc, char *argv[])
{
  if (argc == 0) {
    chprintf(chp, "%d %d %d\r\n", frequency0, frequency1, sweep_points);
    return;
  } else if (argc > 3) {
    chprintf(chp, "usage: sweep {start(Hz)} [stop] [points]\r\n");
    return;
  }
  if (argc >= 2) {
    if (strcmp(argv[0], "start") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_START, value);
      return;
    } else if (strcmp(argv[0], "stop") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_STOP, value);
      return;
    } else if (strcmp(argv[0], "center") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_CENTER, value);
      return;
    } else if (strcmp(argv[0], "span") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_SPAN, value);
      return;
    } else if (strcmp(argv[0], "cw") == 0) {
      int32_t value = atoi(argv[1]);
      set_sweep_frequency(ST_CW, value);
      return;
    }
  }

  if (argc >= 1) {
    int32_t value = atoi(argv[0]);
    set_sweep_frequency(ST_START, value);
  }
  if (argc >= 2) {
    int32_t value = atoi(argv[1]);
    set_sweep_frequency(ST_STOP, value);
  }
}
static const CLI_Command_Definition_t x_cmd_sweep = {
"sweep", "usage: sweep {start(Hz)} [stop] [points]\r\n", (shellcmd_t)cmd_sweep, -1};

/*
=======================================
    命令：查看文件
=======================================
*/
#if 0
static void cmd_list(BaseSequentialStream *chp, int argc, char *argv[])
{
  int disk_total, disk_free;

  (void)chp;
  (void)argc;
  (void)argv;

  if (g_HDStatus != FR_OK) {
      MX_FATFS_Init();
      dbprintf("MX_FATFS_Init !\r\n");
  }

  if (g_HDStatus == FR_OK) {
      get_free((const TCHAR *)SDPath, &disk_total, &disk_free);
      dbprintf("%s total: %6d KB, free: %6d KB\r\n", SDPath, disk_total, disk_free);
      scan_files((const TCHAR *)SDPath);
  } else {
      dbprintf("No SD card or err occur %d\r\n", g_HDStatus);
  }
}
static const CLI_Command_Definition_t x_cmd_list = {
"list", "list the SD Files\r\n", (shellcmd_t)cmd_list, 0};
#endif

/*
=======================================
    命令：存取文件
=======================================
*/
#if 0
static void cmd_fatfs(BaseSequentialStream *chp, int argc, char *argv[])
{
  FIL  fnew;
  UINT f_res, bw, br, inter = 0;

  (void)chp;

  if (argc < 1) {
    dbprintf("No filename\r\n");
    return;
  }

  if (g_HDStatus != FR_OK) {
    MX_FATFS_Init();
    dbprintf("MX_FATFS_Init !\r\n");
  }

  if (g_HDStatus != FR_OK) {
    dbprintf("No SD card or err occur %d\r\n", g_HDStatus);
    return;
  }

  dbprintf("\r\n");
  f_res = f_open(&fnew, argv[0], FA_CREATE_ALWAYS|FA_WRITE|FA_READ);
  if (f_res == FR_OK) {
    dbprintf("cr ok\r\n");
  }

  f_res = f_write(&fnew, file_buf, FILE_SIZE, &bw);
  f_sync(&fnew);

  dbprintf("wr %8u bytes, %8u us  ", bw, (uint32_t)inter);
  // dbprintf("wr speed %8u KB/s\r\n", (uint32_t)get_speed(bw, inter, 456));
  if ((f_res == FR_OK) && (bw == FILE_SIZE)) {
    dbprintf("wr ok\r\n");
  } else {
    dbprintf("wr er, %d, %d\r\n", f_res, bw);
  }

  f_lseek(&fnew, 0);

  f_res = f_read(&fnew, file_buf, FILE_SIZE, &br);

  dbprintf("rd %8u bytes, %8u us  ", br, (uint32_t)inter);
  // dbprintf("rd speed %8u KB/s\r\n", (uint32_t)get_speed(br, inter, 456));
  if ((f_res == FR_OK) && (br == FILE_SIZE)) {
    dbprintf("rd ok\r\n");
  } else {
    dbprintf("rd er, %d, %d\r\n", f_res, br);
  }
  f_close(&fnew);
}
static const CLI_Command_Definition_t x_cmd_fatfs = {
"fatfs", "fatfs r/w test\r\n", (shellcmd_t)cmd_fatfs, -1};
#endif

/*
=======================================
    命令：设置背光
=======================================
*/
static void cmd_pwm(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;

  float duty;
  if (argc != 1) {
    dbprintf("usage: pwm {0.0-1.0}\r\n");
    return;
  }
  duty = (float)atof(argv[0]);
  if (duty < 0)
    duty = 0;
  if (duty > 1)
    duty = 1;
  // __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 1999);  // 占空比 (2ms-1us)/2ms
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, (uint16_t)(2000*duty));
}
static const CLI_Command_Definition_t x_cmd_pwm = {
"pwm", "usage: pwm {0.0-1.0}\r\n", (shellcmd_t)cmd_pwm, -1};

/*
=======================================
    命令：测试BEEP
=======================================
*/
static void cmd_beep(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;

  if (argc != 1) {
    dbprintf("usage: beep on/off\r\n");
    return;
  }
  if (strcmp(argv[0], "on") == 0) {
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint16_t)(185));
    return;
  }
  if (strcmp(argv[0], "off") == 0) {
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
    return;
  }
  dbprintf("usage: beep on/off\r\n");
}
static const CLI_Command_Definition_t x_cmd_beep = {
"beep", "usage: beep on/off\r\n", (shellcmd_t)cmd_beep, -1};

/*
=======================================
    命令：LCD 测试
=======================================
*/
static void cmd_lcd(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;

  uint16_t x, y, w, h, color;
  uint8_t val[2];

  if (argc != 5) {
    goto usage;
  }

  x = atoi(argv[0]);
  if (x > LCD_WIDTH-1) {
    x = LCD_WIDTH-1;
  }

  y = atoi(argv[1]);
  if (y > LCD_HEIGHT-1) {
    y = LCD_HEIGHT-1;
  }

  w = atoi(argv[2]);
  if ((x+w) > LCD_WIDTH) {
    w = LCD_WIDTH-x;
  }

  h = atoi(argv[3]);
  if ((y+h) > LCD_HEIGHT) {
    h = LCD_HEIGHT-y;
  }

  if (strlen(argv[4]) == 4) {
    str2hex(val, argv[4]);
    color = val[0]*256 + val[1];
    // nt35510_fill(0, 0, 800, 480, BLACK);
    nt35510_fill_x2(x, y, w, h, color);
    return;
  }

usage:
  dbprintf("usage: lcd X Y WIDTH HEIGHT FFFF\r\n");
}
static const CLI_Command_Definition_t x_cmd_lcd = {
"lcd", "usage: lcd X Y WIDTH HEIGHT FFFF\r\n", (shellcmd_t)cmd_lcd, -1};

/*
=======================================
    命令：任务状态
=======================================
*/
static void cmd_task(BaseSequentialStream *chp, int argc, char *argv[])
{
  const char *const pcHeader = "\
             State   Priority  Stack    #\r\n\
************************************************\r\n";
  BaseType_t xSpace;

  /* Remove compile time warnings about unused parameters, and check the
  write buffer is not NULL.  NOTE - for simplicity, this example assumes the
  write buffer length is adequate, so does not check for buffer overflows. */
  (void)argc;
  (void)argv;
  configASSERT( chp );

  /* Generate a table of task stats. */
  strcpy( chp, "Task" );
  chp += strlen( chp );

  /* Minus three for the null terminator and half the number of characters in
  "Task" so the column lines up with the centre of the heading. */
  configASSERT( configMAX_TASK_NAME_LEN > 3 );
  for( xSpace = strlen( "Task" ); xSpace < ( configMAX_TASK_NAME_LEN - 3 ); xSpace++ )
  {
    /* Add a space to align columns after the task's name. */
    *chp = ' ';
    chp++;

    /* Ensure always terminated. */
    *chp = 0x00;
  }
  strcpy( chp, pcHeader );
  vTaskList( chp + strlen( pcHeader ) );

  CDC_Transmit_FS((uint8_t* )chp, strlen(chp));
}
static const CLI_Command_Definition_t x_cmd_task = {
"task", "usage: task\r\n", (shellcmd_t)cmd_task, -1};

static void eterm_set(int term, float re, float im)
{
  int i;
  for (i = 0; i < sweep_points; i++) {
    cal_data[term][i][0] = re;
    cal_data[term][i][1] = im;
  }
}

static void eterm_copy(int dst, int src)
{
  memcpy(cal_data[dst], cal_data[src], sizeof cal_data[dst]);
}


const struct open_model {
  float c0;
  float c1;
  float c2;
  float c3;
} open_model = { 50, 0, -300, 27 };

#if 0
static void adjust_ed(void)
{
  int i;
  for (i = 0; i < sweep_points; i++) {
    // z=1/(jwc*z0) = 1/(2*pi*f*c*z0)  Note: normalized with Z0
    // s11ao = (z-1)/(z+1) = (1-1/z)/(1+1/z) = (1-jwcz0)/(1+jwcz0)
    // prepare 1/s11ao to avoid dividing complex
    float c = 1000e-15;
    float z0 = 50;
    //float z = 6.2832 * frequencies[i] * c * z0;
    float z = 0.02;
    cal_data[ETERM_ED][i][0] += z;
  }
}
#endif

// S'11mo = S11mo-Ed =  Er/(1-Es) = a; Es = 1-Er/a
// S'11ms = S11ms-Ed = -Er/(1+Es) = b; Es = -Er/b-1
// Es = (a+b)/(a-b)
// Er = 2ab/(b-a)
static void eterm_calc_es(void)
{
  int i;
  for (i = 0; i < sweep_points; i++) {
    // z=1/(jwc*z0) = 1/(2*pi*f*c*z0)  Note: normalized with Z0 normalized:归一化
    // s11ao = (z-1)/(z+1) = (1-1/z)/(1+1/z) = (1-jwcz0)/(1+jwcz0)
    // prepare 1/s11ao for effeiciency
    float c = 50e-15;
    //float c = 1.707e-12;
    float z0 = 50;
    float z = 6.2832 * frequencies[i] * c * z0;
    float sq = 1 + z*z;
    float s11aor = (1 - z*z) / sq;
    float s11aoi = 2*z / sq;

    // S11mo’= S11mo - Ed
    // S11ms’= S11ms - Ed
    float s11or = cal_data[CAL_OPEN][i][0] - cal_data[ETERM_ED][i][0];
    float s11oi = cal_data[CAL_OPEN][i][1] - cal_data[ETERM_ED][i][1];
    float s11sr = cal_data[CAL_SHORT][i][0] - cal_data[ETERM_ED][i][0];
    float s11si = cal_data[CAL_SHORT][i][1] - cal_data[ETERM_ED][i][1];
    // Es = (S11mo'/s11ao + S11ms’)/(S11mo' - S11ms’)
    float numr = s11sr + s11or * s11aor - s11oi * s11aoi;
    float numi = s11si + s11oi * s11aor + s11or * s11aoi;
    float denomr = s11or - s11sr;
    float denomi = s11oi - s11si;
    sq = denomr*denomr+denomi*denomi;
    cal_data[ETERM_ES][i][0] = (numr*denomr + numi*denomi)/sq;  // ETERM_ES = 1
    cal_data[ETERM_ES][i][1] = (numi*denomr - numr*denomi)/sq;  // 替换 cal_data[CAL_OPEN]
  }
  cal_status &= ~CALSTAT_OPEN;
  cal_status |= CALSTAT_ES;
}

// S'11mo = S11mo-Ed =  Er/(1-Es) = a; Es = 1-Er/a
// S'11ms = S11ms-Ed = -Er/(1+Es) = b; Es = -Er/b-1
// Es = (a+b)/(a-b)
// Er = 2ab/(b-a)
static void eterm_calc_er(int sign)
{
  int i;
  for (i = 0; i < sweep_points; i++) {
    // Er = sign*(1-sign*Es)S11ms'
    float s11sr = cal_data[CAL_SHORT][i][0] - cal_data[ETERM_ED][i][0];
    float s11si = cal_data[CAL_SHORT][i][1] - cal_data[ETERM_ED][i][1];
    float esr = cal_data[ETERM_ES][i][0];
    float esi = cal_data[ETERM_ES][i][1];
    if (sign > 0) {
      esr = -esr;
      esi = -esi;
    }
    esr = 1 + esr;
    float err = esr * s11sr - esi * s11si;
    float eri = esr * s11si + esi * s11sr;
    if (sign < 0) {
      err = -err;
      eri = -eri;
    }
    cal_data[ETERM_ER][i][0] = err;  // ETERM_ER=2
    cal_data[ETERM_ER][i][1] = eri;  // 替换 cal_data[CALSTAT_SHORT]
    // cal_data[ETERM_ES][i][1] = 0;
  }
  cal_status &= ~CALSTAT_SHORT;
  cal_status |= CALSTAT_ER;
}

// CAUTION: Et is inversed for efficiency
static void eterm_calc_et(void)
{
  int i;
  for (i = 0; i < sweep_points; i++) {
    // Et = 1/(S21mt - Ex)(1 - Es)
    float esr = 1 - cal_data[ETERM_ES][i][0];
    float esi = -cal_data[ETERM_ES][i][1];
    float s21mr = cal_data[CAL_THRU][i][0] - cal_data[CAL_ISOLN][i][0];
    float s21mi = cal_data[CAL_THRU][i][1] - cal_data[CAL_ISOLN][i][1];
    float etr = esr * s21mr - esi * s21mi;
    float eti = esr * s21mi + esi * s21mr;
    float sq = etr*etr + eti*eti;
    float invr = etr / sq;
    float invi = -eti / sq;
    cal_data[ETERM_ET][i][0] = invr;  // ETERM_ET=3
    cal_data[ETERM_ET][i][1] = invi;  // 替换 cal_data[CALSTAT_THRU]
  }
  cal_status &= ~CALSTAT_THRU;
  cal_status |= CALSTAT_ET;
}

void apply_error_term(void)
{
  int i;
  for (i = 0; i < sweep_points; i++) {
    // S11m' = S11m - Ed
    // S11a = S11m' / (Er + Es S11m')
    float s11mr = measured[0][i][0] - cal_data[ETERM_ED][i][0];
    float s11mi = measured[0][i][1] - cal_data[ETERM_ED][i][1];
    float err = cal_data[ETERM_ER][i][0] + s11mr * cal_data[ETERM_ES][i][0] - s11mi * cal_data[ETERM_ES][i][1];
    float eri = cal_data[ETERM_ER][i][1] + s11mr * cal_data[ETERM_ES][i][1] + s11mi * cal_data[ETERM_ES][i][0];
    float sq = err*err + eri*eri;
    float s11ar = (s11mr * err + s11mi * eri) / sq;
    float s11ai = (s11mi * err - s11mr * eri) / sq;
    measured[0][i][0] = s11ar;
    measured[0][i][1] = s11ai;

    // CAUTION: Et is inversed for efficiency
    // S21m' = S21m - Ex
    // S21a = S21m' (1-EsS11a)Et
    float s21mr = measured[1][i][0] - cal_data[ETERM_EX][i][0];
    float s21mi = measured[1][i][1] - cal_data[ETERM_EX][i][1];
    float esr = 1 - (cal_data[ETERM_ES][i][0] * s11ar - cal_data[ETERM_ES][i][1] * s11ai);
    float esi = - (cal_data[ETERM_ES][i][1] * s11ar + cal_data[ETERM_ES][i][0] * s11ai);
    float etr = esr * cal_data[ETERM_ET][i][0] - esi * cal_data[ETERM_ET][i][1];
    float eti = esr * cal_data[ETERM_ET][i][1] + esi * cal_data[ETERM_ET][i][0];
    float s21ar = s21mr * etr - s21mi * eti;
    float s21ai = s21mi * etr + s21mr * eti;
    measured[1][i][0] = s21ar;
    measured[1][i][1] = s21ai;
  }
}

void apply_error_term_at(int i)
{
  // S11m' = S11m - Ed
  // S11a = S11m' / (Er + Es S11m')
  float s11mr = measured[0][i][0] - cal_data[ETERM_ED][i][0];
  float s11mi = measured[0][i][1] - cal_data[ETERM_ED][i][1];
  float err = cal_data[ETERM_ER][i][0] + s11mr * cal_data[ETERM_ES][i][0] - s11mi * cal_data[ETERM_ES][i][1];
  float eri = cal_data[ETERM_ER][i][1] + s11mr * cal_data[ETERM_ES][i][1] + s11mi * cal_data[ETERM_ES][i][0];
  float sq = err*err + eri*eri;
  float s11ar = (s11mr * err + s11mi * eri) / sq;
  float s11ai = (s11mi * err - s11mr * eri) / sq;
  measured[0][i][0] = s11ar; // real 校准反射系数
  measured[0][i][1] = s11ai; // imag

  // CAUTION: Et is inversed for efficiency
  // S21m' = S21m - Ex
  // S21a = S21m' (1-EsS11a)Et
  float s21mr = measured[1][i][0] - cal_data[ETERM_EX][i][0];
  float s21mi = measured[1][i][1] - cal_data[ETERM_EX][i][1];
  float esr = 1 - (cal_data[ETERM_ES][i][0] * s11ar - cal_data[ETERM_ES][i][1] * s11ai);
  float esi = - (cal_data[ETERM_ES][i][1] * s11ar + cal_data[ETERM_ES][i][0] * s11ai);
  float etr = esr * cal_data[ETERM_ET][i][0] - esi * cal_data[ETERM_ET][i][1];
  float eti = esr * cal_data[ETERM_ET][i][1] + esi * cal_data[ETERM_ET][i][0];
  float s21ar = s21mr * etr - s21mi * eti;
  float s21ai = s21mi * etr + s21mr * eti;
  measured[1][i][0] = s21ar; // real 校准传输系数
  measured[1][i][1] = s21ai; // imag
}

void apply_edelay_at(int i)
{
  float w = 2 * M_PI * electrical_delay * frequencies[i] * 1E-12;
  float s = sin(w);
  float c = cos(w);
  float real = measured[0][i][0];
  float imag = measured[0][i][1];
  measured[0][i][0] = real * c - imag * s;
  measured[0][i][1] = imag * c + real * s;
  real = measured[1][i][0];
  imag = measured[1][i][1];
  measured[1][i][0] = real * c - imag * s;
  measured[1][i][1] = imag * c + real * s;
}

/*
=======================================
    执行校准时，先采集数据，再校准
    measured 拷贝到 cal_data
=======================================
*/
void cal_collect(int type)
{
  ensure_edit_config();
  chMtxLock(&mutex);

  switch (type) {
  case CAL_LOAD:  // 采集反射信号 Ed = S11ml; 直接采集 CAL_LOAD=ETERM_ED=0
    cal_status |= CALSTAT_LOAD;
    memcpy(cal_data[CAL_LOAD], measured[0], sizeof measured[0]);
    break;

  case CAL_OPEN:  // 采集反射信号 S11mo-Ed =  Er/(1-Es)
    cal_status |= CALSTAT_OPEN;
    cal_status &= ~(CALSTAT_ES|CALSTAT_APPLY);
    memcpy(cal_data[CAL_OPEN], measured[0], sizeof measured[0]);
    break;

  case CAL_SHORT: // 采集反射信号 S11ms-Ed = -Er/(1+Es)
    cal_status |= CALSTAT_SHORT;
    cal_status &= ~(CALSTAT_ER|CALSTAT_APPLY);
    memcpy(cal_data[CAL_SHORT], measured[0], sizeof measured[0]);
    break;

  case CAL_THRU:  // 采集传输信号 Et = S21mt - Ex
    cal_status |= CALSTAT_THRU;
    memcpy(cal_data[CAL_THRU], measured[1], sizeof measured[0]);
    break;

  case CAL_ISOLN: // 采集传输信号 Ex = S21ml; 直接采集 CAL_ISOLN=ETERM_EX=4
    cal_status |= CALSTAT_ISOLN;
    memcpy(cal_data[CAL_ISOLN], measured[1], sizeof measured[0]);
    break;
  }
  chMtxUnlock(&mutex);
}

/*
=======================================
    计算校准数据
=======================================
*/
void cal_done(void)
{
  ensure_edit_config();
  if (!(cal_status & CALSTAT_LOAD))  // Ed 默认等于 S11ml，如果没有 LOAD 校准，则清空 Ed
    eterm_set(ETERM_ED, 0.0, 0.0);
  //adjust_ed();
  if ((cal_status & CALSTAT_SHORT) && (cal_status & CALSTAT_OPEN)) { // 如果有短路有开路
    eterm_calc_es();               // 计算 Es = (a+b)/(a-b)
    eterm_calc_er(-1);             // 计算 Er = 2ab/(b-a)
  } else if (cal_status & CALSTAT_OPEN) {    // 如果只有开路
    eterm_copy(CAL_SHORT, CAL_OPEN);
    eterm_set(ETERM_ES, 0.0, 0.0);
    eterm_calc_er(1);
  } else if (cal_status & CALSTAT_SHORT) {   // 如果只有短路
    eterm_set(ETERM_ES, 0.0, 0.0);
    cal_status &= ~CALSTAT_SHORT;
    eterm_calc_er(-1);
  } else {
    eterm_set(ETERM_ER, 1.0, 0.0);         // 无短路无开路
    eterm_set(ETERM_ES, 0.0, 0.0);
  }

  if (!(cal_status & CALSTAT_ISOLN))         // Ex 默认等于 S21ml，如果没有隔离，则清空 Ex
    eterm_set(ETERM_EX, 0.0, 0.0);         // 
  if (cal_status & CALSTAT_THRU) {           // 如果有直通，Et = S21mt - Ex
    eterm_calc_et();
  } else {
    eterm_set(ETERM_ET, 1.0, 0.0);
  }

  cal_status |= CALSTAT_APPLY;
}

/*
=======================================
    校准插值？
=======================================
*/
void cal_interpolate(int s)
{
  const properties_t *src = caldata_ref(s);
  int i, j;
  int eterm;
  if (src == NULL)
    return;

  ensure_edit_config();

  // lower than start freq of src range
  for (i = 0; i < sweep_points; i++) {
    if (frequencies[i] >= src->_frequencies[0])
      break;

    // fill cal_data at head of src range
    for (eterm = 0; eterm < 5; eterm++) {
      cal_data[eterm][i][0] = src->_cal_data[eterm][0][0];
      cal_data[eterm][i][1] = src->_cal_data[eterm][0][1];
    }
  }

  j = 0;
  for (; i < sweep_points; i++) {
    uint32_t f = frequencies[i];

    for (; j < sweep_points-1; j++) {
      if (src->_frequencies[j] <= f && f < src->_frequencies[j+1]) {
        // found f between freqs at j and j+1
        float k1 = (float)(f - src->_frequencies[j])
                    / (src->_frequencies[j+1] - src->_frequencies[j]);
        float k0 = 1.0 - k1;
        for (eterm = 0; eterm < 5; eterm++) {
          cal_data[eterm][i][0] = src->_cal_data[eterm][j][0] * k0 + 
                                  src->_cal_data[eterm][j+1][0] * k1;
          cal_data[eterm][i][1] = src->_cal_data[eterm][j][1] * k0 + 
                                  src->_cal_data[eterm][j+1][1] * k1;
        }
        break;
      }
    }
    if (j == sweep_points-1)
      break;
  }

  // upper than end freq of src range
  for (; i < sweep_points; i++) {
  // fill cal_data at tail of src
    for (eterm = 0; eterm < 5; eterm++) {
      cal_data[eterm][i][0] = src->_cal_data[eterm][sweep_points-1][0];
      cal_data[eterm][i][1] = src->_cal_data[eterm][sweep_points-1][1];
    }
  }

  cal_status |= src->_cal_status | CALSTAT_APPLY | CALSTAT_INTERPOLATED;
}

/*
=======================================
    命令：校准命令
=======================================
*/
static void cmd_cal(BaseSequentialStream *chp, int argc, char *argv[])
{
  const char *items[] = { "load", "open", "short", "thru", "isoln", "Es", "Er", "Et", "cal'ed" };

  if (argc == 0) {
    int i;
    for (i = 0; i < 9; i++) {
      if (cal_status & (1<<i))
        chprintf(chp, "%s ", items[i]);
    }
    chprintf(chp, "\r\n");
    return;
  }

  char *cmd = argv[0];
  if (strcmp(cmd, "load") == 0) {
    cal_collect(CAL_LOAD);    // 负载
  } else if (strcmp(cmd, "open") == 0) {
    cal_collect(CAL_OPEN);    // 开路
  } else if (strcmp(cmd, "short") == 0) {
    cal_collect(CAL_SHORT);   // 短路
  } else if (strcmp(cmd, "thru") == 0) {
    cal_collect(CAL_THRU);    // 直通
  } else if (strcmp(cmd, "isoln") == 0) {
    cal_collect(CAL_ISOLN);   // 隔离
  } else if (strcmp(cmd, "done") == 0) {
    cal_done();
    draw_cal_status();
    return;
  } else if (strcmp(cmd, "on") == 0) {
    cal_status |= CALSTAT_APPLY;
    draw_cal_status();
    return;
  } else if (strcmp(cmd, "off") == 0) {
    cal_status &= ~CALSTAT_APPLY;
    draw_cal_status();
  return;
  } else if (strcmp(cmd, "reset") == 0) {
    cal_status = 0;
    draw_cal_status();
    return;
  } else if (strcmp(cmd, "data") == 0) {
    chprintf(chp, "%f %f\r\n", cal_data[CAL_LOAD][0][0], cal_data[CAL_LOAD][0][1]);
    chprintf(chp, "%f %f\r\n", cal_data[CAL_OPEN][0][0], cal_data[CAL_OPEN][0][1]);
    chprintf(chp, "%f %f\r\n", cal_data[CAL_SHORT][0][0], cal_data[CAL_SHORT][0][1]);
    chprintf(chp, "%f %f\r\n", cal_data[CAL_THRU][0][0], cal_data[CAL_THRU][0][1]);
    chprintf(chp, "%f %f\r\n", cal_data[CAL_ISOLN][0][0], cal_data[CAL_ISOLN][0][1]);
    return;
  } else if (strcmp(cmd, "in") == 0) {
    int s = 0;
    if (argc > 1)
      s = atoi(argv[1]);
    cal_interpolate(s);
    draw_cal_status();
    return;
  } else {
    chprintf(chp, "usage: cal [load|open|short|thru|isoln|done|reset|on|off|in]\r\n");
    return;
  }
}
static const CLI_Command_Definition_t x_cmd_cal = {
"cal", "usage: cal [load|open|short|thru|isoln|done|reset|on|off|in]\r\n", (shellcmd_t)cmd_cal, -1};

/*
=======================================
    命令：参数保存
=======================================
*/
static void cmd_save(BaseSequentialStream *chp, int argc, char *argv[])
{
  int id;
  (void)chp;

  if (argc != 1)
    goto usage;

  id = atoi(argv[0]);
  if (id < 0 || id >= SAVEAREA_MAX)
    goto usage;
  caldata_save(id);
  draw_cal_status();
  return;

  usage:
    chprintf(chp, "usage: save {id}\r\n");
}
static const CLI_Command_Definition_t x_cmd_save = {
"save", "usage: save {id}\r\n", (shellcmd_t)cmd_save, -1};

static void cmd_recall(BaseSequentialStream *chp, int argc, char *argv[])
{
  int id;
  (void)chp;
  if (argc != 1)
    goto usage;

  id = atoi(argv[0]);
  if (id < 0 || id >= SAVEAREA_MAX)
    goto usage;

  pause_sweep();
  chMtxLock(&mutex);
  if (caldata_recall(id) == 0) {
    // success
    update_frequencies();
    draw_cal_status();
  }
  chMtxUnlock(&mutex);
  resume_sweep();
  return;

usage:
  chprintf(chp, "usage: recall {id}\r\n");
}
static const CLI_Command_Definition_t x_cmd_recall = {
"recall", "usage: recall {id}\r\n", (shellcmd_t)cmd_recall, -1};

// 幅频图/相频图/群时延/史密斯图/极坐标图/幅频图/驻波比
const char *trc_type_name[] = {
  "LOGMAG", "PHASE", "DELAY", "SMITH", "POLAR", "LINEAR", "SWR"
};
const uint8_t default_refpos[] = {
  7, 4, 4, 0, 0, 0, 0
};

const char *trc_channel_name[] = {
  "S11", "S21"
};

void set_trace_type(int t, int type)
{
  int polar = type == TRC_SMITH || type == TRC_POLAR;
  int enabled = type != TRC_OFF;
  int force = FALSE;

  if (trace[t].polar != polar) {
    trace[t].polar = polar;
    force = TRUE;
  }
  if (trace[t].enabled != enabled) {
    trace[t].enabled = enabled;
    force = TRUE;
  }
  if (trace[t].type != type) {
    trace[t].type = type;
    trace[t].refpos = default_refpos[type];
    if (polar)
      force = TRUE;
  }    
  if (force) {
    plot_into_index(measured);
    force_set_markmap();
  }
}

void set_trace_channel(int t, int channel)
{
  if (trace[t].channel != channel) {
    trace[t].channel = channel;
    force_set_markmap();
  }
}

void set_trace_scale(int t, float scale)
{
  switch (trace[t].type) {
  case TRC_LOGMAG:
    scale /= 10;
    break;
  case TRC_PHASE:
    scale /= 90;
    break;
  }

  if (trace[t].scale != scale) {
    trace[t].scale = scale;
    force_set_markmap();
  }
}

float get_trace_scale(int t)
{
  float n = 1;
  if (trace[t].type == TRC_LOGMAG)
    n = 10;
  else if (trace[t].type == TRC_PHASE)
    n = 90;
  return trace[t].scale * n;
}

void set_trace_refpos(int t, float refpos)
{
  if (trace[t].refpos != refpos) {
    trace[t].refpos = refpos;
    force_set_markmap();
  }
}

float get_trace_refpos(int t)
{
  return trace[t].refpos;
}

float my_atof(const char *p)
{
  int neg = FALSE;
  if (*p == '-')
    neg = TRUE;
  if (*p == '-' || *p == '+')
    p++;
  float x = atoi(p);
  while (isdigit((int)*p))
    p++;
  if (*p == '.') {
    float d = 1.0f;
    p++;
    while (isdigit((int)*p)) {
      d /= 10;
      x += d * (*p - '0');
      p++;
    }
  }
  if (*p == 'e' || *p == 'E') {
    p++;
    int exp = atoi(p);
    while (exp > 0) {
      x *= 10;
      exp--;
    }
    while (exp < 0) {
      x /= 10;
      exp++;
    }
  }
  if (neg)
    x = -x;
  return x;
}

static void cmd_trace(BaseSequentialStream *chp, int argc, char *argv[])
{
    int t;
    if (argc == 0) {
      for (t = 0; t < 4; t++) {
        if (trace[t].enabled) {
          const char *type = trc_type_name[trace[t].type];
          const char *channel = trc_channel_name[trace[t].channel];
          float scale = trace[t].scale;
          float refpos = trace[t].refpos;
          chprintf(chp, "%d %s %s %f %f\r\n", t, type, channel, scale, refpos);
        }
      }
      return;
    }

    if (strcmp(argv[0], "all") == 0 &&
      argc > 1 && strcmp(argv[1], "off") == 0) {
      set_trace_type(0, TRC_OFF);
      set_trace_type(1, TRC_OFF);
      set_trace_type(2, TRC_OFF);
      set_trace_type(3, TRC_OFF);
      goto exit;
    }

    t = atoi(argv[0]);
    if (t < 0 || t >= 4)
      goto usage;
    if (argc == 1) {
      const char *type = trc_type_name[trace[t].type];
      const char *channel = trc_channel_name[trace[t].channel];
      chprintf(chp, "%d %s %s\r\n", t, type, channel);
      return;
    }
    if (argc > 1) {
      if (strcmp(argv[1], "logmag") == 0) {
        set_trace_type(t, TRC_LOGMAG);
      } else if (strcmp(argv[1], "phase") == 0) {
        set_trace_type(t, TRC_PHASE);
      } else if (strcmp(argv[1], "polar") == 0) {
        set_trace_type(t, TRC_POLAR);
      } else if (strcmp(argv[1], "smith") == 0) {
        set_trace_type(t, TRC_SMITH);
      } else if (strcmp(argv[1], "delay") == 0) {
        set_trace_type(t, TRC_DELAY);
      } else if (strcmp(argv[1], "linear") == 0) {
        set_trace_type(t, TRC_LINEAR);
      } else if (strcmp(argv[1], "swr") == 0) {
        set_trace_type(t, TRC_SWR);
      } else if (strcmp(argv[1], "off") == 0) {
        set_trace_type(t, TRC_OFF);
      } else if (strcmp(argv[1], "scale") == 0 && argc >= 3) {
        //trace[t].scale = my_atof(argv[2]);
        set_trace_scale(t, my_atof(argv[2]));
        goto exit;
      } else if (strcmp(argv[1], "refpos") == 0 && argc >= 3) {
        //trace[t].refpos = my_atof(argv[2]);
        set_trace_refpos(t, my_atof(argv[2]));
        goto exit;
      }
    }
    if (argc > 2) {
      int src = atoi(argv[2]);
      if (src != 0 && src != 1)
        goto usage;
      trace[t].channel = src;
    }
exit:
    return;
usage:
    chprintf(chp, "trace {0|1|2|3|all} [logmag|phase|smith|linear|delay|swr|off] [src]\r\n");
}
static const CLI_Command_Definition_t x_cmd_trace = {
"trace", "usage: trace {id}\r\n", (shellcmd_t)cmd_trace, -1};

void set_electrical_delay(float picoseconds)
{
  if (electrical_delay != picoseconds) {
    electrical_delay = picoseconds;
    force_set_markmap();
  }
}

float get_electrical_delay(void)
{
  return electrical_delay;
}

static void cmd_edelay(BaseSequentialStream *chp, int argc, char *argv[])
{
  if (argc == 0) {
    chprintf(chp, "%f\r\n", electrical_delay);
    return;
  }
  if (argc > 0) {
    set_electrical_delay(my_atof(argv[0]));
  }
}
static const CLI_Command_Definition_t x_cmd_edelay = {
"edelay", "usage: edelay {id}\r\n", (shellcmd_t)cmd_edelay, -1};

static void cmd_marker(BaseSequentialStream *chp, int argc, char *argv[])
{
  int t;
  if (argc == 0) {
    for (t = 0; t < 4; t++) {
      if (markers[t].enabled) {
        chprintf(chp, "%d %d %d\r\n", t+1, markers[t].index, markers[t].frequency);
      }
    }
    return;
  }
  if (strcmp(argv[0], "off") == 0) {
    active_marker = -1;
    for (t = 0; t < 4; t++)
      markers[t].enabled = FALSE;
    return;
  }

  t = atoi(argv[0])-1;
  if (t < 0 || t >= 4)
    goto usage;
  if (argc == 1) {
    chprintf(chp, "%d %d %d\r\n", t+1, markers[t].index, frequency);
    active_marker = t;
    markers[t].enabled = TRUE;
    return;
  }
  if (argc > 1) {
    if (strcmp(argv[1], "off") == 0) {
      markers[t].enabled = FALSE;
      if (active_marker == t)
      active_marker = -1;
    } else if (strcmp(argv[1], "on") == 0) {
      markers[t].enabled = TRUE;
      active_marker = t;
    } else {
      markers[t].enabled = TRUE;
      int index = atoi(argv[1]);
      markers[t].index = index;
      markers[t].frequency = frequencies[index];
      active_marker = t;
    }
  }
  return;
usage:
  chprintf(chp, "usage: marker [n] [off|{index}]\r\n");
}
static const CLI_Command_Definition_t x_cmd_marker = {
"marker", "usage: marker [n] [off|{index}]\r\n", (shellcmd_t)cmd_marker, -1};

static void cmd_touchcal(BaseSequentialStream *chp, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    int i;

    chMtxLock(&mutex);
    osThreadSuspend(Task001Handle);

    chprintf(chp, "first touch upper left, then lower right...");
    touch_cal_exec();
    chprintf(chp, "done\r\n");

    chprintf(chp, "touch cal params: ");
    for (i = 0; i < 4; i++) {
        chprintf(chp, "%d ", config.touch_cal[i]);
    }
    chprintf(chp, "\r\n");
    chMtxUnlock(&mutex);

    osThreadResume(Task001Handle);
}
static const CLI_Command_Definition_t x_cmd_touchcal = {
"touchcal", "usage: touchcal\r\n", (shellcmd_t)cmd_touchcal, -1};

static void cmd_touchtest(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;

#if 0
  uint8_t val = 0xFF;
  ctp_readreg(FT_ID_G_MODE, &val, 1);
  dbprintf("FT_ID_G_MODE         %02X\r\n", val);
  ctp_readreg(FT_ID_G_PERIODACTIVE, &val, 1);
  dbprintf("FT_ID_G_PERIODACTIVE %02X\r\n", val);
#endif

#if 1
  chMtxLock(&mutex);
  do {
    touch_draw_test();
  } while(argc);
  chMtxUnlock(&mutex);
#endif


}
static const CLI_Command_Definition_t x_cmd_touchtest = {
"touchtest", "usage: touchtest\r\n", (shellcmd_t)cmd_touchtest, -1};

static void cmd_frequencies(BaseSequentialStream *chp, int argc, char *argv[])
{
  int i;
  (void)chp;
  (void)argc;
  (void)argv;
  for (i = 0; i < sweep_points; i++) {
    chprintf(chp, "%d\r\n", frequencies[i]);
  }
}
static const CLI_Command_Definition_t x_cmd_frequencies = {
"frequencies", "usage: frequencies\r\n", (shellcmd_t)cmd_frequencies, -1};

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)chp;
  (void)argc;
  (void)argv;

#if 0
  int i;
  for (i = 0; i < 100; i++) {
    palClearPad(GPIOC, GPIOC_LED);
    set_frequency(10000000);
    palSetPad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(50);

    palClearPad(GPIOC, GPIOC_LED);
    set_frequency(90000000);
    palSetPad(GPIOC, GPIOC_LED);
    chThdSleepMilliseconds(50);
  }
#endif

#if 0
  int i;
  int mode = 0;
  if (argc >= 1)
    mode = atoi(argv[0]);

  for (i = 0; i < 20; i++) {
    // palClearPad(GPIOC, GPIOC_LED);
    nt35510_test(mode);
    // palSetPad(GPIOC, GPIOC_LED);
    // chThdSleepMilliseconds(50);
    osDelay(50);
  }
#endif

#if 0
  //extern adcsample_t adc_samples[2];
  //chprintf(chp, "adc: %d %d\r\n", adc_samples[0], adc_samples[1]);
  int i;
  int x, y;
  for (i = 0; i < 50; i++) {
    test_touch(&x, &y);
    chprintf(chp, "adc: %d %d\r\n", x, y);
    chThdSleepMilliseconds(200);
  }
  //extern int touch_x, touch_y;
  //chprintf(chp, "adc: %d %d\r\n", touch_x, touch_y);
#endif

#if 0
  while (argc > 1) {
    int x, y;
    touch_position(&x, &y);
    chprintf(chp, "touch: %d %d\r\n", x, y);
    chThdSleepMilliseconds(200);
  }
#endif

#if 0
  int x, y;
  uint8_t buf[7];
  while (argc > 1)
  {
    if (!CTP_INT_IN())
    {
      ctp_readreg(0, (uint8_t *)&buf, 7);
      if ((buf[2]&0x0F) == 1)
      {
        x = (uint16_t)(buf[5] & 0x0F)<<8 | (uint16_t)buf[6];
        y = (uint16_t)(buf[3] & 0x0F)<<8 | (uint16_t)buf[4];
        dbprintf("Event %d, X:%3d Y:%3d\r\n", (buf[3] & 0xC0) >> 6, x, y);
      }
    }
    osDelay(10);
  }
#endif

#if 0
  dbprintf("config.magic: %08X\r\n", config.magic);
  dbprintf("props.checksum: %08X\r\n", current_props.checksum);
#endif

#if 0
  nt35510_drawstring_x2(&font_06x13,  "0123ABCDMNOPQ",   0,  0, 0xFFFF, 0x0000);
  nt35510_drawstring_x2(&font_06x13,  "0123ABCDMNOPQ",   0, 13, 0xFFFF, 0x0000);

  nt35510_drawstring(&font_08x15,  "0123ABCDMNOPQ", 400,  0, 0xFFFF, 0x0000);
  nt35510_drawstring(&font_12x24, "0123ABCDMNOPQ", 400, 15, 0xFFFF, 0x0000);
#endif

#if 0
  int x, y;
  while (argc > 1)
  {
    if (g_TP_Irq)
    {
      g_TP_Irq = 0;
      dbprintf("[%8d] RTP Irq !\r\n", HAL_GetTick());
    }
    osDelay(10);
  }
#endif

#if 1
  while (argc > 1)
  {
    if (g_TP_Irq)
    {
      g_TP_Irq = 0;
      HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
      dbprintf("[%8d] RTP X:%4d Y:%4d\r\n", HAL_GetTick(), TPReadX(), TPReadY());
    }
    osDelay(10);
    __HAL_GPIO_EXTI_CLEAR_IT(TP_IRQ_Pin);
    HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  }
#endif
}
static const CLI_Command_Definition_t x_cmd_test = {
"test", "usage: test\r\n", (shellcmd_t)cmd_test, -1};

static void cmd_gain(BaseSequentialStream *chp, int argc, char *argv[])
{
  int rvalue;
  int lvalue = 0;
  if (argc != 1 && argc != 2) {
    chprintf(chp, "usage: gain {lgain(0-95)} [rgain(0-95)]\r\n");
    return;
  }
  rvalue = atoi(argv[0]);
  if (argc == 2) 
    lvalue = atoi(argv[1]);
  tlv320aic3204_set_gain(lvalue, rvalue);
}
static const CLI_Command_Definition_t x_cmd_gain = {
"gain", "usage: gain {lgain(0-95)} [rgain(0-95)]\r\n", (shellcmd_t)cmd_gain, -1};

static void cmd_port(BaseSequentialStream *chp, int argc, char *argv[])
{
  int port;
  if (argc != 1) {
    chprintf(chp, "usage: port {1:S11 2:S21}\r\n");
    return;
  }
  port = atoi(argv[0]);
  if (port == 1)
    tlv320aic3204_select_in3(); // 反射
  else
    tlv320aic3204_select_in1(); // 传输
}
static const CLI_Command_Definition_t x_cmd_port = {
"port", "usage: port {1:S11 2:S21}\r\n", (shellcmd_t)cmd_port, -1};

/*
=======================================
    APP 初始化
=======================================
*/
void app_init(void)
{
  /* 递归锁 Recursive Mutex
     二者唯一的区别是，同一个线程可以多次获取同一个递归锁，不会产生死锁。
     而如果一个线程多次获取同一个非递归锁，则会产生死锁。
  */
  mutex = xSemaphoreCreateRecursiveMutex();
  configASSERT( mutex );

  I2C_InitGPIO();

  CTP_RST_L();
  osDelay(5);    //
  CTP_RST_H();

  /* 检查所有 I2C 设备 */
  si5351_init();

  AIC_RESET_L;
  osDelay(5);
  AIC_RESET_H;
  osDelay(5);
  tlv320aic3204_init_slave();

  /* LCD 初始化 */
  nt35510_init();
  osDelay(100);    //
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 1500);
  // HAL_GPIO_WritePin(LED_PWM_GPIO_Port, LED_PWM_Pin, GPIO_PIN_SET);

  // ctp_init();
  rtp_init();

  /*
   * Initialize graph plotting
   */
  plot_init();  // uint16_t markmap[2][8] 全 0xFF

  /* restore config */
  config_recall();  // 重载配置，如果有的话

  // DAC Start

  /* initial frequencies */
  update_frequencies();

  /* restore frequencies and calibration properties from flash memory */
  if (config.default_loadcal >= 0)
    caldata_recall(config.default_loadcal);

  redraw_frame();

  /* 开始 I2S 接收 */
  if (HAL_I2S_Receive_DMA(&hi2s2, (uint16_t *)rx_buffer, AUDIO_BUFFER_LEN*2) != HAL_OK)
  {
    Error_Handler();
  }

  ui_init();
}

/*
=======================================
    注册命令
=======================================
*/
void cmd_register( void )
{
  FreeRTOS_CLIRegisterCommand( &x_cmd_reset );
  FreeRTOS_CLIRegisterCommand( &x_cmd_freq );
  FreeRTOS_CLIRegisterCommand( &x_cmd_offset );
  FreeRTOS_CLIRegisterCommand( &x_cmd_time );
  FreeRTOS_CLIRegisterCommand( &x_cmd_dac );
  FreeRTOS_CLIRegisterCommand( &x_cmd_saveconfig );
  FreeRTOS_CLIRegisterCommand( &x_cmd_clearconfig );
  FreeRTOS_CLIRegisterCommand( &x_cmd_data );

#ifdef ENABLED_DUMP
  FreeRTOS_CLIRegisterCommand( &x_cmd_dump );
#endif

  FreeRTOS_CLIRegisterCommand( &x_cmd_frequencies );
  FreeRTOS_CLIRegisterCommand( &x_cmd_port );
  FreeRTOS_CLIRegisterCommand( &x_cmd_gain );
  FreeRTOS_CLIRegisterCommand( &x_cmd_power );

  FreeRTOS_CLIRegisterCommand( &x_cmd_gamma );
  // FreeRTOS_CLIRegisterCommand( &x_cmd_scan );

  FreeRTOS_CLIRegisterCommand( &x_cmd_sweep );
  FreeRTOS_CLIRegisterCommand( &x_cmd_test );
  FreeRTOS_CLIRegisterCommand( &x_cmd_touchcal );
  FreeRTOS_CLIRegisterCommand( &x_cmd_touchtest );
  FreeRTOS_CLIRegisterCommand( &x_cmd_pause );
  FreeRTOS_CLIRegisterCommand( &x_cmd_resume );
  FreeRTOS_CLIRegisterCommand( &x_cmd_cal );
  FreeRTOS_CLIRegisterCommand( &x_cmd_save );
  FreeRTOS_CLIRegisterCommand( &x_cmd_recall );
  FreeRTOS_CLIRegisterCommand( &x_cmd_trace );
  FreeRTOS_CLIRegisterCommand( &x_cmd_marker );
  FreeRTOS_CLIRegisterCommand( &x_cmd_edelay );

  // FreeRTOS_CLIRegisterCommand( &x_cmd_list );
  // FreeRTOS_CLIRegisterCommand( &x_cmd_fatfs );
  FreeRTOS_CLIRegisterCommand( &x_cmd_pwm );
  FreeRTOS_CLIRegisterCommand( &x_cmd_beep );
  FreeRTOS_CLIRegisterCommand( &x_cmd_lcd );
  FreeRTOS_CLIRegisterCommand( &x_cmd_task );
}

/*
=======================================
    BAT Voltage Display
    VREFINT = 1.2V
=======================================
*/
void bat_adc_display(void)
{
  uint32_t i, adc, ref;

  ref = 0;
  adc = 0;
  for (i=0; i<100; i++)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc1, 10);  // VREFINT
    HAL_ADC_PollForConversion(&hadc2, 10);  // BAT
    ref += (uint32_t)HAL_ADC_GetValue(&hadc1);
    adc += (uint32_t)HAL_ADC_GetValue(&hadc2);
  }

  adc = adc*1200*2/ref;

  nt35510_drawstring_5x7("BAT:",            0,   180, 0xffff, 0x0000);
  nt35510_drawchar_5x7(adc/1000+'0',        0, 190*2, BRG556(0,0,255), 0x0000);
  nt35510_drawchar_5x7('.',               5*2, 190*2, BRG556(0,0,255), 0x0000);
  nt35510_drawchar_5x7(adc%1000/100+'0', 10*2, 190*2, BRG556(0,0,255), 0x0000);
  nt35510_drawchar_5x7(adc%100/10+'0',   15*2, 190*2, BRG556(0,0,255), 0x0000);
  nt35510_drawchar_5x7('V',              20*2, 190*2, BRG556(0,0,255), 0x0000);
}
