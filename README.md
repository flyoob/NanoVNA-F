# NanoVNA-F
NanoVNA-F, FreeRTOS version of [edy555](https://github.com/ttrftech/NanoVNA)'s NanoVNA.  
"VNA" means: 矢量网络分析仪器 / Vector Network Analyzer  

### 项目描述 / Project Description
NanoVNA-F is a product based on the Open Source Project of NanoVNA(https://ttrf.tk/kit/nanovna/).
Thanks to [hugen](https://github.com/hugen79/NanoVNA-H)'s creative idea of use the harmonic of Si5351, We build NanoVNA-F and `expand the measure frequency up to 1GHz`, which S11 still have 40dB dynamic range At 1GHz.  
![1](https://s1.imgsha.com/2019/10/07/NanoVNA-F_main_pic.jpg)  

NanoVNA-F can measure S parameters, Voltage Standing Wave Ratio(SWR), Phase, Group Delay, Smith chart and so on  
![2](https://s1.imgsha.com/2019/10/07/NanoVNA-F_rf_kit.jpg)  
![2.1](https://s1.imgsha.com/2019/10/07/NanoVNA-F_SWR.jpg)  

更多性能和指标测试信息，请访问：[TaoBao](https://hamelec.taobao.com/)  
For more infomation about performance parameter & details, please visit:[AliExpress hamelec Store](https://hamelec.aliexpress.com)  

NanoVNA-F hardware features include and are not limited to the following improvements:  
1. Use 4.3-inch IPS TFT LCD & resistive touch screen, with a larger view angle and can be seen clearly outdoor.  
2. Use large capacity 5000mAh/3.7V lithium battery, standby time is longer, and expand a USB interface. Usually, it can be used as a power bank when the instrument is not used.  
3. The Lipo Charing IC to changed to IP5306(with 2A high charing current) , make the charging time shorter.  
4. Use a larger and better operating level.  
5. Support Chinese and English menu switching;  
6. Upgrade the user program(firmware) by virtual U disk(16MB SPI Flash Memory Inside).  
7. Standard aluminium case to protect SMA head and reduce the interference of external electromagnetic wave to instrument.  
...  

![3](https://s1.imgsha.com/2019/10/07/NanoVNA-F_block_diagram.png)  

### 如何购买 / Where to Buy
如果你是中国用户，请到[TaoBao](https://hamelec.taobao.com/)购买。  
You can get one on [AliExpress hamelec Store](https://hamelec.aliexpress.com) and we support shipping to most parts of the world.  
[NanoVNA-F 快速入门指南（中文）](http://www.sysjoint.com/files/NanoVNA-F_快速入门指南_Quick_start_guide_v1.1.pdf)  
[NanoVNA-F Quick start guide (English)](http://www.sysjoint.com/files/NanoVNA-F_Menu_Structure_Quick_start_guide_v1.0.pdf)  

### 固件更新 / Firmware update
[v0.0.2](http://www.sysjoint.com/files/NanoVNA-F_APP_v0.0.2.zip) 2019-08-29 : 首次发行 / First Release  

[v0.0.3](http://www.sysjoint.com/files/NanoVNA-F_APP_v0.0.3.zip) 2019-10-07 : 本次更新需要重新校准设备 / This update requires recalibration of the device  
* 支持NanoVNA上位机 / support NanoVNA's PC software;
* 加入开机提示音 / Add a boot tone;
* 优化低频段（<100MHz）S11测量结果 / Optimize band (<100MHz) S11 measurement results;  

[v0.0.4](http://www.sysjoint.com/files/NanoVNA-F_APP_v0.0.4.zip) 2019-10-30 : 本次更新无需重新校准设备 / This update does not require recalibration of the device  
* 呼号显示 / Can display call sign
* 修复 100MHz 频点不输出的 BUG / fix BUG: CW=100MHz do not output
* 优化中文翻译 / Optimize Chinese translation

[如何更新固件 / How to update the firmware](https://github.com/flyoob/NanoVNA-F_Boot)

如何显示呼号 / How to display your call sign
1. 使用 Type-C 将设备连接到 USB，进入 Bootloader 模式。 / Connect the device to USB using Type-C, go into Bootloader Mode.  
![v0.0.4_1](https://s1.imgsha.com/2019/10/30/v0.0.4_3.png)
2. 将你的 callsign.txt 文件放入 U盘，重新上下电后自动显示。 / Put you callsign.txt into Udisk, Re-power the device.  
![v0.0.4_2](https://s1.imgsha.com/2019/10/30/v0.0.4_1.jpg)

后续计划 / Follow up plan:
* 跟进原作者 NanoVNA 项目更新，BUG修复 / Follow up the original NanoVNA project update, BUG fix
* 对数扫频 / Logarithmic frequency sweep

### PC控制软件 / PC control software
[NanoVNASharp v1.0.3](http://www.sysjoint.com/files/NanoVNASharp.zip)(by Hugen)  
![4](https://s1.imgsha.com/2019/10/07/NanoVNASharp.jpg)

[nanoVNA_mod_v2](http://www.sysjoint.com/files/nanoVNA_mod_v2.zip)(by alex_m)  
![5](https://s1.imgsha.com/2019/10/07/nanoVNA_mod_v2.jpg)

[nanovna-saver](https://github.com/mihtjel/nanovna-saver/releases)(by Rune B. Broberg)  
![6](https://s1.imgsha.com/2019/10/07/nanovna-saver.v0.1.0.jpg)

### 相关网站 / Related website
* https://groups.io/g/nanovna-f The NanoVNA-F discussion group.You can ask any questions about NanoVNA-F here.
* https://groups.io/g/nanovna-users Users of nanovna small VNA, very popular forum.
* NanoVNA-F 网分/天分 交流QQ群：522796745

### MDK-ARM 工程编译 / Build by MDK-ARM
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
After Build/Rebuild RUN #1  
D:\Keil_v5\ARM\ARMCC\bin\fromelf.exe --bin -o .\update.bin .\NanoVNA-F\NanoVNA-F.axf  

### SW4STM32 工程编译 / Build by SW4STM32
待完善 / Coming soon
