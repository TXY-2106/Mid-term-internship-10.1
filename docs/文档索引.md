<div align=center>

**中文** ｜ [English](../README.en.md) ｜ [技术方案](proposal.md)

</div>

# 文档索引 ｜ Documentation Index

> 本目录存放三类内容：**项目自写文档**、**参考文献**、**器件手册**。
> 命名约定：`<器件>_<文档类型>_<语言>.pdf`

## 1. 项目文档 ｜ Project documents

| 文件 | 语言 | 说明 |
| --- | --- | --- |
| [proposal.md](proposal.md) | English | 技术方案正式版，用于提交与设计关口评审 |
| [proposal.zh.md](proposal.zh.md) | 中文 | 技术方案中文版，与英文版章节一一对应 |

两版结构一致（8 节）：引言 / 目标与范围 / 相关工作 / 技术方案 / 评估计划 / 风险与缓解 / 进度计划 / 参考文献。

## 2. 参考文献 ｜ Reference papers（`papers/`）

| 文件 | 编号 | 内容 |
| --- | --- | --- |
| [s40313-022-00916-z.pdf](papers/s40313-022-00916-z.pdf) | **[1]** | dos Santos 等，ESP32 + E18-D80NK 双向人数计数器；餐厅闸机 98.65%、公交车门 91.45%。**本方案的方法基础**（双束 300 ms 符合规则、方向判别、误差分类） |
| [LOADS_LiDAR-based_Privacy-Preserving_Queue_Monitoring_and_Analysis.pdf](papers/LOADS_LiDAR-based_Privacy-Preserving_Queue_Monitoring_and_Analysis.pdf) | [2] | LOADS：单 LiDAR + HDBSCAN 的隐私保护排队监测（IEEE PerCom Workshops 2025） |
| [Monitoring_Social_Distancing_in_Queues_Using_Infrared_Array_Sensor.pdf](papers/Monitoring_Social_Distancing_in_Queues_Using_Infrared_Array_Sensor.pdf) | [3] | Rezzouki 等，MLX90640 红外阵列监测队列；6 人 / 93%（IEEE Sensors J. 2022） |
| [Privacy-Driven Network Data for Smart Cities.pdf](papers/Privacy-Driven%20Network%20Data%20for%20Smart%20Cities.pdf) | [4] | 智慧城市 Wi-Fi 数据匿名化；用于论证"匿名化亦有实质代价" |
| [Crowd_Monitoring_Using_Mobile_Phones.pdf](papers/Crowd_Monitoring_Using_Mobile_Phones.pdf) | — | 背景文献，正文未引用：手机 Wi-Fi / 蓝牙人群计数 |
| [People_Counting_and_Positioning_Using_Low-Resolution_Infrared_Images_for_Resource-Constrained_Edge_Devices.pdf](papers/People_Counting_and_Positioning_Using_Low-Resolution_Infrared_Images_for_Resource-Constrained_Edge_Devices.pdf) | — | 背景文献，正文未引用：32×32 热成像 + 边缘推理的精度上限 |

## 3. 器件手册 ｜ Datasheets（`datasheets/`）

| 器件 | 文档类型 | 中文 | 英文 |
| --- | --- | --- | --- |
| **E18-D80NK**（核心传感器） | 数据手册 | [cn](datasheets/E18-D80NK_datasheet_cn.pdf) | [en](datasheets/E18-D80NK_datasheet_en.pdf) |
| **STM32F103C8T6**（主控） | 参考手册 | [cn](datasheets/STM32F10xxx_technical_reference_manual_cn.pdf) | [en](datasheets/STM32F10xxx_technical_reference_manual_en.pdf) |
| ESP32-WROOM-32（联网） | 数据手册 | [cn](datasheets/esp32-wroom-32_datasheet_cn.pdf) | [en](datasheets/esp32-wroom-32_datasheet_en.pdf) |
| ESP32（联网） | 技术参考手册 | [cn](datasheets/esp32_technical_reference_manual_cn.pdf) | [en](datasheets/esp32_technical_reference_manual_en.pdf) |

## 4. 维护约定 ｜ Maintenance notes

- 新增或更新文献后，请同步更新 `proposal.md` / `proposal.zh.md` 的参考文献编号，并补充本索引第 2 节
- 本目录下器件手册合计约 54 MB。若仓库体积成为负担，可改用 Git LFS，或只在索引中保留厂商来源链接
- 尚未纳入的器件（DS3231 RTC、SSD1306 OLED、microSD 模块）如需在报告附录引用参数，建议补入手册并登记于此
