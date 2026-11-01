<!-- OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner. -->
# OrbitQueue: SomaiyaSat Downlink Router

> **Notice:** All data used in this project is purely synthetic for teaching and educational demonstration. None of the values represent actual operational telemetry from SomaiyaSat or SomaiyaPod.
>
> **Build Status:** Scaffold state. All components are initial stubs and have **NOT yet been built or tested on Linux**.

---

## 1. Project Overview

**OrbitQueue** is a Linux simulation of satellite downlink packet scheduling for a small satellite ground station link. When contact windows and electrical power are constrained, three distinct streams compete for radio transmission:
1. **TT&C / Housekeeping:** Critical telemetry, tracking, and telecommand data (never deferred or rejected).
2. **SSTV (Slow Scan TV):** Large image frames (deferrable).
3. **Voice / Data:** Digital voice and general scientific measurements.

Four core Operating Systems experiments are integrated into this unified architecture:
- **Experiment 7 (Synchronization):** Bounded priority queue between telemetry producers and router consumers.
- **Experiment 8 (Memory Management):** Paging and segmentation translation simulation with bounds violation checks.
- **Experiment 9 (Deadlock Avoidance):** Banker's Algorithm safety checking before granting CPU, buffer, and transmitter slots.
- **Experiment 10 (Virtual Memory):** FIFO, LRU, and Optimal page replacement algorithms evaluated on simulated memory traces.

---

## 2. Team and Responsibilities

| Member | Primary Focus | Modules & Experiments Owned |
|---|---|---|
| **Nidhi** | Core Pipeline & Sync | Exp 7 (`satellite_sync`), Integrated Router (`router_app`), `packet_queue` |
| **Rimjhim** | Memory Architecture & Automation | Exp 8 (`memory_management`), Exp 10 (`page_replacement`), `mem_trace`, Build & CI |
| **Surbhi** | Data & Deadlock Avoidance | Exp 9 (`bankers`), Resource Manager (`resource_manager`), ML Router, Loaders |

---

## 3. Directory Structure

```
orbitqueue-os-project/
├── .github/workflows/       # GitHub Actions CI workflow
├── data/                    # Synthetic CSV packets, scenario inputs, and test cases
├── docs/                    # Interface specs, project brief, first-run guide, notes
├── include/                 # Shared project headers (common.h, mem_trace.h, resource_manager.h)
├── logs/                    # Execution logs and rejected row logs
├── ml/                      # Python ML model training, generator, and baseline heuristic
├── outputs/                 # Output traces, metrics, decisions, and routing logs
├── report/sections/         # Formal project report sections (01 to 09)
├── screenshots/             # Execution screenshots for report evidence
├── scripts/                 # Automation scripts (env_check.sh, build.sh, run_all.sh)
├── src/                     # C++ source code and executable entrypoints
├── tasks/                   # Individual team member task breakdown files
├── tests/                   # Additional unit and regression test suites
├── Makefile                 # Rimjhim's build configuration
└── README.md                # Project documentation
```

---

## 4. Linux Setup and Execution

All programs must be built and run on native Linux (or WSL2 / Linux VM). Refer to [`docs/FIRST_LINUX_RUN.md`](docs/FIRST_LINUX_RUN.md) for detailed step-by-step instructions.

### Prerequisites (Ubuntu / Debian)
```bash
sudo apt update && sudo apt install -y build-essential python3 python3-pip git
pip3 install -r ml/requirements.txt
```

### Build Commands
```bash
# Clean and build all binaries into build/
make clean && make

# Build ThreadSanitizer binary for race detection
make tsan
```

### Running the System
Always run automation scripts using `bash <script>`:
```bash
# Run environment check
bash scripts/env_check.sh

# Run complete end-to-end simulation pipeline
bash scripts/run_all.sh

# Run self-tests across modules
make test
```

---

## 5. Component Status Table

| Component | Owner | Target / Binary | Status |
|---|---|---|---|
| **Packet Queue** | Nidhi | `src/packet_queue.cpp` | Complete & verified on Linux (mutex + 3 semaphores, critical headroom) |
| **Exp 7 Synchronization** | Nidhi | `build/satellite_sync` | Complete & verified on Linux (safe/unsafe PC, Reader-Writer demo) |
| **Integrated Router App** | Nidhi | `build/router_app` | Complete & verified on Linux (safety override, Banker's check, mem trace) |
| **Memory Trace Module** | Rimjhim | `src/mem_trace.cpp` | stub, NOT yet built or tested on Linux |
| **Exp 8 Memory Management** | Rimjhim | `build/memory_management` | stub, NOT yet built or tested on Linux |
| **Exp 10 Page Replacement** | Rimjhim | `build/page_replacement` | stub, NOT yet built or tested on Linux |
| **Build & Test Automation** | Rimjhim | `Makefile`, `scripts/*.sh` | stub, NOT yet built or tested on Linux |
| **CSV Loaders & Check** | Surbhi | `build/csv_check`, `src/csv_loader.cpp` | stub, NOT yet built or tested on Linux |
| **Exp 9 Banker's Algorithm** | Surbhi | `build/bankers`, `src/resource_manager.cpp` | stub, NOT yet built or tested on Linux |
| **ML Model & Baseline** | Surbhi | `ml/*.py` | stub, NOT yet built or tested on Linux |

---

## 6. Synthetic Data Disclaimer

All data in `data/` (`satellite_data.csv`, `decisions_sample.csv`, `addr_tests.txt`, `page_trace_sample.txt`) is completely synthetic and generated for educational simulation within the Operating Systems course curriculum. It does not reflect real telemetry or transmission logs.
