# SomaiyaSat Downlink Router: Project Brief

Team: Nidhi (Roll 16010424106) | Rimjhim (Roll ________) | Surbhi (Roll ________)
Batch: B2 | Use case: KJS-SRS-01 (SomaiyaSat and SomaiyaPod) | OS Experiments: 7, 8, 9, 10

Put this file in the repo at `docs/PROJECT_BRIEF.md`. Read it together with `docs/INTERFACES.md` before touching any code.

---

## 1. What we are building

A Linux simulation of how a small satellite decides what to send to a ground station when power and link time are limited. Three kinds of data compete for the link: TT&C/housekeeping (critical), SSTV images (large, deferrable) and voice/data. A small ML model recommends TRANSMIT, DEFER or REJECT for each packet, and a hard safety rule always protects TT&C and housekeeping.

The four OS experiments are not separate demos. They are the machinery inside this one application:

- **Exp 7 (Producer-Consumer):** the shared packet queue between data producers and the router.
- **Exp 9 (Banker's):** resource safety check before a task gets CPU slots, buffer units or the transmitter.
- **Exp 8 (Paging and Segmentation):** the memory map of the router program and address translation, including an oversized input frame causing a violation.
- **Exp 10 (FIFO, LRU, Optimal):** page replacement on a trace produced by the router's own memory accesses.

Everything is simulated and all data is synthetic. Nothing talks to a real satellite or radio.

## 2. Simulation flow

```
 1. Generate synthetic data            ml/gen_data.py              -> data/*.csv
 2. Load and validate                  csv_loader                  -> bad rows to logs/rejected_rows.log
 3. Train model + baseline, predict    ml/train_router.py          -> outputs/decisions.csv
 4. Producers push packets             Exp 7: bounded priority queue (8 slots, 2 reserved for critical)
 5. Router workers pop highest priority packet, look up its decision
 6. Safety override                    TT&C / HOUSEKEEPING can never be DEFERRED or REJECTED
 7. Resource check                     Exp 9: Banker's safety check before CPU/buffer/transmitter are granted
 8. Every step logs a memory access    (region, offset) -> outputs/page_trace.txt
 9. Ground-station simulation          transmitted / deferred / rejected + reason -> outputs/routing_log.csv
10. After the run:
      Exp 8  translates addresses with the segment table and page table, shows page fault and violation
      Exp 10 runs FIFO, LRU, Optimal on outputs/page_trace.txt at several frame counts
```

## 3. Shared vocabulary (use these exact names everywhere)

- Packet types: `TT&C`, `HOUSEKEEPING`, `SSTV`, `VOICE` (VOICE covers Codec2/M17 voice and data).
- Priority: TT&C > HOUSEKEEPING > VOICE > SSTV.
- Actions: `TRANSMIT`, `DEFER`, `REJECT`.
- Tasks (Exp 9): T0 TelemetryHandler, T1 PacketRouter, T2 SSTVProcessor, T3 VoiceCodec, T4 DownlinkPrep.
- Resources (Exp 9): A = CPU slots, B = buffer units, C = transmitter slots. Abstract units, not real satellite figures.
- Memory regions (Exp 8 and 10): CODE, MODEL_PARAMS, INPUT_BUFFER, OUTPUT_LOG, STACK.

## 4. Who does what (equal split, everyone owns real work)

| | Nidhi | Rimjhim | Surbhi |
|---|---|---|---|
| OS experiments | Exp 7 | Exp 8 and Exp 10 | Exp 9 |
| Application part | Integrated router app, safety override, memory-access pattern | Memory map and trace module, trace analysis | Data generation, CSV loaders, ML model and baseline, Banker's library |
| Engineering and delivery | Repo admin, architecture diagram, README | Makefile, run_all, test matrix, measurements | Final report assembly, ZIP and links packaging |
| Report sections | Intro and scope, architecture and tools, Exp 7 | Exp 8, Exp 10, testing and performance | Data and ML, Exp 9, limitations, references, AI acknowledgement |
| Demo video segment | Intro, Exp 7 | Exp 8, Exp 10, video editing | Data and ML, Exp 9, closing |
| Code review | Reviews Rimjhim | Reviews Surbhi | Reviews Nidhi |

Each person's detailed list is in `tasks/TASK_<NAME>.md`.

## 5. Phases and handoffs

- **Phase 0 (about 1 hour):** Nidhi runs the scaffold prompt, pushes to `main`. Rimjhim and Surbhi clone and create their branches.
- **Phase 1 (parallel):** each member builds their own experiment or module against the stubs. No one waits for anyone.
- **Handoffs:**
  - Surbhi gives Nidhi the real loaders, `outputs/decisions.csv` and the real `resource_manager`.
  - Rimjhim gives Nidhi a hardened `mem_trace`.
  - Nidhi gives Rimjhim the real `outputs/page_trace.txt` produced by the router.
- **Phase 2 (integration):** Nidhi finishes the router, `scripts/run_all.sh` goes green end to end.
- **Phase 3 (evidence):** each member re-runs their own commands on a clean clone in Linux, saves outputs, takes screenshots, writes report sections, records their video segment.
- **Phase 4 (review and viva):** cross-review, then every member practises explaining all four experiments.

## 6. Team Git workflow

1. Nidhi creates an empty GitHub repo `somaiyasat-os-project` and adds Rimjhim and Surbhi as collaborators (Settings, Collaborators).
2. Nidhi clones it into Linux (WSL or VM), copies the docs in, runs the scaffold prompt and pushes to `main`.
3. Everyone else: `git clone <url>`, then `git checkout -b <name>/<topic>`.
4. Work only on your own files (ownership table in `docs/INTERFACES.md`). Commit small and often.
5. Push your branch and open a Pull Request. The assigned reviewer must approve before merge.
6. After every merge: `git checkout main && git pull`.
7. Never commit generated binaries (`build/`). Commit real outputs and screenshots only after real runs.

## 7. Environment

- Everything must build and run on Linux (WSL Ubuntu or the Ubuntu VM). A Windows MinGW build does not count.
- Antigravity runs on your laptop. If it can open the repo through WSL, build and test directly there. If not, edit in Antigravity, push, `git pull` in the VM or WSL, and build there.
- Install: `sudo apt update && sudo apt install -y build-essential python3 python3-pip git`
- Evidence (outputs and screenshots) is valid only when it comes from Linux.

## 8. Ground rules

- No fabricated screenshots, outputs or measurements. If a run fails, record the failure and the fix.
- All data is synthetic and labelled so. Never call it real SomaiyaSat telemetry.
- Agents (Antigravity) write drafts, humans verify. Every member must understand the code they submit and be able to explain it in the viva.
- Acknowledge AI assistance in the report.
- Do not mark a task `[x]` until it was actually built and tested.
- Check with faculty that team submission is allowed. The brief asks for student name, roll number and a viva, so every member must be able to explain all four experiments, not just their own.

## 9. Definition of done

- All programs build with no warnings and pass valid and invalid tests on Linux.
- `scripts/run_all.sh` works from a clean clone and ends with all steps PASS (no SKIPPED).
- Exp 7 shows safe versus unsafe. Exp 9 shows a safe state, an unsafe state and a denied request. Exp 8 shows a page fault and a segmentation violation. Exp 10 compares three algorithms at three or more frame counts.
- ML model trained, compared against the baseline and connected to the router.
- Report PDF, code ZIP, GitHub link and 5-minute video link are ready and open for faculty.

## 10. Demo video plan (5 minutes, each member records their segment)

- 0:00 to 0:30 Nidhi: use case, Linux environment.
- 0:30 to 1:15 Surbhi: data, validation, ML versus baseline, decisions.
- 1:15 to 2:15 Nidhi: Exp 7 safe run, full and empty queue, unsafe run and audit, router end to end.
- 2:15 to 3:00 Surbhi: Exp 9 safe state, denied request, unsafe state.
- 3:00 to 3:45 Rimjhim: Exp 8 translation, page fault, segmentation violation.
- 3:45 to 4:30 Rimjhim: Exp 10 comparison table and interpretation.
- 4:30 to 5:00 all three: one failure case, measurements, limitations.
