# NanoVNA-F
NanoVNA-F, NanoVNA-Fairy, FreeRTOS version of [edy555](https://github.com/ttrftech/NanoVNA)'s NanoVNA.  
"VNA" means: 矢量网络分析仪器 / Vector Network Analyzer  

<font color=#FF0000 >New firmware support 10k ~ 1.5GHz released</font>, Click [deepelec.com](https://deepelec.com) to get latest info.

### 项目描述 / Project Description
NanoVNA-F is a product made by BH5HNU based on the Open Source Project of NanoVNA(https://ttrf.tk/kit/nanovna/).
Thanks to [hugen](https://github.com/hugen79/NanoVNA-H)'s creative idea to use harmonics output by Si5351, we designed NanoVNA-F to **expand the measurement range to 1GHz**, where S11 still has 40dB dynamic range at 1GHz.
![1](https://s2.imgsha.com/2020/01/01/VNA-F-ALI.png)  
![1](https://s2.imgsha.com/2020/03/26/DSC01292_1.jpg)  

更多性能和指标测试信息，请访问：[TaoBao](https://hamelec.taobao.com)唯一官方店铺  
For more infomation about performance parameters & details, please visit:[NanoVNA-F product page](https://www.aliexpress.com/item/4000402236126.html)  

### 产品功能 / Product Features
NanoVNA-F can measure S parameters, Voltage Standing Wave Ratio (SWR), Phase, Group Delay, Smith chart and more  
![2](https://s2.imgsha.com/2019/12/14/VNA-F_UHF.png)  
![2.1](https://s1.imgsha.com/2019/12/03/rf_demo_kit_circuit_1.png)  
![2.2](https://s1.imgsha.com/2019/12/03/rf_demo_kit_circuit_11.png)  

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
如果你是中国用户，请到[TaoBao](https://hamelec.taobao.com)唯一官方店铺购买。  
You can get one on [AliExpress Deepelec Store](https://deepelec.aliexpress.com) and we support shipping to most parts of the world.  
[NanoVNA-F 快速入门指南（中文）](http://www.deepelec.com/files/NanoVNA-F_Menu_Structure_Quick_start_guide_v2.0_zh-cn.pdf)  
[NanoVNA-F Quick start guide (English)](http://www.deepelec.com/files/NanoVNA-F_Menu_Structure_Quick_start_guide_v2.0.pdf)  

### 固件更新 / Firmware update
https://github.com/flyoob/NanoVNA-F/releases
https://groups.io/g/nanovna-f/wiki/How-do-I-upgrade-the-firmware

如何显示呼号 / How to display your call sign
1. 使用 Type-C 将设备连接到 USB，进入 Bootloader 模式。 / Connect the device to USB using Type-C, go into Bootloader Mode.  
![v0.0.4_1](https://s1.imgsha.com/2019/10/30/v0.0.4_3.png)
2. 将你的 callsign.txt 文件放入 U盘，重新上下电后自动显示。 / Copy your **null terminated** callsign.txt onto Udisk, Re-power the device.  
![v0.0.4_2](https://s2.imgsha.com/2020/02/03/shot_logo1.png)
3. example callsign.txt content created in bash (`<ctrl-d>` means Control+D keys)
```
$ cat > callsign.txt
BH5HNU<ctrl-d><ctrl-d>
$ od -tc -Ax callsign.txt
000000   B   H   5   H   N   U
000006
```
### 后续计划 / Follow up plan
* 跟进原作者 NanoVNA 项目更新，BUG修复 / Follow up the original NanoVNA project update, BUG fix
* 对数扫频 / Logarithmic frequency sweep

### PC控制软件 / PC control software
[NanoVNASharp v1.0.3](http://www.deepelec.com/files/NanoVNASharp.zip)(by Hugen)  
![4](https://s1.imgsha.com/2019/10/07/NanoVNASharp.jpg)

[nanoVNA_mod_v2](http://www.deepelec.com/files/nanoVNA_mod_v2.zip)(by alex_m)  
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
