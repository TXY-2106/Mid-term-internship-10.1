<div align=center>

**English** ｜ [中文](proposal.zh.md)

</div>

# A Dual-Beam Infrared Queue Monitor for Campus Service Counters

> UL–LICHPU Smart Campus Living Lab · Mid-term Internship 10.1 · 15 ECTS · 12-week delivery cycle
>
> Document type: technical proposal · Language: English ([中文版](proposal.zh.md))

## 1. Introduction

Service counters on campus — canteen food stalls, library enquiry desks, print stations and coffee counters — form queues at peak times. Users cannot judge the waiting situation before joining, and service-point operators have no real-time view of demand distribution. The result is avoidable congestion and poorly targeted staffing.

Camera-based crowd monitoring is technically effective but captures identifiable features. For a university deployment this is not acceptable, and anonymising network-level alternatives extract a substantial utility cost: prior work shows that Wi-Fi session data must be degraded to hourly granularity with added noise before it can be shared [4].

This project takes a different route. Rather than sensing what people look like or which devices they carry, it senses **when they cross a boundary**. Two pairs of infrared photoelectric beams mark the tail and the head of a single-file queue lane. Counting crossings in each direction yields the number of people waiting directly, with no imaging device and no identity, and arrival and service rates follow from the same events. The proposal is deliberately narrow: one well-defined queue geometry, one sensing principle, one edge node.

## 2. Objectives and Scope

| ID | Objective | Criterion | Traces to |
| --- | --- | --- | --- |
| O1 | Queue length estimation | Error ≤ ±2 persons, target ±1, within a design range of 0–20 persons | A1 |
| O2 | Waiting-time estimation | Derived from Little's Law; agreement with manual observation within the tolerance fixed at the requirements gate | A2 |
| O3 | End-to-end latency | ≤ 5 s from an on-site change to a display update | A3 |
| O4 | Operational stability | ≥ 4 h continuous operation without failure | A4 |
| O5 | Privacy preservation | No imaging device of any kind; aggregate counts only | A5 |
| O6 | Cost and replicability | Bill of materials below ¥150 per service point; UL–LICHPU differences documented | A9 |

**In scope:** anonymous queue-length and waiting-time estimation for a single-file queue lane; local display; web dashboard with historical trend and alerting; verification against a manual baseline.

**Out of scope:** personal identification; face or gait recognition; attendance or law-enforcement use; automated decisions about university staffing; sensing in open areas without a defined queue lane.

## 3. Related Work

**Camera-based crowd analysis** is the established approach and remains the accuracy reference, but it is privacy-invasive and costs more. **LiDAR-based queue monitoring** avoids imagery entirely by clustering point clouds, one cluster per person [2]; it is privacy-preserving and light-independent, but a single 360° LiDAR dominates the bill of materials and the published work is a demonstration without quantitative accuracy.

**Infrared array sensing** in queues has been demonstrated with an MLX90640 32×24 array on an STM32L476, detecting up to six persons per queue with 93 % accuracy at sub-80 cm spacing [3]. This establishes that thermal sensing works in queues but also documents its density limit: resolution, not cost, becomes the binding constraint.

**Bidirectional infrared beam counting** is the closest match to this proposal. dos Santos et al. [1] built a counter from an ESP32 and an **E18-D80NK** sensor with RTC and microSD logging, and validated it in two real deployments: bus doors (**91.45 %**) and **university restaurant ticket gates (98.65 %)**. Their counting rule — a crossing counts only if two sensors activate within a **300 ms** window, direction taken from activation order — directly addresses the false-trigger and direction-ambiguity problems that plague single-beam counters. Their error taxonomy identifies simultaneous passage as the dominant residual source.

**Gap.** Published queue-monitoring work reports either sensing performance in laboratory conditions or flow-counting accuracy at a gate; none reports **queue-length estimation error against a manual baseline** together with a **waiting-time estimate derived from Little's Law**, at a bill of materials compatible with replication. That combination, for a single-file lane, is the contribution of this project.

## 4. Technical Approach

### 4.1 System overview

