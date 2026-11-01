# TASK: Nidhi (Core pipeline owner: Exp 7 and the integrated router)

Put this file in the repo at `tasks/TASK_NIDHI.md`.
Read `docs/PROJECT_BRIEF.md` and `docs/INTERFACES.md` first.

**Status convention:** change `[ ]` to `[x]` only after the task was built and tested on Linux. Never mark done based on writing code alone.

---

## Rules for the AI agent working on this file

- You are helping Nidhi. Work through the tasks below in order.
- Edit only the files listed under "My files". If something needs to change in another owner's file, stop and tell the human to ask that person.
- Build and test on Linux (WSL or the Ubuntu VM) only. If you are running on native Windows, stop and tell the human.
- Never invent output, screenshots or measurements. Run the command, save the real output with `tee` into `outputs/`.
- Keep code simple and well commented so Nidhi can explain every line in the viva. No clever abstractions.
- Commit to a branch named `nidhi/<topic>` and open a Pull Request. Do not push to `main`.
- Screenshots, video recording and VM runs are human tasks. Ask the human to do them.

## My files

`src/packet_queue.h`, `src/packet_queue.cpp`, `src/satellite_sync.cpp`, `src/router_app.cpp`, `docs/ACCESS_PATTERN.md`, `docs/architecture.*`, `README.md`, `report/sections/01_intro_scope.md`, `02_architecture_tools.md`, `04_exp7.md`.

## What I need from others, and what I give

- From Surbhi: real `load_packets` and `load_decisions`, `outputs/decisions.csv`, real `resource_manager`. Until then the scaffold stubs work, so I am not blocked.
- From Rimjhim: hardened `mem_trace`. Same, stub works meanwhile.
- To Rimjhim: the real `outputs/page_trace.txt` produced by my router.
- Review duty: I review Rimjhim's Pull Requests.

---

## Part A. Experiment 7: Producer-Consumer with semaphores and mutex

Use-case link: producers create satellite packets, the router consumes them from a bounded shared queue.

### A1. Bounded priority queue (`packet_queue.*`)
- [x] 8 slots stored in an array, protected by one `pthread_mutex_t`.
- [x] Semaphores: `empty` (starts at 8), `full` (starts at 0), `bulk` (starts at 6).
- [x] SSTV and VOICE producers take `bulk` first, so they can never fill the last 2 slots. TT&C and HOUSEKEEPING skip `bulk`.
- [x] Always acquire in the same order: `bulk`, then `empty`, then mutex. This avoids deadlock. After removing a bulk packet, the consumer posts `bulk` then `empty`.
- [x] `pop()` removes the highest-priority packet (`type_priority`), ties broken by arrival order, while holding the mutex.
- [x] Stats under the mutex: max depth, number of times a producer blocked on full, number of times the consumer blocked on empty.
- [x] Constructor and destructor initialise and destroy every semaphore and the mutex correctly.

### A2. Producer-Consumer demo (`satellite_sync.cpp`, `--demo=pc`)
- [x] Two producer threads: TelemetryProducer (TT&C, HOUSEKEEPING) and PayloadProducer (SSTV, VOICE), reading packets from the CSV via `load_packets`.
- [x] One consumer thread (the router). The run is finite: the consumer stops when consumed equals the total produced. All threads are joined.
- [x] Clear log lines such as `[PRODUCER] packet 17 SSTV added (depth 5)` and `[CONSUMER] packet 3 TT&C removed (depth 4)`.
- [x] `--producer-delay-ms` and `--consumer-delay-ms` only shape the scenario (force a full or empty queue). They are not the proof of correctness.
- [x] Log `BLOCKED: queue full` and `BLOCKED: queue empty` when blocking actually happens.
- [x] End-of-run audit: produced count, consumed count, every `packet_id` seen exactly once, depth never above 8 or below 0, no TT&C lost. Print `AUDIT PASS` or `AUDIT FAIL`. Exit 2 on failure in safe mode.

### A3. Unsafe comparison (`--mode=unsafe`)
- [x] Separate, clearly labelled DEMONSTRATION-ONLY queue with no mutex and no semaphores. Use indices with modulo so a race causes lost or duplicated items but never an out-of-bounds crash.
- [x] Run `--iterations` (default 200000) and report anomalies: duplicates, lost packets, count mismatch.
- [x] Be honest: if a run happens not to show an anomaly, raise iterations and say so in the report. Never claim a failure that did not happen.

