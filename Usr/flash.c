/*-----------------------------------------------------------------------------/
 * Module       : flash.c
 * Create       : 2019-06-03
 * Copyright    : hamelec.taobao.com
 * Author       : huanglong
 * Brief        : 
/-----------------------------------------------------------------------------*/
#include "system.h"
#include "board.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "nanovna.h"
#include <string.h>

int flash_erase_page(uint32_t page_address)
{
  FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t PAGEError = 0;

  EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.PageAddress = page_address;
  EraseInitStruct.NbPages     = 1;

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK)
  {
    /*
    Error occurred while page erase.
    User can add here some code to deal with this error.
    PAGEError will contain the faulty page and then to know the code error on this page,
    user can call function 'HAL_FLASH_GetError()'
    */
    /* Infinite loop */
    while (1)
    {
      LED1_ON;
      HAL_Delay(100);
      LED1_OFF;
      HAL_Delay(2000);
    }
  }

  return 0;
}

void flash_program_half_word(uint32_t address, uint16_t data)
{
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, data) != HAL_OK)
  {
    /* Infinite loop */
    while (1)
    {
      LED1_ON;
      HAL_Delay(100);
      LED1_OFF;
      HAL_Delay(2000);
    }
  }
}

static uint32_t checksum(uint8_t *start, size_t len)
{
  uint32_t *p = (uint32_t*)start;
  uint32_t *tail = (uint32_t*)(start + len);
  uint32_t value = 0;
  while (p < tail)
    value ^= *p++;
  return value;
}

#define FLASH_PAGESIZE 0x800

const uint32_t save_config_area = 0x08078000;  // 原先基础上加 0x00060000

/*
=======================================
    配置保存
=======================================
*/
int config_save(void)
{
  uint16_t *src = (uint16_t*)&config;
  uint16_t *dst = (uint16_t*)save_config_area;
  int count = sizeof(config_t) / sizeof(uint16_t);

  config.magic = CONFIG_MAGIC;
  config.checksum = 0;
  config.checksum = checksum((uint8_t *)&config, sizeof config);

  taskENTER_CRITICAL();
  HAL_FLASH_Unlock();

  /* erase flash */
  flash_erase_page((uint32_t)dst);

  /* write to flash */
  while(count-- > 0) {
    flash_program_half_word((uint32_t)dst, *src++);
    dst++;
  }

  HAL_FLASH_Lock();
  taskEXIT_CRITICAL();

  return 0;
}

/*
=======================================
    重载配置
=======================================
*/
int config_recall(void)
{
  const config_t *src = (const config_t*)save_config_area;
  void *dst = &config;

  if (src->magic != CONFIG_MAGIC)
    return -1;
  if (checksum((uint8_t *)src, sizeof(config_t)) != 0)
    return -1;

  /* duplicated saved data onto sram to be able to modify marker/trace */
  memcpy(dst, src, sizeof(config_t));
  return 0;
}

#define SAVEAREA_MAX 5

// 原先基础上加 0x00060000
// 每组参数间隔：0x1800 6KB=6144B
const uint32_t saveareas[] =
{ 0x08078800, 0x0807a000, 0x0807b800, 0x0807d000, 0x0807e800 };

int16_t lastsaveid = 0;

/*
=======================================
    校准保存
=======================================
*/
int caldata_save(int id)
{
  uint16_t *src = (uint16_t*)&current_props;
  uint16_t *dst;
  int count = sizeof(properties_t) / sizeof(uint16_t);

  if (id < 0 || id >= SAVEAREA_MAX)
  return -1;
  dst = (uint16_t*)saveareas[id];

  current_props.magic = CONFIG_MAGIC;
  current_props.checksum = 0;
  current_props.checksum = checksum((uint8_t *)&current_props, sizeof current_props);

  taskENTER_CRITICAL();
  HAL_FLASH_Unlock();

  /* erase flash pages */
  uint8_t *p = (uint8_t *)dst;
  uint8_t *tail = p + sizeof(properties_t);
  while (p < tail) {
    flash_erase_page((uint32_t)p);
    p += FLASH_PAGESIZE;
  }

  /* write to flahs */
  while(count-- > 0) {
    flash_program_half_word((uint32_t)dst, *src++);
    dst++;
  }

  HAL_FLASH_Lock();
  taskEXIT_CRITICAL();

  /* after saving data, make active configuration points to flash */
  active_props = (properties_t*)saveareas[id];
  lastsaveid = id;

  return 0;
}

/*
=======================================
    重载校准
=======================================
*/
int caldata_recall(int id)
{
  properties_t *src;
  void *dst = &current_props;

  if (id < 0 || id >= SAVEAREA_MAX)
    return -1;

  // point to saved area on the flash memory
  src = (properties_t*)saveareas[id];

  if (src->magic != CONFIG_MAGIC)
    return -1;
  if (checksum((uint8_t *)src, sizeof(properties_t)) != 0)
    return -1;

  /* active configuration points to save data on flash memory */
  active_props = src;
  lastsaveid = id;

  /* duplicated saved data onto sram to be able to modify marker/trace */
  memcpy(dst, src, sizeof(properties_t));

  return 0;
}

/*
=======================================
    指针指向某一个校准参数
=======================================
*/
const properties_t * caldata_ref(int id)
{
  const properties_t *src;
  if (id < 0 || id >= SAVEAREA_MAX)
    return NULL;
  src = (const properties_t*)saveareas[id];

  if (src->magic != CONFIG_MAGIC)
    return NULL;
  if (checksum((uint8_t *)src, sizeof(properties_t)) != 0)
    return NULL;
  return src;
}

const uint32_t save_config_prop_area_size = 0x8000;

void clear_all_config_prop_data(void)
{
  taskENTER_CRITICAL();
  HAL_FLASH_Unlock();

  /* erase flash pages */
  uint8_t *p = (uint8_t *)save_config_area;
  uint8_t *tail = p + save_config_prop_area_size;
  while (p < tail) {
    flash_erase_page((uint32_t)p);
    p += FLASH_PAGESIZE;
  }

  HAL_FLASH_Lock();
  taskEXIT_CRITICAL();
}