```
 entry gate (2 beams) --EXTI--> direction FSM --> lambda_in   (+)
 exit  gate (2 beams) --EXTI--> direction FSM --> lambda_out  (-)
                                   |
                     STM32F103C8T6 edge node
                     ├── L(t)  = Σin − Σout              occupancy
                     ├── W     = mean(L) / mean(lambda)  Little's Law
                     └── L_mod = lambda / (mu − lambda)  M/M/1 cross-check
                                   |
                 OLED (local)  +  MQTT (3 s)  →  web dashboard
                                   |
                          microSD event log (evidence)
```

### 4.2 Queue zone and deployment geometry

The monitored zone is a single-file queue lane: 60–90 cm wide, up to 6 m long, with one entry at the tail and one exit 0.5 m before the counter. The zone boundary is defined by the two gates; nothing inside the zone is sensed.

Each gate uses two **E18-D80NK** diffuse-reflective sensors (adjustable detection range 3–80 cm) mounted 10 cm apart along the direction of travel at approximately 100 cm height. The 10 cm spacing combined with a walking speed of 1.2–1.4 m/s gives a 70–85 ms separation between beam interruptions, comfortably above the 20 ms debounce and within the 300 ms coincidence window.

### 4.3 Counting and direction discrimination

Following [1], a crossing is registered only when **both** beams of a gate are activated within 300 ms; a single-beam activation is discarded as a false trigger. Direction follows from activation order: A-then-B is one direction, B-then-A the other. A 20 ms debounce rejects contact bounce, and a 2 s blocking timeout discards events in which a person remains within the beam.

Each gate runs an independent finite state machine, so the entry and exit gates cannot interfere with one another:

```
IDLE --A or B interrupted--> HALF_HIT (timestamp)
HALF_HIT --second beam within 300 ms--> direction resolved → emit event → IDLE
HALF_HIT --timeout, or beam blocked > 2 s--> discard → IDLE
```

### 4.4 Occupancy accounting and drift correction

Queue length follows from conservation of flow:

```
L(t) = Σ in − Σ out ,  clamped at L ≥ 0
```

Occupancy accounting has one structural weakness: **error accumulates**. A missed crossing is never recovered, so a systematic bias causes L to drift without bound. Two mitigations are applied. First, the dual-beam coincidence rule removes the largest class of false events. Second, an **empty-zone re-zeroing** is performed when both gates have been idle for a sustained interval and the most recent event was an exit, which indicates an empty queue. Re-zeroing events are logged, so the correction itself is auditable.

### 4.5 Waiting time from Little's Law

With queue length and arrival rate measured independently, waiting time follows from Little's Law:

```
W = mean(L over window) / mean(lambda over window)      window = 5 min
```

A time-averaged formulation is required because Little's Law relates long-run averages; dividing an instantaneous L by an averaged arrival rate would be biased. When λ falls below a minimum threshold, the state is reported as **idle** and W as unavailable, rather than reporting an unbounded value.

Because the exit gate measures the service rate μ, the M/M/1 model provides a second, independent prediction,

```
L_model = lambda / (mu − lambda) ,     W_model = 1 / (mu − lambda)
```

which uses no occupancy information at all. The divergence `|L − L_model|` is reported as a consistency indicator and used to flag anomalous conditions such as a person leaving the queue without crossing the exit gate.

Queue status is classified by dual thresholds on L and W, so that the reported state reflects both how many people are waiting and how long they have waited.

### 4.6 Edge software

The firmware is a bare-metal, time-sliced application in four layers — driver, service, application, HAL — with an interrupt-to-task event ring buffer so no algorithmic work executes in interrupt context.

| Module | Responsibility |
| --- | --- |
| `e18` | Four EXTI channels, debounce, direction FSM, event queue |
| `tracker` | Occupancy, λ, μ, W, M/M/1 cross-check, status |
| `ds3231` / `sdlog` | Timestamps and auditable event logging |
| `oled` / `esp_at` | Local display, MQTT publication |
| `main` | 1 ms tick; 10 ms FSM; 1 s display and statistics; 3 s publish; 5 s log |

The local display updates within 1 s and MQTT publishes every 3 s, satisfying the 5 s end-to-end latency criterion with margin.

