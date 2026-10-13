#  Queue / Service Demand Monitor

## Ⅰ. Background
Consisdering that there is a **long queues** at restaurant in peak time, students can't know the queue number. And traditional camera setup involves privacy which university probably won't accept. So we try to solve it by infrared sensor, a schema has less problem with privacy.

## Ⅱ. Objectives & Scope
The project delivers a working prototype with the following objectives:

| ID | Objective | Goal |
| --- | --- | --- | 
| O1 | Queue length estimation | Error ≤ ±1 person within a design density range of 0-6 persons per node |
| O2 | Waiting-time estimation | Estimate waiting-time by using Little's Law |
| O3 | End-to-end latency | ≤ 5 s from a change to a display |
| O4 | Operational stability | ≥ 4 h continuous operation |
| O5 | Privacy preservation | No visible-light imaging, no facial or biometric identification |
| O6 | Cost and replicability | Bill of materials below ¥300; deployment differences between UL and LICHPU |

**In scope**: anonymous queue-length and waiting-time estimation; local display
**Out of scope**:personal identification, face recognition
## Ⅲ. Related Work
Today, crowd monitoring and density estimating are im
plemented by video (or image) based recognition technology
[1]. But, it's privay-invasive. The most directly relevant prior work is summarised below.

[📄](papers/Monitoring_Social_Distancing_in_Queues_Using_Infrared_Array_Sensor.pdf)[2], a paper about using infrared array sensor to monitor queue-lengh. Deployed an **MLX90640** 32×24 infrared array sensor on an **STM32L476** microcontroller, communicating over I<sup>2</sup>C and LoRa to monitor queues. Installed overhead and tested aat several heights, the node detects **up to six persons per queue with 93% accuracy** when inter-person spacig is below 80cm, while preservig privacy and consuming less energy.

[📄](papers/s40313-022-00916-z.pdf)[3], a paper bulit a low -cost bidirectional people counter from an **ESP32** and **E18-D80NK infrared sensor**, with an RTC and microSD logging. Deployed at bus and **university restaurant gates**, it achieved 91.45% and 98.65% accurary.

[]()

## Ⅳ. Technical Approach
The project uses two sensing channels on one microcontroller

·MLX90640 -- I <sup>2</sup>C--> waiting number

·two E18-D80NK  --EXTI--> flow

## Ⅴ. Hardware Bill of Materials
| Function | Component | Quantity |
| --- | --- | --- |
| Edge compute | STM32 Cortex-M4F board | 1 |
| Queue sensing | MLX90640,110°×75°, 32×24, I<sup>2</sup>C | 1-2 |
| Flow sensing | E18-D80NK infrared sensor | 4-6 |
| Timing / logging |  DS3231 RTC, microSD adapter | 1-2 each |
| Local display | OLED display, I<sup>2</sup>C | 1 |
| Connectivity | ESP32 co-processor | 1 |
| Power | 5 V supply; 10_000 mAh power bank | 1 |


## References
[1] M. Li, Z. Zhang, K. Huang, and T. Tan, Estimating the number of
people in crowded scenes by mid based foreground segmentation and
head-shoulder detection, in Proc. of IEEE ICPR, 2008, pp. 1C4.

[2] M. Rezzouki, S. Ouajih, and G. Ferré, "Monitoring social distancing in queues using infrared array sensor," IEEE Sensors J., vol. 22, no. 17, Sep. 2022, doi: 10.1109/JSEN.2021.3104669.

[3] R. L. dos Santos, H. C. de Oliveira, M. C. de Almeida, D. F. Vieira, E. P. Lacusta Junior, and T. Ji, "A low-cost bidirectional people counter device for assisting social distancing monitoring for COVID-19," J. Control, Autom. Electr. Syst., vol. 33, pp. 1148–1160, 2022, doi: 10.1007/s40313-022-00916-z.
