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
| `Core`（`../../core`） | `gate_counter.c`、`queue_grade.c`、`protocol.c`、`qsm_history.c` |
| `App` | `app_main.c`、`data_logger.c`、`hist_store.c` |
| `BSP` | `bsp_e18.c`、`bsp_ds3231.c`、`bsp_sdcard.c`、`bsp_sd_disk.c`、`bsp_uart.c` |
| `Library`（标准库） | `stm32f10x_gpio.c`、`_rcc.c`、`_i2c.c`、`_spi.c`、`_usart.c`、`_misc.c` |
| `Middlewares` | `ff.c`、`diskio.c`（`Middlewares/FatFs`，已随仓库提供） |
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

## 硬件底函现状

原本留空的三处底函都已实现，代码与 `arm-none-eabi-gcc -Wall -Wextra` 检查均通过，**待上板实测**：

| 文件 | 内容 |
|---|---|
| `BSP/bsp_ds3231.c` | I²C1（PB6/PB7，100 kHz）读写寄存器：`i2c_write_reg()` / `i2c_read_regs()`，含 9 个 SCL 脉冲的总线恢复与 `I2C_SoftwareResetCmd` 兜底，探测寄存器 `0x0F` |
| `BSP/bsp_sd_disk.c` | SD 卡 SPI 协议：CMD0 / CMD8 / ACMD41 / CMD58 / CMD16 / CMD17 / CMD24，281 kHz 起步后提到 9 MHz，支持 SDHC 块寻址 |
| `App/hist_store.c` | 预测历史落盘：一天一个 `H<YYMMDD>.CSV`，上电回读最近 7 天 |
| `BSP/bsp_uart.c` | `bsp_uart_link_ok()` 的接收超时判断（可选，未做也不影响主流程） |

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

**USART2（PA2/PA3 → ESP32）** 每 250 ms 输出一帧 `$QSM,...`。没有 ESP32 时，可临时把 USB-TTL 接到 PA2/PA3 直接看帧。

接线说明里的分级检查顺序（对应 `hardware/硬件接线说明.md` 第 4 节）：逐个接 E18，黑线无遮挡 3.3 V、有遮挡 0 V → 接 DS3231 并扫到 `0x68` → 接 SD 卡确认挂载 → 最后接 ESP32 看网页。

## 历史人数预测与 SD 卡落盘

`App/app_config.h` 里的开关：

| 宏 | 默认 | 作用 |
|---|---|---|
| `APP_HISTORY_ENABLE` | `1u` | 1=每分钟记录一次在场人数并计算预测值 |
| `APP_TASK_HISTORY_MS` | `60000u` | 记录周期（60 s） |
| `APP_RTC_ENABLE` | `1u` | 1=启用 DS3231 真实时间（预测要靠它对齐时段） |
| `APP_SDCARD_ENABLE` | `1u` | 1=挂载 SD 卡并写 `status.csv` / `events.csv` |
| `APP_HIST_PERSIST` | `1u` | 1=把历史人数按天落 SD 卡，上电回读（0=只存 RAM，掉电清零） |

做法：把在场人数按 **15 分钟一段、存 7 天**放进 RAM（`qsm_history_t`，7×96 = 672 B），
预测值 = 过去 7 天同一时段的平均在场人数，随 4 Hz 上报帧的 `pred` / `pred_n` 两个字段发给 ESP32。
同一帧尾还会带 `beams`（四路探头电平位图），网页据此显示每支探头当前是否被遮挡，现场可用它逐支排查。

SD 卡上的历史文件：

```
H261109.CSV
HH:MM,occ
09:00,3
09:01,4
...
```

- 一天一个文件，8.3 短名；卡要求 **FAT16/FAT32、≤32 GB**（未开长文件名支持）。
- 每 60 s 追加一行 `HH:MM,occ`；写完立刻 `f_sync`（掉电最多丢一行）。
- 上电时读 `today-8 … today-1` 共 8 个文件，把历史灌回 RAM 表；今天不读，避免把当天重复计入。
- 文件不存在（第一次上电）是正常现象，串口会打印 `[QSM] no history file yet`。

注意事项：

