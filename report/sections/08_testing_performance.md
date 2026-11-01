# Section 8: Testing and Performance Evaluation

## Testing Environment
Testing was performed in an Ubuntu Linux virtual machine. The project was compiled using the repository Makefile and its configured C++17 compiler flags (`-std=c++17 -Wall -Wextra -pthread`).

## Automated Test Matrix

The following test matrix documents verification across all memory subsystem components and pipeline integration, specifying valid test vectors, boundary/invalid cases, expected behavior, and verified execution status.

| Component | Valid test case | Invalid/boundary test case | Expected behavior | Evidence/status |
|---|---|---|---|---|
| Paging | Valid logical page and offset translation (`P 3 99` $\rightarrow$ Frame 5, Physical 599; `P 0 0` $\rightarrow$ Physical 400) | Unmapped page (`P 2 20`), invalid page index (`P 9 5`), offset at or beyond page boundary (`P 4 100`, `P 4 -3`) | Valid addresses map correctly; unmapped pages trigger page faults; out-of-range page numbers and offsets are rejected | **Passed** (`./build/memory_management --selftest` and pipeline test suite on `data/addr_tests.txt`) |
| Segmentation | Valid segment and offset translation within segment limits (`S 1 150` $\rightarrow$ 2150; `S 2 299` $\rightarrow$ 3299) | Offset at or beyond limit (`S 2 300`, `S 2 450`), non-existent segment ID (`S 7 0`, `S -1 0`) | Valid offsets map using base + offset; boundary overruns report segmentation violation; invalid segment IDs are rejected | **Passed** (`./build/memory_management --selftest` and pipeline test suite on `data/addr_tests.txt`) |
| Page Replacement | Multi-algorithm trace simulation across frame counts 1, 3, 4, 5 on standard reference strings for FIFO, LRU, Optimal | Non-numeric trace tokens, invalid frame counts ($<1$), page numbers outside 0–7, Belady anomaly trace (`1 2 3 4 1 2 5 1 2 3 4 5`) | Computes exact fault/hit counts matching hand-verified vectors (FIFO 10, LRU 9, Opt 7); invalid parameters abort with code 1; detects Belady anomaly | **Passed** (`./build/page_replacement --selftest` and pipeline execution on `data/page_trace_sample.txt`) |
| Memory Trace & Concurrency | 2 concurrent worker threads writing 10,000 trace entries each to `mem_trace` buffer (20,000 total recorded accesses) | Thread contention under high-frequency writes, mutex synchronization under heavy concurrency | Concurrent logging records all accesses without dropped entries; total recorded accesses strictly equals 20,000 | **Passed**: The concurrent memory-trace test (`./build/test_mem_trace`) recorded 20,000 accesses across two worker threads. ThreadSanitizer race detection was not verified. |
| Pipeline Integration | End-to-end multi-stage pipeline run via `bash scripts/run_all.sh` across build and evaluation stages | Pipeline run with incomplete upstream modules (CSV validator, ML router, Router app, Exp 9 stubs) and missing runtime artifacts | Incomplete pipeline stages are skipped, the pipeline falls back to sample datasets, executes completed memory modules, and the overall script finishes with exit code 0 | **Partial Pass** (The overall pipeline script completed with exit code 0, reporting 3 passed, 4 skipped, and 0 failed; full system integration not verified due to skipped upstream stubs) |
| Upstream Components (CSV Validator, ML Router, Router App, Exp 9 Banker's) | N/A (Assigned to teammates) | Incomplete or unintegrated stage stubs | Expected to run end-to-end once implemented by respective team members | **Not verified** (Currently remaining as stubs in repository) |

## Build and Unit Tests
The following checks were completed:

- `make clean` completed successfully.
- `make` compiled the project without warnings.
- `make test` passed.
- The memory-trace test passed, including concurrent writes from two threads and verification of 20,000 recorded logical accesses.
- Experiment 8 self-tests passed.
- Experiment 10 self-tests passed, including the FIFO Belady anomaly test.

## Pipeline Integration Results
The command `bash scripts/run_all.sh` completed with exit code 0. However, this indicates that the pipeline finished without a reported fatal error; it does not mean that every component was implemented or executed.

| Pipeline component | Status | Notes |
|---|---|---|
| Environment check | Passed | Linux environment information recorded |
| Build | Passed | Project compiled |
| CSV validation | Skipped | Component remains a stub |
| ML router training | Skipped | Component remains a stub |
| Router application | Skipped | Component remains a stub |
| Experiment 8 address tests | Passed | Address translation tests executed |
| Experiment 9 Banker's Algorithm | Skipped | Component remains a stub |
| Experiment 10 evaluation | Passed | Page replacement evaluation executed |

Overall pipeline summary: **3 passed, 4 skipped, 0 failed**.

## Performance Measurements
The pipeline was measured using `/usr/bin/time -v bash scripts/run_all.sh`.

| Metric | Measured value |
|---|---:|
| Elapsed wall-clock time | 0.14 seconds |
| Maximum resident set size | 10,132 KB |
| User CPU time | 0.09 seconds |
| System CPU time | 0.08 seconds |
| CPU utilization | 126% |
| Minor page faults | 16,693 |
| Major page faults | 2 |
| Voluntary context switches | 130 |
| Involuntary context switches | 39 |

These values describe one run in the Ubuntu virtual machine. They are not hardware performance measurements from an actual satellite.

## Artifact Validation
The measurement script reported that three expected integration artifacts were missing:

- `outputs/routing_log.csv`
- `outputs/page_trace.txt`
- `outputs/routing_summary.txt`

The generated `outputs/decisions.csv` was a fallback sample dataset, not verified output from an implemented ML router. The page replacement evaluation used the sample reference trace in `data/page_trace_sample.txt`, rather than a trace generated by the integrated router.

## Screenshots
<!-- Placeholder: Add real screenshots captured from Linux execution once taken. -->
- `screenshots/test_suite.png`: Terminal output of `make test` executing the test suite and concurrency check.
- `screenshots/pipeline_execution.png`: Output of `bash scripts/run_all.sh` demonstrating pipeline execution and step reporting.
- `screenshots/performance_measure.png`: Output of `bash scripts/measure.sh` showing resource consumption metrics.

## Limitations and Next Steps
The current implementation verifies the memory-management experiments and records partial pipeline measurements. Full integration testing remains incomplete because the CSV validator, ML router, routing application, and Experiment 9 component are still stubs or skipped.

Before claiming full system integration, these components must be implemented and tested, the missing integration artifacts must be generated, and the complete pipeline must be rerun. Screenshots of the actual Linux terminal output will be added as supporting evidence.
