# STM32 固件（Keil MDK5）

芯片：**STM32F103C8T6**（Cortex-M3，72 MHz，64 KB Flash / 20 KB RAM）
库：**STM32F10x 标准外设库**（与课程/常见教程一致）
工具：Keil MDK5 + ST-Link V2

## 引脚分配

**本表以仓库 [`hardware/硬件接线说明.md`](../../hardware/硬件接线说明.md) 为准**，代码已与之对齐。

| 功能 | 引脚 | 说明 |
|---|---|---|
| E18 入口探头 A | **PB12** | 上拉输入；无遮挡 3.3V、有遮挡 0V |
| E18 入口探头 B | **PB13** | 同上 |
| E18 队列 50% 探头 | **PB14** | 同上 |
| E18 队列 100% 探头 | **PB15** | 同上 |
| DS3231 SCL | **PB6** | I²C1，复用开漏 |
| DS3231 SDA | **PB7** | I²C1 |
| SD 卡 CS | **PA4** | 软件片选 |
| SD 卡 SCK | **PA5** | SPI1 |
| SD 卡 MISO | **PA6** | SPI1 |
| SD 卡 MOSI | **PA7** | SPI1 |
| → ESP32 TX | **PA2** | USART2_TX → ESP32 GPIO16(RX2) |
| ← ESP32 RX | **PA3** | USART2_RX ← ESP32 GPIO17(TX2) |
| 调试 TX（可选） | **PA9** | USART1_TX → USB-TTL(CH340) |
| 调试 RX（可选） | **PA10** | USART1_RX ← USB-TTL(CH340) |
| 供电 | 5V / GND | 来自 ESP32 的 VIN |
| 外设 3.3V | 3V3 | 供 DS3231 与四路 10k 上拉 |
| ST-Link | PA13 / PA14 | SWDIO / SWCLK（3.3V 脚不接） |

> **USART2 已被 ESP32 占用**，所以调试口改用空闲的 USART1。若手头没有 CH340，把 `app_config.h` 里 `APP_DEBUG_UART_ENABLE` 置 0 即可，不影响采集与落盘。

> 四路探头**不用 EXTI**：采用 SysTick 1 kHz 定时采样 + 5 点移位寄存器去抖。轮询时序确定、抗噪更好，也与"下降沿=开始遮挡"的电平语义一致。

### 接线要点

1. **E18-D80NK 黑线各串 10 kΩ 上拉到 3.3 V**（不是 5 V）。棕=VCC(5V)、蓝=GND、黑=OUT，NPN 常开、低电平有效。
2. **全部共地**；ESP32 与 STM32 都是 3.3 V 逻辑，UART 直连即可（TX→RX 交叉）。
3. **供电顺序**：充电宝 → ESP32 Type-C → ESP32 的 **VIN** 分出 5V 给 STM32 / E18×4 / SD 卡座；STM32 的 **3V3** 供 DS3231 与上拉。
   **不要插 STM32 的 Type-C**，**不要把两块板的 3V3 并联**。
4. SD 卡座：`+5` 接 VIN，`+3.3` **悬空**（模块自带稳压）。
5. DS3231：电池为 CR2032 时拆掉 R5（充电电阻），或改用 LIR2032，否则有鼓包风险。
6. E18 探头引线尽量 ≤2 m；更长时用双绞线，并在 MCU 端加 100 nF 去耦。
7. 探头离地约 100 cm；入口双束沿**行进方向**错开约 10 cm（否则无法判方向）。

## 直接打开工程（推荐）

工程文件已生成，并已用 Keil UV4 命令行实测编译通过：

```
firmware/stm32/Project/queue_monitor.uvprojx
```

双击用 Keil 打开 → **F7 编译** → **F8 下载**。

实测构建结果（ARMCC 5.06 update 5，MDK 5.24）：

```
Program Size: Code=5092  RO-data=268  RW-data=64  ZI-data=1984
0 Error(s), 0 Warning(s)
```

即 **Flash 约 5.3 KB / 64 KB，RAM 约 2 KB / 20 KB**，余量充足（也远低于 MDK-Lite 的 32 KB 限制）。

工程已配置好以下项，无需手动改：

| 配置 | 值 |
|---|---|
| 器件 | `STM32F103C8` |
| 预处理宏 | `USE_STDPERIPH_DRIVER, STM32F10X_MD` |
| C 标准 | C99（`<uC99>1</uC99>`，代码里用了 for 循环内声明） |
| 包含路径 | `..\Start;..\Library;..\App;..\BSP;..\..\core` |
| 输出 | `Objects\queue_monitor.axf` + **HEX**（`CreateHexFile=1`） |

### 下载设置（重要）

**调试器与 Flash 下载设置存在 `.uvoptx` 里，不在 `.uvprojx` 里。** 只给 `.uvprojx` 时 Keil 会退回默认调试器，一按下载就报：

```
Error: Flash Download failed - "Cortex-M3"
```

仓库已随工程提供 `Project/queue_monitor.uvoptx`，其调试配置照搬自你本地能正常烧录的 `3-1 LED闪烁` 工程：

| 项 | 值 |
|---|---|
| 调试器 | ST-Link Debugger（`nTsel=5`） |
| 驱动 DLL | `STLink\ST-LINKIII-KEIL_SWO.dll` |
| 接口 / 时钟 | SWD / 10 MHz |
| 下载算法 | `STM32F10x_128.FLM`（起始 `0x08000000`） |
| 烧后动作 | Reset and Run |

