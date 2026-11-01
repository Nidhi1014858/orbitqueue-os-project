<!-- OWNER: Nidhi -->
# Section 1: Introduction and Project Scope

## 1.1 Project Title and Use Case Context

**Project Title:** *OrbitQueue: SomaiyaSat Downlink Router*<br/>
**Mission Use Case:** SomaiyaSat & SomaiyaPod nanosatellite Downlink Infrastructure (`KJS-SRS-01`)<br/>
**Academic Batch / Team:** Batch B2 | Experiments 7, 8, 9, 10

OrbitQueue is an operating systems mini-project designed to model and simulate the onboard downlink packet routing pipeline of a Low Earth Orbit (LEO) nanosatellite. Operating within the operational context of SomaiyaSat and its deployment pod (SomaiyaPod), the onboard computer (OBC) must orchestrate concurrent packet streams, allocate shared communications hardware, and prioritize critical telemetry under rigorous computational, memory, and energy constraints.

---

## 1.2 Problem Statement

Nanosatellite missions face severe physical, atmospheric, and orbital operational hurdles:

1. **Transient Contact Windows:** Due to rapid orbital passes in Low Earth Orbit, communications with terrestrial ground stations are confined to short, intermittent contact windows.
2. **Fluctuating Link Quality and Power Reserves:** Solar illumination cycles, battery depth of discharge, atmospheric attenuation, and antenna elevation angles lead to dynamically varying link quality and tight power budgets.
3. **Competing Traffic Profiles:** Diverse onboard subsystems concurrently generate data streams with vastly different service requirements:
   - *Mission-Critical Health & Status:* Telemetry, Tracking, and Command (TT&C) and subsystem HOUSEKEEPING data, which must never be deferred or rejected.
   - *Bulk Payload Telemetry:* Bandwidth-intensive Slow Scan Television (SSTV) earth observation imagery and voice/data streams (e.g., Codec2/M17 digitized voice), which are valuable but deferrable to future passes.

Without deterministic operating-system-level scheduling, resource safety guarantees, and memory access governance, bulk data can exhaust onboard buffers, cause priority inversions, trigger deadlock conditions over shared transmission peripherals, or lead to the unrecoverable loss of mission health beacons.

---

## 1.3 Project Objectives

The OrbitQueue architecture is designed to address this challenge through five concrete objectives:

1. **Prioritized Packet Routing:** Implement a thread-safe, bounded priority queue (`PacketQueue`) that enforces strict priority ordering (`TT&C > HOUSEKEEPING > VOICE > SSTV`) with arrival-order tie-breaking.
2. **Inviolable Telemetry Safety Rule:** Enforce an immutable hardware-protection safety override ensuring critical TT&C and HOUSEKEEPING packets are never deferred or rejected, guaranteeing ground controllers retain satellite command authority.
3. **Explainable ML Classification with Resilient Fallback:** Incorporate machine learning classification models to recommend transmission decisions (`TRANSMIT`, `DEFER`, `REJECT`) based on channel conditions and battery state, backed by a deterministic rule-based fallback when model predictions are absent.
4. **Cohesive Four-Experiment OS Integration:** Combine four classical operating systems problems into one functioning application rather than isolated classroom toys:
   - Multi-threaded producer-consumer synchronization with slot reservations (Experiment 7).
   - Memory segmentation and paging address translation (Experiment 8).
   - System deadlock avoidance using Dijkstra's Banker's Algorithm (Experiment 9).
   - Virtual memory page replacement simulation across access reference strings (Experiment 10).
5. **Auditable and Reproducible Linux Engineering:** Deliver clean, modular, and standard-compliant POSIX C++ code designed for automated compilation (`Makefile`), ThreadSanitizer race detection, and structured logging.

---

## 1.4 Architectural Integration of the Four OS Experiments

Rather than executing as standalone assignments, the four operating system experiments serve as functional subsystems of the unified `router_app` pipeline:

```
+---------------------------------------------------------------------------------------+
|                                    OrbitQueue OBC                                     |
|                                                                                       |
|  [Exp 7: Concurrency & Synchronization]                                               |
|  - TelemetryProducer (TT&C, HK) & PayloadProducer (SSTV, Voice)                       |
|  - Bounded Priority Queue (8 slots; sem_bulk reserves 2 slots for critical packets)   |
|  - Multi-threaded Router Workers popping highest-priority packets                     |
|                                     |                                                 |
|                                     v                                                 |
|  [Router Engine: ML Decision & Safety Override]                                       |
|  - ML / Baseline lookup (decisions.csv) with deterministic fallback                   |
|  - Hard safety check: critical packets forced to TRANSMIT                             |
|                                     |                                                 |
|                                     v                                                 |
|  [Exp 9: Deadlock Avoidance (Banker's Algorithm)]                                     |
|  - Tasks: T0 Telemetry, T1 Router, T2 SSTV, T3 Voice, T4 DownlinkPrep                 |
|  - Shared Resources: CPU slots (A), Buffer units (B), Transmitter slots (C)           |
|  - rm::request() checks safety state S1; non-critical denied requests are deferred    |
|                                     |                                                 |
|                                     v                                                 |
|  [Exp 8 & Exp 10: Virtual Memory Pipeline]                                            |
|  - 9 deterministic memory accesses per packet logged via memtrace::access()           |
|  - Exp 8: Logical address translation across 5 segments and 8 logical pages           |
|  - Exp 10: Page reference string evaluated under FIFO, LRU, and Optimal algorithms   |
|                                     |                                                 |
|                                     v                                                 |
|  [Outputs: Downlink Dispatch & Ground Station Logging]                                |
|  - outputs/routing_log.csv, outputs/router_summary.txt, live [GS] console status     |
+---------------------------------------------------------------------------------------+
```

---

## 1.5 Prototype Scope and Boundaries

To maintain rigorous experimental focus, the boundaries of the OrbitQueue prototype are clearly delineated:

- **Userspace Simulation:** The project is implemented entirely as a userspace C++ simulation executing on Linux (Ubuntu / WSL), utilizing native POSIX threads (`pthreads`), POSIX semaphores, and standard synchronization mechanisms.
- **No Physical Hardware Transceiver:** The downlink router simulates transmission actions; it does not interface with actual Software Defined Radios (SDR), physical RF modems, or orbital transceivers.
- **Synthetic Memory Abstraction:** Memory translation in Experiment 8 and page replacement in Experiment 10 operate on synthetic logical memory traces produced by the router, rather than interacting with the host operating system's kernel MMU, TLB hardware, or hardware page fault interrupt vectors.
- **Deterministic Evaluation:** System workloads, initial resource allocation matrices, synthetic input datasets, and documented memory-access formulas are designed to be deterministic and reproducible, enabling direct side-by-side comparison between safe and unsafe synchronization modes, as well as replacement algorithm page fault counts.

---

## 1.6 Ethical & Data Integrity Disclaimer

All satellite orbital passes, packet counts, telemetry values (battery level, urgency, link quality, payload data size), and hardware resource vectors utilized in this project are **entirely synthetic teaching data**. They are engineered to model representative satellite operations for educational evaluation in university-level Operating Systems laboratory coursework. No real, proprietary, or classified spacecraft telemetry from SomaiyaSat, SomaiyaPod, or any institutional aerospace partner is utilized or exposed.

---

## 1.7 Current Verification Status

The software architecture and component implementations (including `PacketQueue`, `satellite_sync`, `router_app`, and memory access specifications) have been authored and structurally checked against the interface specifications. In strict accordance with laboratory integrity guidelines, empirical performance figures, execution benchmarks, ThreadSanitizer race logs, and terminal output captures will only be collected and recorded following direct execution on the dedicated Linux environment.
