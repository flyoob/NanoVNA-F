/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
 * NOTE:  This file uses a third party USB CDC driver.
 */

/* Standard includes. */
#include "string.h"
#include "stdio.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Example includes. */
#include "FreeRTOS_CLI.h"

/* Demo application includes. */
#include "serial.h"
#include "system.h"

/* Dimensions the buffer into which input characters are placed. */
#define cmdMAX_INPUT_SIZE           48

/* Dimentions a buffer to be used by the UART driver, if the UART driver uses a
buffer at all. */
#define cmdQUEUE_LENGTH             64

/* DEL acts as a backspace. */
#define cmdASCII_DEL               (0x7F)

/* The maximum time to wait for the mutex that guards the UART to become
available. */
#define cmdMAX_MUTEX_WAIT           pdMS_TO_TICKS( 300 )

#ifndef configCLI_BAUD_RATE
    #define configCLI_BAUD_RATE     115200
#endif

// 清除屏幕
#define CLEAR()             dbprintf("\033[2J")
// 上移光标  
#define MOVEUP(x)           dbprintf("\033[%dA", (x))
// 下移光标  
#define MOVEDOWN(x)         dbprintf("\033[%dB", (x))
// 左移光标  
#define MOVELEFT(y)         dbprintf("\033[%dD", (y))
// 右移光标  
#define MOVERIGHT(y)        dbprintf("\033[%dC",(y))
// 定位光标  
#define MOVETO(x,y)         dbprintf("\033[%d;%dH", (x), (y))
// 光标复位  
#define RESET_CURSOR()      dbprintf("\033[H")
// 隐藏光标  
#define HIDE_CURSOR()       dbprintf("\033[?25l")
// 显示光标  
#define SHOW_CURSOR()       dbprintf("\033[?25h")
// 反显
#define HIGHT_LIGHT()       dbprintf("\033[7m")
#define UN_HIGHT_LIGHT()    dbprintf("\033[27m")
// 清除一行
#define CLEAR_LINE()        dbprintf("\033[K")

/*-----------------------------------------------------------*/

/*
 * The task that implements the command console processing.
 */
void cmd_register( void );
/*-----------------------------------------------------------*/

/* Const messages output by the command console. */
// static const char * const pcWelcomeMessage = "FreeRTOS command server.\r\nType Help to view a list of registered commands.\r\n\r\n>";
static const char * const pcEndOfMsg = "nanovna-F> ";
static const char * const pcNewLine  = "\r\n";

/* Used to guard access to the UART in case messages are sent to the UART from
more than one task. */
static SemaphoreHandle_t xTxMutex = NULL;

/* The handle to the UART port, which is not used by all ports. */
static xComPortHandle xPort = 0;
/*-----------------------------------------------------------*/

void cmd_init(void)
{
  /* Create the semaphore used to access the UART Tx. */
  xTxMutex = xSemaphoreCreateMutex();
  configASSERT( xTxMutex );

  cmd_register();
}
/*-----------------------------------------------------------*/

