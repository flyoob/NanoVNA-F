/*------------------------------------------------------------------------------
* Module Name      : fs_funs.h
* Copyright        :
* Description      : fatfs 使用说明
  FA_READ          指定读访问对象。可以从文件中读取数据。与 FA_WRITE 结合可以进行读写访问。
  FA_WRITE         指定写访问对象。可以向文件中写入数据。与 FA_READ 结合可以进行读写访问。
  FA_OPEN_EXISTING 打开文件。如果文件不存在，则打开失败。 ( 默认 )
  FA_CREATE_NEW    创建一个新文件。如果文件已存在，则创建失败。
  FA_CREATE_ALWAYS 创建一个新文件。如果文件已存在，则它将被截断并覆盖。
  FA_OPEN_ALWAYS   如果文件存在，则打开；否则，创建一个新文件。
  FA_OPEN_APPEND   与 FA_OPEN_ALWAYS 一样，除了文件读写指针放到了文件末尾。
* Revision History :
* Date          Author        Version        Notes
  2016-08-01    huanglong     V1.0           创建
------------------------------------------------------------------------------*/
#include "system.h"
#include "ff.h"
#include <string.h>

/*
=======================================
    获取文件大小
=======================================
*/
DWORD get_size(char *path)
{
  FIL fp;
  DWORD size;
  if (f_open(&fp, path, FA_READ) == FR_OK) {
    size = f_size(&fp);
  } else {
    size = 0;
  }
  f_close(&fp);
  return size;
}

/*
=======================================
    得到磁盘剩余容量
    path   : 磁盘编号("0:" "1:")
    total  : 总容量    （单位KB）
    free   : 剩余容量  （单位KB）
    返回   : FR_OK 正常
             else  错误代码
=======================================
*/
FRESULT get_free(const TCHAR* path, int *total, int *free)
{
  FRESULT res;
  FATFS *fs;
  int fre_clust=0, fre_sect=0, tot_sect=0;

  /* 得到磁盘信息及空闲簇数量 */
  res = f_getfree(path, (DWORD*)&fre_clust, &fs);

  if (res == FR_OK) 
  {
    tot_sect = (fs->n_fatent-2)*fs->csize;  /* 得到总的扇区数 */
    fre_sect = fre_clust*fs->csize;         /* 得到空闲扇区数 */
#if _MAX_SS != 512                              /* 扇区大小不是512字节，则转换为512字节 */
    tot_sect *= fs->ssize/512;
    fre_sect *= fs->ssize/512;
#endif
    *total = tot_sect >> 1;     /* 单位为 KB */
    *free = fre_sect >> 1;      /* 单位为 KB */
  }

  return res;
}

/*
=======================================
    遍历文件，中文名选 GBK 编码 CP936
=======================================
*/
FRESULT scan_files(const TCHAR* path)
{
  FRESULT res;
  char *fn;          /* This function is assuming non-Unicode cfg. */
  DIR dir;
  FILINFO fileinfo;  /* 文件信息 */

  /* 打开目录 */
  res = f_opendir(&dir, (const TCHAR*)path);
  if (res == FR_OK) {
    while(1)
    {   /* 读取目录下的一个文件 */
      res = f_readdir(&dir, &fileinfo);
      if (res != FR_OK || fileinfo.fname[0] == 0) {
        break;     /* 错误了/到末尾了，退出 */
      }
      if (fileinfo.fname[0] == '.') {
        continue;  /* 忽略上级目录 */
      }
      fn = fileinfo.fname;
      dbprintf("<FILE> %10d    %s/%s\r\n", get_size(fn), (char *)path, (char *)fn);
    }
  }

  return res;
}