- **必须开 RTC**：`APP_RTC_ENABLE = 1` 且 DS3231 正常，否则无法判断"同一时段"，预测恒为 0。
- **不含今天**：当天的数据不参与平均（当天没跑完会把均值拉低），所以**要连续跑满一个自然日**才有预测值。
- **一个时段存一个值**：同一 15 分钟内每分钟都会写，后写覆盖先写。
- 调试口每秒那行会多出预测值，形如 `L=0101 in=12 out=3 occ=9 pred=9/4`（`pred=` 预测人数 / 参考天数）。


## I2C 扫描（定位 DS3231 不识别）

`bsp_ds3231_scan()` 在上电初始化失败时、以及每 10 s 的重试失败时各跑一次，扫 0x08–0x77，
把结果压成 3 bit 塞进帧的 `i2c` 字段，网页"外设状态"卡片会直接给出结论：
总线空 → 查 VCC/GND/SDA/SCL/上拉；只发现 0x57 → 模块没响应或型号不对；
0x68 应答但读取失败 → 模块在线，检查寄存器读写时序与杜邦线接触。
串口同步打印 `[QSM] i2c scan: ...` 便于无网页时排查。

**扫描严格限时**：单次探测 0.3 ms 上限、整轮总预算约 15 ms（`I2C_PROBE_TO` / `I2C_SCAN_BUDGET`），
命中 0x68 立即返回。这样即使总线被拉死，也不会把主循环冻住——
早期版本用 `I2C_TIMEOUT` 逐个扫 112 个地址，总线异常时单轮要几十秒，
表现为“网页突然收不到任何数据”。

## 实时性、复位命令与稳定性

### 任务节拍（`App/app_config.h`）

| 宏 | 默认 | 说明 |
|---|---|---|
| `APP_TASK_FSM_MS` | `10u` | 状态机推进 + 事件出队 |
| `APP_TASK_REPORT_MS` | `250u` | 组帧发 ESP32（4 Hz，网页才“实时”） |
| `APP_TASK_STATUS_MS` | `1000u` | 写状态 CSV + 调试行（1 Hz，不频繁写卡） |
| `APP_TASK_LOGFLUSH_MS` | `10000u` | `f_sync` 落盘 |
| `APP_TASK_HISTORY_MS` | `60000u` | 历史记录与预测 |

### 网页复位（USART2 收命令，不用按复位键）

ESP32 下发一行 `$QSR,<cmd>*<CRC8>\n`：

| cmd | 动作 |
|---|---|
| 1 | `gate_init()` / `grade_init()` 归零，`in`/`out`/`occ`/`grade`/`pred` 清零 |
| 2 | `qsm_hist_init()` + `hist_store_clear()`，删掉近 8 天的 `H*.CSV` |
| 3 | 1 + 2 |

STM32 侧由 `USART2_IRQHandler` 收进 128 B 环形缓冲，主循环 `bsp_uart_poll_cmd()` 非阻塞取整行，
`qsm_cmd_parse()` 校验后执行；对应网页上的“清零计数 / 清空历史 / 全部复位”三个按钮。

### 稳定性改动（针对“必须手动复位才恢复”）

- `task_report()` 不再无条件读 DS3231：`APP_RTC_ENABLE=0` 或初始化失败时完全不碰 I2C，
  也就不会因为每次超时把主循环拖慢上百毫秒。
- I2C 连续 10 次读失败 → 自动重新初始化 I2C1（含 9 个 SCL 的总线恢复），
  串口会打印 `[QSM] DS3231 recovered`；总线被从机拉死时不再需要按复位键。
- **启动文件栈从 1 KB 加到 4 KB**（`Start/startup_stm32f10x_md.s` 的 `Stack_Size EQU 0x00001000`）：
  原先 1 KB 时 `task_report` 链已占 560 B，叠上 FatFs 的 `f_open/f_gets` 有 HardFault 风险。
- `bsp_uart.c` 的发送改为**带上限轮询**（TXE/TC 都有 guard），串口异常不会卡死主循环。
- 若仍出现 HardFault：用 Keil 的 `Register` 窗口看 `LR` 是否为 `0xFFFFFFF9`，
  或把 `APP_HIST_PERSIST` 临时置 0 缩小栈占用定位。
