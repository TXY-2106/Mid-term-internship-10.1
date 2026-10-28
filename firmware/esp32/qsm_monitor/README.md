# ESP32 网关（Arduino IDE）

功能：接收 STM32 的 `$QSM` 帧 → 连 Wi-Fi → 托管网页仪表板。**不做任何计数判定**。

## 环境准备

1. Arduino IDE → 文件 → 首选项 → 附加开发板管理器网址，填（国内镜像，走 `dl.espressif.cn`）：

   ```
   https://arduino.me/packages/esp32.json
   ```

2. 工具 → 开发板 → 开发板管理器 → 搜索 `esp32` → 安装 **esp32 by Espressif Systems `3.2.1-cn`**
3. 开发板选 **ESP32 Dev Module**，端口选 CH340 对应的 COM 口

> 镜像最高为 3.2.1，比官方 3.3.12 旧一个小版本，本项目功能不受影响。注意 ESP32 core **3.x 与 2.x 的部分 API 有差异**，查文档时看清版本。

## 打开与修改

1. 用 Arduino IDE 打开 `qsm_monitor/qsm_monitor.ino`（草图中所有 `.h/.cpp` 会一起编译）
2. 改 **`config.h`** 里两个必填项：

   ```c
   #define WIFI_SSID       "你的WiFi名称"
   #define WIFI_PASSWORD   "你的WiFi密码"
   ```

3. 上传

## 接线

依据仓库 [`hardware/硬件接线说明.md`](../../../hardware/硬件接线说明.md)：

| ESP32 | STM32 | 说明 |
|---|---|---|
| GPIO16 (U2RXD) | **PA2** (USART2_TX) | STM32 → ESP32 |
| GPIO17 (U2TXD) | **PA3** (USART2_RX) | ESP32 → STM32 |
| GND | GND | **必须共地** |
| Type-C | — | 充电宝供电（USB-A 转 Type-C） |
| VIN | 5V | 由 ESP32 的 VIN 分出 5V 给 STM32 / E18×4 / SD 卡座 |

> **不要**把 ESP32 的 3V3 与 STM32 的 3V3 并联；**不要**给 STM32 插 Type-C。
> ESP32 发射瞬时电流可达 300 mA，务必由充电宝独立供电并在模块旁加 470 µF 电容，否则会复位。

## 使用

上传后打开串口监视器（115200）看到：

```
[QSM] gateway booting
[WiFi] connected, IP = 192.168.x.x
[QSM] http server started on port 80
```

浏览器访问该 IP 即可看到仪表板：三级状态色块、在场人数、累计进出、两路探头遮挡时长、最近 2 分钟趋势条、拥挤告警。

## HTTP 接口

| 路径 | 说明 |
|---|---|
| `/` | 仪表板网页（内嵌，无外部依赖，断网可用） |
| `/api/state` | 最新状态 JSON，含链路统计与告警阈值 |

`/api/state` 示例：

```json
{"valid":true,"seq":1024,"ms":3735000,"datetime":"2026-10-28T12:31:04",
 "in":37,"out":29,"occ":8,"q50":6100,"q100":0,"grade":2,"flags":3,
 "good":1024,"bad":0,"age":450,"alert_grade":2,"alert_occ":8}
```

`good` / `bad` / `age` 三个字段是现场排障用的：`bad` 持续增长说明串口干扰或两端协议不一致，`age` 过大说明 STM32 掉线或没在发。

## 文件说明

| 文件 | 职责 |
|---|---|
| `qsm_monitor.ino` | 入口：初始化、主循环、Wi-Fi 后台重连 |
| `config.h` | Wi-Fi、串口引脚、历史长度、告警阈值 |
| `serial_link.cpp/.h` | 帧解析 + CRC8 校验（须与 `firmware/core/protocol.c` 一致） |
| `web_server.cpp/.h` | HTTP 路由与 JSON 序列化 |
| `web_page.h` | 内嵌 HTML/CSS/JS（单页，1 s 轮询） |

## 注意

- **CRC 算法两端必须一致**：改 `firmware/core/protocol.c` 的 `qsm_crc8` 时，必须同步改 `serial_link.cpp` 的同名函数。
- 网页用 1 s 轮询而非 WebSocket/SSE，是为了让整页逻辑足够短、便于现场排障；A3 要求 ≤5 s，余量充足。
- `config.h` 里的 `HISTORY_SIZE` 目前未用于服务端历史（趋势条在前端生成）；若要在服务端存 1 小时历史，需要再加环形缓冲与 `/api/history` 接口。
