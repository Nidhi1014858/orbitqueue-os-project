# TASK: Surbhi (Data and AI owner: ML model, Exp 9, loaders, report assembly)

Put this file in the repo at `tasks/TASK_SURBHI.md`.
Read `docs/PROJECT_BRIEF.md` and `docs/INTERFACES.md` first.

**Status convention:** change `[ ]` to `[x]` only after the task was built and tested on Linux. Never mark done based on writing code alone.

---

## Rules for the AI agent working on this file

- You are helping Surbhi. Work through the tasks below in order.
- Edit only the files listed under "My files". If something needs to change in another owner's file, stop and tell the human to ask that person.
- You may write and edit code on Windows. Building, testing, evidence, and completion claims are Linux-only (WSL or the Ubuntu VM). On Windows, never claim the code builds or passes; leave every checkbox unchecked until the human runs it on Linux and pastes the real output.
- Write all text and source files with LF line endings.
- Never invent output, screenshots or measurements. Run the command, save the real output with `tee` into `outputs/`.
- Keep code simple and well commented so Surbhi can explain every line in the viva. No clever abstractions.
- Commit to a branch named `surbhi/<topic>` and open a Pull Request. Do not push to `main`.
- Screenshots, packaging checks and VM runs are human tasks. Ask the human to do them.

## My files

`include/common.h`, `src/common.cpp`, `src/csv_loader.cpp`, `src/csv_check.cpp`, `include/resource_manager.h`, `src/resource_manager.cpp`, `src/bankers.cpp`, `ml/*`, `data/*.csv`, `docs/LABEL_POLICY.md`, `report/sections/03_data_ml.md`, `06_exp9.md`, `09_limitations_refs_ai.md`.

## What I need from others, and what I give

- To Nidhi: real loaders, `outputs/decisions.csv`, real `resource_manager`. The scaffold stubs keep her unblocked until I merge.
- To Rimjhim: nothing blocks her. She reviews my Pull Requests.
- Review duty: I review Nidhi's Pull Requests.

---

## Part A. Synthetic data and loaders

Use-case link: this is the input of the whole simulation. All data is synthetic and must be labelled so.

- [x] `ml/gen_data.py` with a fixed seed (`--seed 42`). Generates `data/satellite_data.csv` (about 60 packets) and `data/training_scenarios.csv` (about 600 scenarios with a `label` column). Column formats are in `docs/INTERFACES.md` section 2.
- [x] The 60 packets contain all four types with a realistic mix and include edge cases near the decision thresholds. `packet_id` is unique.
- [x] `data/satellite_data_bad.csv` with deliberate bad rows: unknown type `XYZ`, battery 140, negative size, empty field, duplicate id, non-numeric urgency.
- [x] `src/csv_loader.cpp`: implement `load_packets` and `load_decisions` as in `INTERFACES.md`. Check the header. Reject bad rows with line number and reason into `logs/rejected_rows.log`, continue with valid rows, return the rejected count. A missing file returns false with a clear message.
- [x] `src/csv_check.cpp`: `csv_check FILE` prints valid and rejected counts.
- [x] Tests: good file gives 0 rejected, bad file gives the expected rejections, missing file gives a clear error and exit 1. Save to `outputs/data_validation.txt`.
- [x] Add a data disclaimer line in the CSV generator output and in the report: values are synthetic and are not SomaiyaSat telemetry.

## Part B. ML model and rule-based baseline (this is the AI component, worth 20 marks together with the router)

- [x] `docs/LABEL_POLICY.md`: document how labels are created, for example low battery gives REJECT or DEFER, weak link gives DEFER, high urgency with good link and enough battery gives TRANSMIT. Add about 5 percent label noise and overlapping boundaries so the task is not trivially learnable.
- [x] `ml/train_router.py`: scikit-learn `DecisionTreeClassifier` (depth 4 to 5), 80/20 stratified split, fixed seed. Save accuracy, per-class precision and recall, confusion matrix and feature importance to `outputs/ml_metrics.txt`, and the tree rules (`export_text`) to `outputs/tree_rules.txt`.
- [x] `ml/baseline.py`: a simple hand-written threshold rule, clearly labelled BASELINE. Evaluate it on the same test split and compare with the tree.
- [x] Predict on `data/satellite_data.csv` and write `outputs/decisions.csv` (source `ML`) and `outputs/decisions_baseline.csv` (source `BASELINE`).
- [x] `ml/requirements.txt` (scikit-learn, pandas, numpy) and install instructions in the README section I own.
- [x] Be honest in the report: labels come from a synthetic policy, so accuracy shows the model can learn that policy, not that it works on real satellite traffic. State why a decision tree was chosen (small, explainable, no heavy hardware).
- [x] The safety override lives in Nidhi's router, not in the model. Describe how the two fit together.
- [ ] Do not call the ML part complete until it is trained, tested and connected to the router.

## Part C. Experiment 9: Banker's Algorithm (`resource_manager.*` and `bankers.cpp`)

