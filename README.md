# cTCP with BBR

This repo is a part of the lab in computer networking class taught by prof. ramesh govindan at USC.

## Overview

I delved into the intricacies of transport protocol design and implementation, focusing on a cTCP(stripped-down version of TCP) model with Google's BBR (Bottleneck Bandwidth and Round-trip propagation time) congestion control algorithm. This repo aims to provide hands-on experience in implementing and testing cTCP that integrates advanced congestion control mechanisms to optimize network performance.

## Objectives

- Implement cTCP that operates at the user level, outside of the OS kernel.
- Integrate BBR congestion control to enhance the efficiency of data transmission under varying network conditions(drop, corrupt, delay, duplicate).
- Test the implementation in a simulated network environment to evaluate performance and ensure compatibility with standard TCP protocols.

## Implementation

### cTCP

- Implement stop-and-wait and sliding window mechanisms to manage data flow and ensure reliable data transmission.
- Handle cTCP segments to send and receive them.

### BBR Congestion Control

- Incorporate BBR algorithm to dynamically adjust data sending rates based on estimated network bandwidth and latency.
- Test BBR's effectiveness in various network conditions, focusing on its ability to maintain high throughput and low latency.

For details, please navigate to [Report_1.pdf](./Report_1.pdf), [Report_2.pdf](./Report_2.pdf), [ctcp.c](./ctcp.c), [ctcp_bbr.c](./ctcp_bbr.c), etc.
