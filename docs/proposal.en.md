<div align=center>

**English** ｜ [中文](proposal.zh.md)

</div>

# A Four-Beam Infrared Queue Monitor for Campus Service Counters

> UL–LICHPU Smart Campus Living Lab · Mid-term Internship 10.1 · 15 ECTS · 12-week delivery cycle
>
> Document type: technical proposal · Language: English ([中文版](proposal.zh.md))

## 1. Introduction

Service counters on campus — canteen food stalls, library enquiry desks, print stations and coffee counters — form queues at peak times. Users cannot judge the waiting situation before joining, and service-point operators have no real-time view of demand distribution. The result is avoidable congestion and poorly targeted staffing.

Camera-based crowd monitoring is technically effective but captures identifiable features. For a university deployment this is not acceptable, and anonymising network-level alternatives extracts a substantial utility cost: prior work shows that Wi-Fi session data must be degraded to hourly granularity with added noise before it can be shared [4].

This project takes a different route. Rather than sensing what people look like or which devices they carry, it senses **when they cross a boundary**. Four infrared photoelectric beams divide the work: two at the entrance are compared for trigger order to decide entry versus exit, and two along the queue lane sit at roughly the 50 % and 100 % positions as queue-length thresholds. The proposal is deliberately narrow: one well-defined queue geometry, one sensing principle, two low-cost development boards.

## 2. Objectives and Scope

| ID | Objective | Metric | Acceptance |
| --- | --- | --- | --- |
| O1 | Queue-length grading | Three-level grading (Few / About half / Full) agrees with manual observation ≥ 80 % of the time | A1 |
| O2 | Crowding-state decision | Combining occupancy and grade, accuracy against manual observation ≥ 80 % | A2 |
| O3 | End-to-end latency | Field change to web update ≤ 5 s (report every 250 ms, page polls every 400 ms) | A3 |
| O4 | Operational stability | ≥ 3 h continuous operation without crash or data loss | A4 |
| O5 | Privacy protection | No imaging device of any kind; aggregated counts only | A5 |
| O6 | Historical headcount prediction | Predict from the mean occupancy of the same 15-minute slot over the last 7 days, and report how many days contributed | A8 |
| O7 | Cost and replicability | Bill of materials below ¥150 per service point; UL vs LICHPU deployment differences documented | A9 |

**In scope**: anonymous entry/exit counting, occupancy, three-level queue grading and same-slot historical prediction for a single-file lane; long-term local SD-card logging; a live dashboard with status, trend, prediction and alerting; validation against a manual baseline.