### A4. ThreadSanitizer evidence
- [x] Use `make tsan` (target provided by Rimjhim's Makefile). Run safe and unsafe modes. Safe should report no data race, unsafe should report data races. Save both outputs.

### A5. Reader-Writer part (`--demo=rw`)
- [x] Shared telemetry state (`battery_level`, `link_quality`). One writer thread updates it, the router and a dashboard thread read it.
- [x] Reader-preference solution with `readCount` mutex and `writeLock` semaphore (reuse your existing lab code).
- [x] Fix the garbled output from your lab run: build each log line as one string and print it under a log mutex.
- [x] Show that readers can overlap but a writer is always alone.

### A6. Exp 7 tests and evidence
- [x] Normal run. [x] Full queue. [x] Empty queue. [x] Safe audit. [x] Unsafe run. [x] TSan safe and unsafe. [x] Reader-Writer.
- [x] Save: `outputs/exp7_safe.txt`, `exp7_full_queue.txt`, `exp7_empty_queue.txt`, `exp7_unsafe.txt`, `exp7_tsan_safe.txt`, `exp7_tsan_unsafe.txt`, `exp7_readerwriter.txt`.
- [x] Human takes screenshots into `screenshots/exp7_*.png` after the real runs.

Suggested commands:
```
./build/satellite_sync --demo=pc --mode=safe | tee outputs/exp7_safe.txt
./build/satellite_sync --demo=pc --consumer-delay-ms=50 | tee outputs/exp7_full_queue.txt
./build/satellite_sync --demo=pc --producer-delay-ms=50 | tee outputs/exp7_empty_queue.txt
./build/satellite_sync --mode=unsafe --iterations=200000 | tee outputs/exp7_unsafe.txt
make tsan
./build/satellite_sync_tsan --mode=safe 2>&1 | tee outputs/exp7_tsan_safe.txt
./build/satellite_sync_tsan --mode=unsafe 2>&1 | tee outputs/exp7_tsan_unsafe.txt
./build/satellite_sync --demo=rw | tee outputs/exp7_readerwriter.txt
```

---

## Part B. Integrated router application (`router_app.cpp`)

This is what turns four experiments into one application.

- [x] Load packets with `load_packets` and decisions with `load_decisions`. Missing file gives a clear error and exit 1.
- [x] Producers push packets into `PacketQueue`. `--workers N` (default 2) router worker threads pop and process packets concurrently.
- [x] For each packet: look up the decision. If missing, use a simple fallback rule and mark source `FALLBACK`.
- [x] **Safety override:** if the type is critical and the decision is DEFER or REJECT, force TRANSMIT and log `SAFETY_OVERRIDE`. Count overrides.
- [x] **Resource check (Exp 9 link):** use the task mapping in `docs/INTERFACES.md` section 5. Call `rm::request` before the step, hold briefly (short `nanosleep`), then `rm::release`. `WAIT` means retry with backoff. `DENIED_UNSAFE` for a non-critical packet means DEFER with reason `RESOURCE_DENIED`. Critical packets are retried until granted (cap at 1000 retries, then log an error).
- [x] **Memory access trace (Exp 8 and 10 link):** write `docs/ACCESS_PATTERN.md` first, a short deterministic rule listing which region and offset each step touches (for example: CODE per stage, MODEL_PARAMS depending on type, INPUT_BUFFER depending on the packet's slot and size, OUTPUT_LOG at the running log position, STACK by call depth). Ask Rimjhim to review it. Then call `memtrace::access` accordingly.
- [x] Ground-station simulation: print `[GS] packet 17 TT&C -> TRANSMITTED` or `... DEFERRED (reason)` or `... REJECTED (reason)` and write `outputs/routing_log.csv` with the columns in `INTERFACES.md`.
- [x] Summary to `outputs/router_summary.txt`: counts of transmitted, deferred, rejected, safety overrides, resource waits, resource denials, max queue depth, runtime in ms, throughput in packets per second.
- [x] Failure tests: missing CSV, missing decisions file, empty CSV, packet id absent from decisions, full queue. Each ends with a clear message and no crash.

Acceptance: `./build/router_app` runs end to end with real data, critical packets are never deferred or rejected, `outputs/page_trace.txt` is produced and has the format in `INTERFACES.md`.

---

## Part C. Documentation and repo admin

- [x] Create the GitHub repo, add Rimjhim and Surbhi as collaborators, run the scaffold prompt, push to `main`.
- [x] `README.md`: overview, Linux setup, folder structure, build and run commands, data disclaimer (synthetic), how to run each experiment, where results are saved, honest status of each part. Keep it updated as others merge.
- [x] Labelled architecture diagram (draw.io or Mermaid): application, data flow, threads, queue, semaphores, mutex, files, memory management. Save source and PNG in `docs/`.
- [x] Report sections: `01_intro_scope.md` (problem statement, objectives, scope), `02_architecture_tools.md` (diagram, Linux version, compiler, languages, libraries), `04_exp7.md` (lab format, with interpretation of mutex and semaphore behaviour).
- [ ] Review Rimjhim's Pull Requests (builds with no warnings, tests pass, code understandable, no edits to files she does not own).
- [ ] Tag the final commit `v1.0` after Surbhi's packaging check.

## Part D. Demo video

- [ ] Record two segments on Linux: intro and environment (0:00 to 0:30), then Exp 7 safe run, full and empty queue, unsafe run and audit, router end to end (1:15 to 2:15). Send the clips to Rimjhim for editing.

## My definition of done

- [x] Exp 7 passes every test above and the evidence files exist.
- [x] Router runs end to end and the safety override works.
- [x] README, diagram and my three report sections are complete.
- [x] I can explain: why one mutex and three semaphores, what happens if the wait order is swapped, what a race looks like in my unsafe run, why TT&C has reserved slots.