void cmd_loop( void *pvParameters )
{
  signed char cRxedChar;
  uint8_t ucInputIndex = 0;
  char *pcOutStr;
  static char cInputStr[ cmdMAX_INPUT_SIZE ], cLastInputStr[ cmdMAX_INPUT_SIZE ];
  xComPortHandle xPort;

  ( void ) pvParameters;

  /* Obtain the address of the output buffer.  Note there is no mutual
  exclusion on this buffer as it is assumed only one command console interface
  will be used at any one time. */
  pcOutStr = FreeRTOS_CLIGetOutputBuffer();

  /* Initialise the UART. */
  xPort = xSerialPortInitMinimal( configCLI_BAUD_RATE, cmdQUEUE_LENGTH );

  /* Send the welcome message. */
  // vSerialPutString( xPort, ( signed char * ) pcWelcomeMessage, ( unsigned short ) strlen( pcWelcomeMessage ) );

  for( ;; )
  {
    /* Wait for the next character.  The while loop is used in case
    INCLUDE_vTaskSuspend is not set to 1 - in which case portMAX_DELAY will
    be a genuine block time rather than an infinite block time. */
/*
阻塞超时时间。
（1）如果在接收时队列为空，则这个时间是任务处于阻塞状态以等待队列数据有效的最长等待时间。
（2）如果xTicksToWait 设为0，并且队列为空，则xQueueRecieve()与xQueuePeek()均会立即返回。
     阻塞时间是以系统心跳周期为单位的，所以绝对时间取决于系统心跳频率。
     常量portTICK_RATE_MS 可以用来把心跳时间单位转换为毫秒时间单位。
（3）如果把xTicksToWait 设置为portMAX_DELAY，
     并且在FreeRTOSConig.h 中设定INCLUDE_vTaskSuspend 为1，那么阻塞等待将没有超时限制。
*/
    while( xSerialGetChar( xPort, &cRxedChar, portMAX_DELAY ) != pdTRUE );

    /* Ensure exclusive access to the UART Tx. */
    if( xSemaphoreTake( xTxMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )
    {
      /* Echo the character back. */
      // if( cRxedChar != '\r' && cRxedChar != '\n')
          // xSerialPutChar( xPort, cRxedChar, portMAX_DELAY );

      /* Was it the end of the line? */
      if( cRxedChar == '\n' || cRxedChar == '\r' )
      {
        /* Just to space the output from the input. */
        vSerialPutString( xPort, ( signed char * ) pcNewLine, ( unsigned short ) strlen( pcNewLine ) );

        /* See if the command is empty, indicating that the last command
        is to be executed again. */
        if( ucInputIndex > 0)
        {
          /* Get the next output string from the command interpreter. */
          FreeRTOS_CLIProcessCommand( cInputStr, pcOutStr, config_MAX_OUTPUT_SIZE );

          /* Write the generated string to the UART. */
          /* 命令执行结果放入缓存，在这里打印 */
          #if 0  // 改为直接在命令里打印
          vSerialPutString( xPort, ( signed char * ) pcOutStr, ( unsigned short ) strlen( pcOutStr ) );
          #endif

          /* All the strings generated by the input command have been
          sent.  Clear the input string ready to receive the next command.
          Remember the command that was just processed first in case it is
          to be processed again. */
          strcpy( cLastInputStr, cInputStr );
          ucInputIndex = 0;
          memset( cInputStr, 0x00, cmdMAX_INPUT_SIZE );
        } else {
          /* Copy the last command back into the input string. */
          // strcpy( cInputStr, cLastInputStr );  // 不执行上一次命令
        }

        /* 清空队列 */
        xSerialReset( xPort );

        vSerialPutString( xPort, ( signed char * ) pcEndOfMsg, ( unsigned short ) strlen( pcEndOfMsg ) );
      }
      else
      {
        if( cRxedChar == '\b' )
        {
          /* Backspace was pressed.  Erase the last character in the
          string - if any. */
          if( ucInputIndex > 0 )
          {
            ucInputIndex--;
            cInputStr[ ucInputIndex ] = '\0';
            /* 删除光标左边的一个字符 */
            MOVELEFT(1);
            CLEAR_LINE();
          }
        }
        else
        {
          /* A character was entered.  Add it to the string entered so
          far.  When a \n is entered the complete    string will be
          passed to the command interpreter. */
          if( ( cRxedChar >= ' ' ) && ( cRxedChar < '~' ) )
          {
            if( ucInputIndex < cmdMAX_INPUT_SIZE )
            {
              cInputStr[ ucInputIndex ] = cRxedChar;
              ucInputIndex++;
              /* 屏幕 ECHO */
              xSerialPutChar( xPort, cRxedChar, portMAX_DELAY );
            }
          }
        }
      }

      /* Must ensure to give the mutex back. */
      xSemaphoreGive( xTxMutex );
    }
  }
}
/*-----------------------------------------------------------*/

/* 外部使用，暂时不用 */
void vCommandPutString( const char * const pcMessage)
{
  if( xSemaphoreTake( xTxMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )
  {
    vSerialPutString( xPort, ( signed char * ) pcMessage, ( unsigned short ) strlen( pcMessage ) );
    xSemaphoreGive( xTxMutex );
  }
}
/*-----------------------------------------------------------*/

