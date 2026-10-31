<!-- OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner. -->
# Section 1: Introduction and Project Scope

## 1.1 Project Overview
OrbitQueue simulates downlink packet routing for the SomaiyaSat and SomaiyaPod nanosatellite mission. Operating within constrained contact windows with terrestrial ground stations, the satellite onboard computer must prioritize telemetry, tracking, and command (TT&C) data while selectively scheduling payload telemetry (SSTV imagery and voice/data streams).

## 1.2 OS Concepts Integration
This project integrates four core Operating System concepts:
- **Experiment 7:** Multi-threaded synchronization via bounded priority queue (Producer-Consumer).
- **Experiment 8:** Paging and segmentation memory translation with bounds checking.
- **Experiment 9:** Deadlock avoidance using Dijkstra's Banker's algorithm.
- **Experiment 10:** Virtual memory page replacement policies (FIFO, LRU, Optimal).
