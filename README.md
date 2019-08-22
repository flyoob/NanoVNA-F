# NanoVNA-F
NanoVNA-F, FreeRTOS version of [TTRFTECH](https://github.com/ttrftech)'s NanoVNA.

谍照如下/Spy Shot
![Spy Shot 1](/Img/NanoVNA-F_样品谍照1.JPG)

Created by STM32CubeMX 4.27.0 V1.0
* HAL Lib  ：STM32Cube_FW_F1_V1.6.1
* MDK Ver  ：uVision V5.23.0.0
* MDK Pack : ARM::CMSIS Ver: 5.2.0(2017-11-16)
* MDK Pack : Keil::STM32F1xx_DFP Ver: 2.3.0(2018-11-05)
* FatFs    ：R0.11 (February 09, 2015)
* CHIP
STM32F103VET6 FLASH: 512 KB, SRAM: 64 KB
* SPI Flash
W25Q128JVSIQTR

HAL Lib Path: C:/Users/S04/STM32Cube/Repository/STM32Cube_FW_F1_V1.6.1

After Build/Rebuild
RUN #1
D:\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin -o .\update.bin .\NanoVNA-F\NanoVNA-F.axf