**Out of scope**: continuous queue-length estimation and waiting-time in minutes (Little's-Law-style time models are deferred to a later iteration); personal identification; face or gait recognition; attendance or law-enforcement use; automated staffing decisions; sensing in open areas without a defined queue lane.

## 3. Related Work

**Camera-based crowd analysis** is the incumbent approach and the accuracy reference, but it is privacy-invasive and more expensive. **LiDAR-based queue monitoring** avoids imaging entirely by clustering point clouds (one cluster per person) [2]; it is privacy-friendly and light-independent, but a single 360° LiDAR dominates the bill of materials, and the published work is a demonstration without quantitative accuracy.

**Infrared array sensing** has been demonstrated in queues: an MLX90640 32×24 array on an STM32L476 detected up to six persons per queue with 93 % accuracy at sub-80 cm spacing [3]. This confirms that thermal sensing works in queues and also documents its density limit — resolution, not cost, becomes the binding constraint.

**Bidirectional infrared beam counting** is closest to this project. dos Santos et al. [1] built a counter from an ESP32 and **E18-D80NK** sensors with RTC and microSD logging, validated in two real deployments: a bus door (**91.45 %**) and a **university canteen turnstile (98.65 %)**. Their counting rule — **a crossing is counted only when both sensors trigger within a 300 ms window**, with direction given by the trigger order — removes the false triggers and directional ambiguity that plague single-beam counters. Their error breakdown also identifies **two people passing together** as the main residual error source.

**Gap.** Published queue-monitoring work reports either sensing performance in laboratory conditions or flow-counting accuracy at a gate; none reports, at a replicable bill of materials, **queue-grade agreement against a manual baseline** together with **same-slot prediction from historical data**. For a single-file lane, that combination is the contribution of this project.

## 4. Technical Approach

### 4.1 System overview

```
 Entrance A/B (PB12/PB13)     Queue 50 %/100 % (PB14/PB15)
        │ 1 kHz sampling, 5 ms debounce
        ▼
 STM32F103C8T6 edge node
   ├── direction FSM: 300 ms two-beam coincidence → in / out / occupancy
   ├── grading FSM: upgrade after 5 s of continuous blocking, downgrade after 15 s clear,
   │                 cross-checked against occupancy
   ├── history table: 7 days × 96 slots (15 min), completed slots only
   ├── I²C1 → DS3231 time base
   └── SPI1 → SD card: events.csv / status.csv / HYYMMDD.CSV
        │ UART 115200, one frame every 250 ms: $QSM,…*CRC8
        ▼
 ESP32 soft-AP "QSM-Monitor" at 192.168.4.1
        │ Wi-Fi
        ▼
 Phone / laptop dashboard: live status · grade trend · predicted headcount ·
                           four-beam state · reset buttons
        ▲
        └── downlink command $QSR,<cmd>*CRC8 (reset counters / clear history / reset all)
```

Sensing and networking are split across two boards: an ESP32 Wi-Fi reconnect or reset never interrupts STM32 sampling or card writes, and moving to another site only means swapping the gateway.

### 4.2 Queue zone and mounting geometry

The monitored zone is a single-file queue lane: 60–90 cm wide, up to 6 m long. At the entrance, two **E18-D80NK** sensors are spaced 10 cm apart along the walking direction at roughly 100 cm height and decide entry versus exit. Two more sit at about 50 % and 100 % (tail) of the lane, also at roughly 100 cm, for grading. All four are diffuse-reflective with a 3–80 cm adjustable range, calibrated one by one at installation.

With 10 cm spacing and a 1.2–1.4 m/s walking speed, the two beams are blocked about 70–85 ms apart — well above the 5 ms debounce and inside the 300 ms coincidence window.

### 4.3 Counting and direction

Following [1], **a crossing is counted only when both entrance beams trigger within 300 ms**; a single-beam trigger is discarded as a false event. Direction follows the trigger order: A then B is an entry, B then A an exit. The firmware samples at 1 kHz with a 5-sample shift-register debounce (5 ms equivalent) and applies a 2 s blocking timeout to discard gestures that dwell between the beams.

```
IDLE --A or B blocked--> HALF_HIT (timestamp)
HALF_HIT --second beam within 300 ms--> direction decided → event → IDLE
HALF_HIT --timeout, or blocked longer than 2 s--> discard → IDLE
```

Occupancy is `Σin − Σout`, clamped at ≥ 0. Counting errors accumulate, so grading is always cross-checked against occupancy: a low headcount cannot produce "Full". This iteration does not auto-reset occupancy; drift is handled through manual baseline calibration and log review.

### 4.4 Queue-length grading

| State | Condition |
| --- | --- |
| Few | the 50 % beam is not continuously blocked |
| About half | the 50 % beam has been blocked ≥ 5 s and the 100 % beam is clear |
| Full | the 100 % beam has been blocked ≥ 5 s |

- **Sustained blocking** separates "standing in the queue" from "walking past": a moving crowd only produces short pulses.
- **Hysteresis**: upgrades apply immediately, downgrades require 15 s of continuous clear lane, avoiding flicker.
- **Cross-check**: compared with occupancy from the gate counter; a low headcount cannot yield "Full", and a conflict raises a flag in the uplink frame.

### 4.5 Historical headcount prediction

Queue length follows a clear daily rhythm (lunch peaks, class breaks), so the predictor uses the historical mean of the same slot:

```
pred(t) = mean( occ(d, t) ),  d ∈ the last 7 days that are already complete
pred_n  = number of days that contributed (0 = not enough history)
```

- The day is cut into **15-minute × 96 slots**, one record per person per slot; the STM32 keeps a 7×96 `uint8` table (672 B) in RAM without a large buffer.
- **Today is excluded**: an unfinished day would bias the mean downward, so at least one complete historical day is required before a value appears.
- Minute-level samples are written to the SD card (`HH:MM,occ`); on power-up the last 8 days are read back to rebuild the table, so prediction survives a reboot.
- Predicted value and contributing-day count travel in the 4 Hz uplink frame; the dashboard shows "predicted headcount / based on N days" and displays "—" rather than 0 when history is insufficient.

Prediction depends on a real clock (DS3231). Without it the system cannot tell what "the same slot" means, so the prediction stays unavailable and is shown as such rather than as a plausible-looking number.

### 4.6 Time base and data storage

- **DS3231** (I²C1, address 0x68, PB6/PB7) supplies offline timestamps. Every I²C wait has a timeout, consecutive failures trigger a 9-clock bus recovery, and a module that is missing at boot is retried every 10 s.
- **SD card** (SPI1, CS = PA4) is mounted with FatFs R0.16 and receives three files:

| File | Content | Rate |
| --- | --- | --- |
| `events.csv` | `datetime,uptime_ms,beam,level` | every edge |
| `status.csv` | `datetime,uptime_ms,in,out,occ,q50_ms,q100_ms,grade,flags` | 1 Hz |
| `HYYMMDD.CSV` | `HH:MM,occ`, one file per day | every minute |

- 8.3 short names are used; the card must be FAT16/FAT32 and ≤ 32 GB. `f_sync` runs every 10 s, so a power cut loses at most 10 s.
- Filesystem timestamps are fixed at 2020-01-01 (`FF_FS_NORTC`) and are not used as a time reference; the `datetime` column is authoritative.

### 4.7 Edge software and communication

The firmware is a bare-metal time-slice architecture: everything under `core/` is hardware-independent C with unit tests that run on a PC, while `stm32/` and `esp32/` hold the hardware and networking specifics.

| Module | Responsibility |
| --- | --- |
| `gate_counter` | two-beam direction FSM, in/out counts, occupancy |
| `queue_grade` | 50 %/100 % threshold grading, hysteresis, occupancy cross-check |
| `qsm_history` | 15-minute slots, date conversion, prediction mean, line formatting |
| `hist_store` / `data_logger` / `bsp_sd_disk` | FatFs, SD driver, the three logs and read-back |
| `protocol` | uplink frame codec, CRC-8, downlink command frame |
| `bsp_e18` / `bsp_uart` / `bsp_ds3231` | 1 kHz sampling and debounce, UART, RTC |

Task cadence:

| Period | Work |
| --- | --- |
| 1 kHz (SysTick) | sample four beams, debounce |
| 10 ms | drain edges, advance FSMs, grade timing |
| 250 ms | build and send the uplink frame (4 Hz) |
| 1 s | write `status.csv`, refresh the debug line |
| 10 s | `f_sync` |
| 60 s | one history-table record |

Uplink frame (`*` marks a two-digit hexadecimal CRC-8):

```
$QSM,<seq>,<ms>,<datetime>,<in>,<out>,<occ>,<q50_ms>,<q100_ms>,<grade>,<flags>,<pred>,<pred_n>,<beams>*<CRC8>
```

The last three fields were added later; the parser reads by index and defaults them to 0, so old and new firmware interoperate. The ESP32 runs in **AP mode**, hosting the dashboard at the fixed address `192.168.4.1` with no dependency on an existing network; the page polls `/api/state` every 400 ms and shows an orange banner when no frame has arrived for 2.5 s.

### 4.8 Privacy design

The system contains **no imaging sensor of any kind**. It measures whether infrared beams are interrupted, from which it derives one count, one direction and two threshold states. No appearance, gait, face, device address or personal identifier is captured. Only aggregated counts and grades leave the node; raw crossing events stay on the local SD card as validation evidence and are never uploaded.

## 5. Evaluation Plan

| ID | Verification |
| --- | --- |
| A1 | Observe the three grades side by side with manual observation, ≥ 5 repeats covering all three states; report agreement rate |
| A2 | Decide crowding state from occupancy plus grade in idle / normal / peak scenarios against manual observation; report accuracy |
| A3 | Compare stopwatch observations with system log timestamps; report the latency distribution from field change to web update |
| A4 | Run continuously for 3 h with SD logging and count crashes and data loss (via `seq` continuity) |
| A5 | Design and data review: no imaging device, no biometric processing, aggregated values only |
| A6 | End-to-end demonstration with all four beams active: entry/exit counts plus grading thresholds |
| A7 | Unit tests of the direction FSM, grading FSM, historical prediction and frame codec using injected synthetic events; integration and system tests against O1–O7, evidenced by SD logs and manual count sheets |
| A8 | On-site demonstration of live status, grade trend, predicted headcount and alerting |
| A9 | Document the UL vs LICHPU deployment comparison: cost, installation and infrastructure dependencies |

The evaluation protocol follows [1], which validated a comparable counter through repeated simultaneous manual and automatic counting and reported both overall accuracy and an error-type breakdown.

## 6. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Two people passing together | missed counts, hence occupancy drift | two-beam coincidence rule; single-file geometry; cross-check with grading. This is the main error source in [1] |
| Occupancy drift | headcount and grade gradually lose accuracy | grading relies on the beams, occupancy only cross-checks; manual baseline review with logs. No auto-reset in this iteration |
| Missing or disconnecting clock | history cannot be filed by slot; prediction unavailable | I²C timeouts throughout, bus recovery, 10 s retry when absent at boot; the dashboard shows "not enough history" instead of 0 |
| Insufficient accumulated history | no prediction on the first day | expectation stated explicitly: at least one complete historical day is needed, and the contributing-day count is always shown |
| Crosstalk or mounting drift | false or missed triggers | 5 ms debounce; 10 cm spacing; per-unit calibration at installation |
| Someone dwelling in a beam | false or blocking events | 2 s blocking timeout |
| No single-file lane at the chosen site | the method does not apply | confirm the service point and queue geometry before the design gate |
| **[1]'s accuracy is flow accuracy, not headcount accuracy** | overstated expectations | measure grade agreement against a manual baseline rather than quoting 91.45 % |

## 7. Timeline

The project follows the 12-week timeline and gates defined in the repository README. Hardware and firmware milestones: weeks 1–4 confirm the approach and queue geometry and complete a single-gate bench test; weeks 5–6 wire all four beams and take the counting and grading FSMs up; week 7 adds historical prediction and SD persistence; week 8 the dashboard and real-time behaviour; weeks 9–10 field validation and design freeze; weeks 11–12 the report and demonstration.

**Deliverables**: a working integrated prototype; a live dashboard with prediction and reset controls; the source repository and documentation; validation evidence against a manual baseline.

## 8. References

[1] R. L. dos Santos, H. C. de Oliveira, M. C. de Almeida, D. F. Vieira, E. P. Lacusta Junior, and T. Ji, "A low-cost bidirectional people counter device for assisting social distancing monitoring for COVID-19," *J. Control, Autom. Electr. Syst.*, vol. 33, pp. 1148–1160, 2022, doi: 10.1007/s40313-022-00916-z.

[2] S. Malkireddy et al., "LOADS: LiDAR-based privacy-preserving queue monitoring and analysis," in *Proc. IEEE Int. Conf. Pervasive Computing and Communications Workshops (PerCom Workshops)*, 2025, pp. 573–575, doi: 10.1109/PerComWorkshops65533.2025.00132.

[3] M. Rezzouki, S. Ouajih, and G. Ferré, "Monitoring social distancing in queues using infrared array sensor," *IEEE Sensors J.*, vol. 22, no. 17, Sep. 2022, doi: 10.1109/JSEN.2021.3104669.

[4] T. Carvalho, L. Antunes, F. Moreira, J. Barata, H. Balu, and J. Bastos, "Privacy-driven network data for smart cities," arXiv:2509.12403, 2025.
