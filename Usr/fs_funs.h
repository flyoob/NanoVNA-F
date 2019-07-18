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
#ifndef __FS_FUNS_H
#define __FS_FUNS_H

#include "ff.h"

// 得到磁盘总容量和剩余容量
extern  FRESULT get_free(const TCHAR* path, int *total, int *free);
// 扫描指定目录
extern  FRESULT scan_files(const TCHAR* path);

#endif