> 若打开工程时 Keil 提示选项文件有问题（不同机器/不同 Keil 版本可能出现），**直接删掉 `.uvoptx` 按下面 5 步手动设置一次**，Keil 会重新生成：
>
> 1. Options for Target → **Debug** → 右侧 `Use:` 选 **ST-Link Debugger**
> 2. 点右侧 **Settings** → `Port` 选 **SW**，确认 `SW Device` 出现 `IDCODE`
> 3. 切到 **Flash Download** 页 → `Programming Algorithm` 里应有 **STM32F10x Med-density Flash**（没有就点 Add 添加）
> 4. 勾 **Reset and Run**
> 5. OK → F7 编译 → F8 下载
>
> 山寨 ST-Link 若连不上，把 Settings 里 `Max Clock` 降到 **1 MHz**，并把 `Connect` 改为 **under Reset**。

> 标准库与启动文件**已随仓库提供**（`Start/`、`Library/`、`App/stm32f10x_conf.h`），来源为 ST 官方 StdPeriph 库，无需另行下载。
> `stm32f10x_conf.h` 已裁剪为只启用 GPIO / I2C / RCC / SPI / USART，缩小编译面。

## 若要从零重建工程

在 `Project` 里新建分组，按下面归类加入：

| 分组 | 文件 |
|---|---|
| `Core`（`../../core`） | `gate_counter.c`、`queue_grade.c`、`protocol.c` |
| `App` | `app_main.c`、`data_logger.c` |
| `BSP` | `bsp_e18.c`、`bsp_ds3231.c`、`bsp_sdcard.c`、`bsp_uart.c` |
| `Library`（标准库） | `stm32f10x_gpio.c`、`_rcc.c`、`_i2c.c`、`_spi.c`、`_usart.c`、`_misc.c` |
| `FatFs` | `ff.c`、`diskio.c`（自行加入并实现 SPI 底层） |
| `Start` | `startup_stm32f10x_md.s`、`system_stm32f10x.c`、`core_cm3.c` |

**包含路径**（Options → C/C++ → Include Paths）：

```
..\..\core
..\App
..\BSP
..\Library
..\Start
..\Middlewares\FatFs
```

**宏定义**（Options → C/C++ → Define）：

```
USE_STDPERIPH_DRIVER, STM32F10X_MD
```

**其他设置**：

- Options → Output → 勾选 **Create HEX File**
- Options → Target → 勾选 **Use MicroLIB**（FatFs 与 printf 更省 Flash）
- `stm32f10x_conf.h` 里取消注释 `#include "stm32f10x_i2c.h"`、`stm32f10x_spi.h`、`stm32f10x_usart.h`

## 待补的硬件底函

骨架已给，接板子时补这三处：

| 文件 | 待补内容 |
|---|---|
| `BSP/bsp_ds3231.c` | `i2c_write_reg()` / `i2c_read_regs()`：标准库 I2C1 的 START/地址/读写/STOP 时序 |
| `BSP/bsp_sdcard.c` | FatFs 的 `diskio.c`：SPI1 读写 SD 卡、`disk_initialize/status/read/write/ioctl` |
| `BSP/bsp_uart.c` | `bsp_uart_link_ok()`：接收 ESP32 心跳字符做链路判断（可选） |

> F103 的硬件 I²C 有已知 errata，若总线锁死，可把 DS3231 改为软件模拟 I²C（SD 卡走 SPI，互不影响）。

## 烧录

### ST-Link + SWD（首选）

| ST-Link | 开发板 |
|---|---|
| 3.3V | 3.3V |
| GND | GND |
| SWDIO | **PA13** |
| SWCLK | **PA14** |
| RST（可选） | NRST |

Keil 中：Options → Debug → 选 **ST-Link Debugger** → Settings → Port 选 **SW**，确认 SW Device 出现 IDCODE → Flash Download 勾 **Reset and Run** → 点 **LOAD**（F8）。

### 串口 ISP（无 ST-Link 时）

1. USB-TTL：TX→PA10、RX→PA9、GND 共地（与调试口共用，两者都是 USART1）
2. **BOOT0 接 1（3.3V）**、BOOT1 接 0，按复位
3. 用 FlyMcu / STM32CubeProgrammer 选串口 + 加载 `.hex` → 下载
4. **BOOT0 拨回 0**，复位运行

> ESP32 接在 USART2（PA2/PA3），与 USART1 的 ISP 烧录**不再冲突**，无需拔线。

### 常见报错

| 报错 | 处理 |
|---|---|
| `No ST-LINK detected` | 装驱动 / 换 USB 口 / 换线 |
| `Cannot access target` | Debug → Settings → Connect 改 **under Reset** |
| 第二次烧不进 | PA13/PA14 被配成普通 IO → 用 under Reset 连接 |
| 代码超 32 KB | 换 MDK-Community 授权（免费、无大小限制） |

## 上电自检

**调试口 USART1（PA9/PA10 + CH340，115200）**应看到：

```
[QSM] DS3231 not found, timestamps disabled    ← RTC 未接/未通时的提示
[QSM] SD card mounted
[QSM] init done
```

**USART2（PA2/PA3 → ESP32）** 每秒输出一帧 `$QSM,...`。没有 ESP32 时，可临时把 USB-TTL 接到 PA2/PA3 直接看帧。

接线说明里的分级检查顺序（对应 `hardware/硬件接线说明.md` 第 4 节）：逐个接 E18，黑线无遮挡 3.3 V、有遮挡 0 V → 接 DS3231 并扫到 `0x68` → 接 SD 卡确认挂载 → 最后接 ESP32 看网页。
