# OrbitQueue: First Run on Linux

> **IMPORTANT NOTICE:** None of the build steps, scripts, tests, or binaries have been run or verified on this Windows machine. Everything in this repository is currently a scaffold stub. Follow the steps below inside an actual Linux environment (Ubuntu 22.04 LTS / 24.04 LTS, WSL2, or a Linux virtual machine).

---

## 1. Prerequisites (Ubuntu / Debian)

Ensure system packages and the g++ C++17 compiler toolchain are installed:

```bash
sudo apt update && sudo apt install -y build-essential python3 python3-pip git
```

Install Python dependencies for machine learning:

```bash
pip3 install -r ml/requirements.txt
```

---

## 2. Execution Sequence

Execute the following commands in exact order from the repository root:

### Step 1: Clean and Build Binaries
```bash
make clean && make
```
*Expected:* Binaries (`build/satellite_sync`, `build/router_app`, `build/memory_management`, `build/page_replacement`, `build/bankers`, `build/csv_check`) should compile into `build/` with zero warnings.

### Step 2: Run End-to-End Orchestration Script
```bash
bash scripts/run_all.sh
```
*Expected:* All steps should report `PASS` or `SKIPPED (stub / not implemented)` (exit code 77). No step should report `FAIL`.

### Step 3: Run Self-Tests
```bash
make test
```
*Expected:* Runs `--selftest` across the binaries and treats exit code 77 as cleanly skipped.