### 4.7 Privacy by design

The system contains **no imaging sensor of any kind**. It measures the interruption of two diffuse infrared beams, from which only a count and a direction are derived. No appearance, gait, face, device address or personal identifier is captured at any stage. Only aggregate counts and rates leave the node over MQTT; raw crossing events remain on the local SD card as verification evidence and are not transmitted.

## 5. Evaluation Plan

| ID | Verification method |
| --- | --- |
| A1 | Simultaneous manual headcount over ≥ 5 repeats across densities 0–20 persons; report mean absolute error and an error-versus-density curve |
| A2 | Compare classified status with manual observation across idle, normal and peak scenarios; report accuracy |
| A3 | Compare stopwatch observation with system log timestamps; report latency distribution |
| A4 | Continuous 4 h operation with SD event log; report failures |
| A5 | Design and data review: confirm no imaging device, no biometric processing, aggregate-only transmission |
| A6 | End-to-end demonstration with both gates active |
| A7 | Unit tests of the direction FSM and occupancy tracker using injected synthetic events; integration and system tests traced to O1–O6, with the SD event log and manual count sheets as evidence |
| A8 | Live demonstration of real-time status, historical trend and alerting |
| A9 | Documented comparison of UL and LICHPU deployment, including cost, mounting and infrastructure dependencies |

The protocol follows [1], which validated a comparable counter by repeated simultaneous manual and automated counting and reported both aggregate accuracy and a breakdown by error type.

## 6. Risks and Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Simultaneous passage of two persons | Under-count, then drift | Dual-beam coincidence rule; single-file lane geometry; error-versus-density characterisation. This was the dominant error in [1] |
| Accumulated drift in occupancy | Unbounded queue-length error | Empty-zone re-zeroing; auditable correction log; M/M/1 consistency check |
| Person standing within a beam | Spurious or blocked events | 2 s blocking timeout |
| Site lacks a single-file lane | Method not applicable | Confirm the service point and queue geometry in weeks 1–2, before the design gate |
| Reported accuracy of [1] is flow accuracy, not queue-length accuracy | Over-stated expectation | State explicitly that occupancy error accumulates; verify against manual headcount rather than transferring the 91.45 % figure |
| Component tolerance (E18-D80NK detection range varies) | Detection zone inconsistent | Set each sensor's potentiometer against a physical gauge during installation; document the procedure for replication |

## 7. Timeline

The project follows the 12-week module timeline and gates defined in the repository README. Hardware and software milestones map as follows: bench verification of one gate in week 4; both gates and occupancy tracking in week 5; integrated prototype with dashboard in week 6; calibration of threshold, debounce and window parameters in week 7; verification campaign in week 8; analysis and design freeze by week 10; report and demonstration by week 12.

**Deliverables:** integrated working prototype; real-time local display; web dashboard with historical trend; source repository and documentation; verification evidence against the manual baseline.

## 8. References

[1] R. L. dos Santos, H. C. de Oliveira, M. C. de Almeida, D. F. Vieira, E. P. Lacusta Junior, and T. Ji, "A low-cost bidirectional people counter device for assisting social distancing monitoring for COVID-19," *J. Control, Autom. Electr. Syst.*, vol. 33, pp. 1148–1160, 2022, doi: 10.1007/s40313-022-00916-z.

[2] S. Malkireddy et al., "LOADS: LiDAR-based privacy-preserving queue monitoring and analysis," in *Proc. IEEE Int. Conf. Pervasive Computing and Communications Workshops (PerCom Workshops)*, 2025, pp. 573–575, doi: 10.1109/PerComWorkshops65533.2025.00132.

[3] M. Rezzouki, S. Ouajih, and G. Ferré, "Monitoring social distancing in queues using infrared array sensor," *IEEE Sensors J.*, vol. 22, no. 17, Sep. 2022, doi: 10.1109/JSEN.2021.3104669.

[4] T. Carvalho, L. Antunes, F. Moreira, J. Barata, H. Balu, and J. Bastos, "Privacy-driven network data for smart cities," arXiv:2509.12403, 2025.
