<div align=center>

[中文](README.md) ｜ **English**

</div>

# Mid-term Internship 10.1 | Queue / Service Demand Monitor

> UL–LICHPU Smart Campus Living Lab · Student Project · 15 ECTS · 12-week delivery cycle
>
> Current stage: early — topic confirmed; service location, requirements and technical approach still to be defined

## 1. Project Overview

**Campus problem**

Service counters on campus — canteen pick-up, library borrowing and enquiry desks, student services, print stations, coffee counters — tend to form queues at peak times. Users cannot judge the waiting situation in advance, and service operators have no real-time view of demand distribution.

**Project tasks**

- Anonymously sense queue length or service demand in the selected service area
- Estimate the waiting status (idle / normal / busy, or expected waiting time in minutes)
- Publish the status in real time on a local display and a web dashboard, and retain historical data

**Users and beneficiaries**

- Direct users: students and staff waiting for service — less blind waiting
- Beneficiaries: service-point operators — visibility of demand distribution for staffing and queue guidance

**Core deliverables**: integrated working prototype + real-time waiting-status display + dashboard + verification evidence against a human baseline

**Out of scope**: personal identification, face or gait recognition, attendance or law-enforcement use, automated decisions about university staffing

## 2. Acceptance Criteria

| ID | Item | Criterion | Verification method |
| --- | --- | --- | --- |
| A1 | Queue length estimation | Error ≤ ±1 person within the design density range | Compare with manual headcount in the same time window, ≥ 5 repeats |
| A2 | Waiting-status decision | ≥ 80% accuracy against manual observation | Cover idle / normal / peak scenarios |
| A3 | End-to-end latency | ≤ 5 s from on-site change to display update | Compare a stopwatch with system log timestamps |
| A4 | Stability | ≥ 4 h continuous operation without failure | Long-run operation log |
| A5 | Privacy compliance | No camera, no face or identity recognition; aggregate counts only | Design and data review |
| A6 | Integrated prototype | At least 2 meaningful data inputs, working end to end | Live demonstration |
| A7 | Test evidence | Unit / integration / system tests traced to requirements, documented | Test records and datasets |
| A8 | Dashboard | Real-time status + historical trend + alerting | Live demonstration |
| A9 | Cross-campus replication | Differences and feasibility of deployment at UL and LICHPU explained | Report chapter |

> These thresholds must be confirmed with the module tutor and frozen before the requirements gate, then refined into testable requirement items.

## 3. Timeline (12 weeks)

| Week | Focus | Required evidence | Gate |
| --- | --- | --- | --- |
| Week 0 | Team formation and topic confirmation | Team list, role allocation | Team confirmation |
| Week 1 | Problem definition and campus needs | Problem statement, users and location, success measures | Concept review |
| Week 2 | Benchmarking and requirements | Benchmark analysis, requirements, acceptance criteria, ethics and privacy screening | Requirements gate |
| Week 3 | System design | Architecture diagram, component selection, data model, bill of materials, risk register | Design gate |
| Week 4 | Development plan and subsystem verification | Schedule, task ownership, single-sensor verification | Progress review 1 |
| Week 5 | Integrated prototype v1 | End-to-end data path, first waiting-status display | Prototype checkpoint |
| Week 6 | Integration and mid-term review | Working core system, contribution matrix, risk review | Mid-term assessment |
| Week 7 | Optimisation and calibration | Queue-length and waiting-time model calibration, alerting logic | Technical review |
| Week 8 | Verification campaign | Structured experiments against the manual baseline | Evidence checkpoint |
| Week 9 | Analysis and deployment assessment | Results analysis, UL–LICHPU replicability, cost | Progress review 2 |
| Week 10 | Final engineering iteration | Close critical defects, complete acceptance tests, design freeze | Design freeze |
| Week 11 | Report and presentation preparation | Report draft, slides, demo script, repository documentation | Draft ready |
| Week 12 | Final assessment | Prototype demo, team presentation, individual defence, final report | Final gate |
