<div align=center>

[中文](README.md) ｜ **English** ｜ [Technical Proposal](docs/proposal.md)

</div>

# Mid-term Internship 10.1 | Queue Status Monitor

> UL–LICHPU Smart Campus Living Lab · Student Project · 15 ECTS · 12-week delivery cycle
>
> Current stage: solution confirmed; procurement and prototype build underway (infrared detection + STM32 edge acquisition + ESP32 connectivity)

> Click a section title to expand it.

<details>
<summary><b>1. Project Overview</b></summary>

**Campus problem**

Service counters on campus — canteen pick-up, library borrowing and enquiry desks, student services, print stations, coffee counters — become crowded at peak times. Users cannot judge the waiting situation in advance, and service operators have no real-time view of demand distribution.

**Project tasks**

- Anonymously sense entry / exit flow, occupancy and queue-length level in the selected service area
- Determine the crowding status (idle / normal / busy) from occupancy and queue level
- Publish the current status and historical trend in real time through a web dashboard, with threshold alerts
- Store raw counts locally and export them after each experiment for comparative analysis

**Technical approach (confirmed)**

| Stage | Approach |
| --- | --- |
| Sensing | 4 × E18-D80NK infrared photoelectric switches: 2 mounted on either side of the passage to determine entry / exit by trigger order; 2 placed along the queue at roughly 50% and 100% positions as queue-length threshold detectors. No cameras, no visible-light images |
| Edge acquisition | STM32F103C8T6: 1 kHz timed sampling, entry / exit counting, queue-level classification, DS3231 offline timestamps, SD card raw logs |
| Connectivity and display | ESP32 (CH340): receives data over UART, reports over Wi-Fi and hosts the web dashboard |
| Power | 5V/3A |

**Data flow**

```
E18 entry/exit ×2 ─┐
                   ├─► STM32 ──SPI──► SD card (raw logs)
E18 queue ×2 ──────┘      │
                          ├──I²C──► DS3231 (offline timestamps)
                          └──UART──► ESP32 ──Wi-Fi──► web dashboard
```

**Queue level rules**

| State | Condition |
| --- | --- |
| Low | 50% sensor not continuously blocked |
| About half | 50% sensor blocked continuously for ≥ 5 s, 100% sensor not blocked |
| Full | 100% sensor blocked continuously for ≥ 5 s |

- **Continuous blocking** distinguishes “standing in the queue” from “walking past”: fast-moving crowds produce only short pulses and do not trigger a level.
- **Hysteresis**: escalation takes effect immediately; de-escalation requires 15 s without blockage to avoid state flicker.
- **Cross-check**: compared with the occupancy count from entry / exit counting; “Full” is never reported when occupancy is too low.

**Design trade-off**: Acquisition and connectivity are split across two boards. A Wi-Fi reconnect or reset on the ESP32 does not interrupt STM32 acquisition or SD logging, keeping experimental data continuous; when moving to another site, only the gateway needs replacing and the acquisition firmware stays unchanged.

**Scope**

Queue length in this phase is expressed as a **three-level discrete classification** (low / about half / full); continuous counts or waiting minutes are not provided. Continuous length estimation and a waiting-time model are left for later iterations.

**Users and beneficiaries**

- Direct users: students and staff waiting for service — less blind waiting
- Beneficiaries: service-point operators — demand visibility for staffing and queue guidance

**Core deliverables**: working prototype (acquisition + storage + wireless reporting) + web dashboard (real-time status / historical trend / alerting) + verification evidence against manual counting

</details>

<details>
<summary><b>2. Acceptance Criteria</b></summary>

| ID | Item | Criterion | Verification method |
| --- | --- | --- | --- |
| A1 | Queue-length level | Three-level (low / about half / full) agreement with manual observation ≥ 80% | Same-period comparison, ≥ 5 repeats, all three states covered |
| A2 | Crowding-status decision | Combines occupancy, entry / exit flow and queue level; ≥ 80% accuracy against manual observation | Cover idle / normal / peak scenarios |
| A3 | End-to-end latency | ≤ 5 s from on-site change to web update | Compare a stopwatch with system log timestamps |
| A4 | Stability | ≥ 4 h continuous operation without crashes or data loss | Long-run log and count-continuity check |
| A5 | Privacy compliance | No visible-light camera, no face or identity recognition; aggregate counts only | Design and data review |
| A6 | Integrated prototype | At least 2 meaningful data inputs (entry / exit flow, queue thresholds), working end to end | Live demonstration |
| A7 | Test evidence | Unit / integration / system tests traced to requirements, documented | Test records and exported count datasets |
| A8 | Dashboard | Real-time status + historical trend + alerting | Live demonstration (web) |
| A9 | Cross-campus replication | Differences and feasibility of deployment at UL and LICHPU explained | Report chapter |

> These thresholds must be confirmed with the module tutor and frozen before the requirements gate, then refined into testable requirement items.

</details>

<details>
<summary><b>3. Cost Control</b></summary>

> Estimated China retail prices (JD / Taobao, October 2026), RMB, tax included; excludes shipping, installation labour and maintenance. Prices, VAT and logistics in the UL / LICHPU locations have not been assessed.

| Item | Qty | Unit (¥) | Subtotal (¥) |
| --- | --- | --- | --- |
| STM32F103C8T6 minimum system board | 1 | 15–25 | 15–25 |
| E18-D80NK infrared photoelectric switch | 4 | 7 | 28 |
| SD card slot module (full-size) | 1 | 5 | 5 |
| SD card 32 GB (owned) | 1 | 100–150 | — (excluded) |
| ESP32 development board (CH340) | 1 | 21 | 21 |
| DS3231 RTC module | 1 | 14 | 14 |
| 5V/3A power supply | 1 | 12 | 12 |
| Perfboard / wiring / enclosure | 1 set | 25–35 | 25–35 |
| **Single-node total** | | | **approx. 120–140** |

> Note: the 32 GB SD card is owned; its market price is shown for reference only and is excluded from the total.

- **One-off prototype**: ST-Link ¥12, breadboard and jumper wires ¥5, shipping ¥10–20; first unit approx. **¥150–180**.
- **Cost controls**: web dashboard replaces the local display; entry / exit sensors also provide occupancy counts; only two threshold sensors for the queue; DS3231 instead of GPS; owned 32 GB SD card (verified and formatted as FAT32), no purchase required; shared debugging tools.
- **Boundary**: hardware material cost only; excludes labour, network and maintenance; overseas costs not assessed.

### Storage capacity note

The 32 GB card is used because one was already available, not because that capacity is required. Based on approximately 1 MB per 4-hour experiment (~0.25 MB/hour):

| Operating period | Estimated data |
| --- | --- |
| 1 hour | ~0.25 MB |
| 1 day (10 h) | ~2.5 MB |
| 1 month (30 days × 10 h) | ~75 MB |
| 1 year (365 days × 10 h) | ~0.9 GB |

- **Recommended deployment capacity: 8 GB.** At 10 h/day, 8 GB holds about 9 years of data; 8 GB is SDHC with native FAT32, usable in FatFs out of the box.
- If 8 GB is hard to source, 16 GB is equally suitable; avoid 64 GB and above — SDXC/exFAT requires FF_FS_EXFAT, and the capacity far exceeds the need.
- Data should still be exported to a PC per experiment or per day; the card is a transfer buffer, not long-term storage.

</details>

<details>
<summary><b>4. Hardware</b></summary>

- [Datasheets](docs/datasheets/)
- [Hardware directory (wiring)](hardware/)

</details>

<details>
<summary><b>5. Documentation Index</b></summary>

- [Documentation index](docs/README.md)

</details>
