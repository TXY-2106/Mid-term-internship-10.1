### A queue monitor based on Thermal imaging and STM32 

## Ⅰ. Background
Consisdering that there is a **long queues** at restaurant in peak time, students can't know the queue number of each food stall. And traditional camera setup involves privacy which university probably won't accept. So we try to solve it by thermal imaging, a schema has less problem with privacy.

## Ⅱ. Schema
We are going to rebuild the project in BiliBili [📺](https://www.bilibili.com/video/BV1EnoRYTEer/). And do some localization modifications such as lightweighting, making it replicable so that it can lanuch more suitable in HPU.

## Ⅲ. Paper endorsement
In this paper[📄](papers/LOADS_LiDAR-based_Privacy-Preserving_Queue_Monitoring_and_Analysis.pdf) the writers discussed a proposal about crowd monitor. It inspires this project.

## Ⅳ. Innovation point
1. Privacy-friendly, doesn't collect faces
2. STM32 edge calculation, doesn't upload original images
3. Loe cost, highly replicable

## Ⅴ. Goal
Achieve a queue-length estimation error of ±1 - 2 pepole and display real-time queue-length 