Use-case link: tasks T0 to T4 ask for CPU slots (A), buffer units (B) and transmitter slots (C). Example danger: DownlinkPrep holds buffer units while waiting for the transmitter, and the router holds the transmitter while waiting for buffer units. That is a circular wait.

### Base state S1 (your existing matrices)
- Allocation: T0 (1,0,1), T1 (1,1,0), T2 (1,2,1), T3 (0,1,1), T4 (1,0,0)
- Max: T0 (3,2,2), T1 (2,2,1), T2 (3,3,3), T3 (1,2,2), T4 (2,1,1)
- Need = Max - Allocation: T0 (2,2,1), T1 (1,1,1), T2 (2,1,2), T3 (1,1,1), T4 (1,1,1)
- Available: (2,2,1)

### Tasks
- [x] `resource_manager.cpp`: implement `init` with validation (counts positive, no negative values, Allocation not above Max, no inconsistent values), `is_safe` (your safety algorithm), `request`, `release`, `snapshot`. One internal mutex so the router's worker threads can call it safely.
- [x] `request` steps: if request exceeds Need then `ERROR_EXCEEDS_NEED`. If request exceeds Available then `WAIT`. Otherwise tentatively grant, run `is_safe`, keep the grant if safe (`GRANTED`) or roll back and return `DENIED_UNSAFE`.
- [x] `bankers.cpp`: print Allocation, Max, Need, Available, each safety step and the final safe sequence. `--scenario S1|S2|S3|all`, `--interactive` (your lab style input), `--selftest` (exit 2 on mismatch).
- [x] Be careful with wording: an unsafe state is not proof that a deadlock has already happened. It means a safe completion order cannot be guaranteed under the declared maximum needs.

### Scenarios and expected results (hand-verified, the program must reproduce them)
- **S1 safe:** Available (2,2,1). Safe sequence T0, T1, T2, T3, T4.
- **S3 unsafe:** same matrices, Available (0,1,0). No task can finish, so no safe sequence.
- **S2 requests**, starting each time from Available (1,2,1):
  - T1 requests (1,1,1): GRANTED, safe sequence T1, T3, T4, T0, T2.
  - T0 requests (1,2,1): tentative Available becomes (0,0,0), no task can finish, so DENIED_UNSAFE and rolled back.
  - T2 requests (2,1,2): WAIT, because C needs 2 but only 1 is available.
  - T2 requests (1,2,0): ERROR_EXCEEDS_NEED, because B exceeds the declared need of 1.
- **Invalid input:** negative count, Allocation above Max, zero tasks. Each gives a clear error and exit 1.

### Evidence and report
- [x] `./build/bankers --scenario all | tee outputs/exp9_scenarios.txt` and `./build/bankers --selftest | tee outputs/exp9_selftest.txt`
- [ ] Human takes screenshots into `screenshots/exp9_*.png`.
- [ ] Report `06_exp9.md` in lab format. Post-lab: you already have the prevention, avoidance, detection and recovery table in the satellite context. Make it concrete by naming the transmitter slot and buffer units in each row. Fix the Outcomes field (the lab file text about OS structure and I/O does not match this experiment, take the CO wording from your syllabus).

---

## Part D. Report assembly and delivery packaging

- [x] Write `03_data_ml.md` (dataset source, synthetic generation method, labelling policy, model, baseline comparison) and `09_limitations_refs_ai.md` (limitations, privacy note, possible improvements, references, AI acknowledgement).
- [x] Limitations to cover: synthetic data, abstract resources, simulated memory (not a real MMU), small decision tree, no real radio link.
- [x] Add an honest AI acknowledgement that distinguishes the Surbhi components verified on Linux from team-owned components still awaiting their owners' verification.
- [ ] Assemble `report/sections/*.md` from all three members into the single consolidated report PDF (cover page with each member's name, roll number and batch, then the nine sections in order, then references). Check every item in section 4 of the assignment brief.
- [ ] Packaging: ZIP of the repo (no `build/`), file names like `RollNo_Name_OS_UseCase_Report.pdf` and `RollNo_Name_OS_UseCase_Code.zip`, GitHub link in the report, README with reproducible steps, video link accessible to faculty.
- [ ] Final check from a clean clone in the VM that the README steps work as written.
- [ ] Review Nidhi's Pull Requests (queue logic, audit, router safety override, resource handling).

## Part E. Demo video

- [ ] Record data, validation, ML versus baseline (0:30 to 1:15), Exp 9 (2:15 to 3:00) and the closing summary with the other two (4:30 to 5:00). Send clips to Rimjhim for editing.

## My definition of done

- [x] Loaders reject bad rows and the model beats or honestly compares with the baseline.
- [ ] `outputs/decisions.csv` is produced and the router reads it.
- [x] Exp 9 shows a safe state, an unsafe state, a denied request and an invalid input.
- [ ] My three report sections are complete and the final report PDF is assembled.
- [ ] I can explain: unsafe state versus deadlock, why a request is denied, what Need means, why a decision tree, what label noise does, why synthetic accuracy is limited evidence.
