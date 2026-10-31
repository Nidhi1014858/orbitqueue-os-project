# INTERFACES: the contract between Nidhi, Rimjhim and Surbhi

Put this file in the repo at `docs/INTERFACES.md`. These interfaces are what let the three of you work in parallel without waiting for each other. Change anything here only after all three agree, and update this file in the same Pull Request.

---

## 1. File ownership (edit only your own files)

**Nidhi**
- `src/packet_queue.h`, `src/packet_queue.cpp`
- `src/satellite_sync.cpp` (Exp 7)
- `src/router_app.cpp` (integrated application)
- `docs/ACCESS_PATTERN.md`, `docs/architecture.*`, `README.md`
- `report/sections/01_intro_scope.md`, `02_architecture_tools.md`, `04_exp7.md`

**Rimjhim**
- `include/mem_trace.h`, `src/mem_trace.cpp`
- `src/memory_management.cpp` (Exp 8)
- `src/page_replacement.cpp` (Exp 10)
- `Makefile`, `scripts/*`, `tests/*`, `.github/*`
- `data/addr_tests.txt`, `data/page_trace_sample.txt`
- `report/sections/05_exp8.md`, `07_exp10.md`, `08_testing_performance.md`

**Surbhi**
- `include/common.h`, `src/common.cpp`, `src/csv_loader.cpp`, `src/csv_check.cpp`
- `include/resource_manager.h`, `src/resource_manager.cpp`, `src/bankers.cpp` (Exp 9)
- `ml/*`, `data/*.csv`, `docs/LABEL_POLICY.md`
- `report/sections/03_data_ml.md`, `06_exp9.md`, `09_limitations_refs_ai.md`

If you need a change in someone else's file, ask them in the team chat or open a GitHub issue assigned to them. Do not edit it yourself.

## 2. Data files

`data/satellite_data.csv` (packets to route)
```
packet_id,type,urgency,link_quality,battery_level,data_size
```
- `packet_id`: unique positive integer.
- `type`: exactly one of `TT&C`, `HOUSEKEEPING`, `SSTV`, `VOICE` (case sensitive).
- `urgency`: integer 1 to 5. `link_quality`: integer 0 to 100. `battery_level`: integer 0 to 100.
- `data_size`: integer bytes, 1 to 1000000.

`data/training_scenarios.csv` (ML training data)
```
scenario_id,type,urgency,link_quality,battery_level,data_size,label
```
- `label`: `TRANSMIT`, `DEFER` or `REJECT`.

`outputs/decisions.csv` (model output read by the router)
```
packet_id,action,source
```
- `action`: `TRANSMIT`, `DEFER` or `REJECT`. `source`: `ML` or `BASELINE`.

`outputs/routing_log.csv` (router output, owner Nidhi)
```
packet_id,type,decision,source,final_action,reason,resource_result,timestamp_ms
```

`outputs/page_trace.txt` (router output, read by Exp 10)
- Lines starting with `#` are comments. Every other line is one page number (0 to 7), in order of access.

`data/addr_tests.txt` (Exp 8 test input)
- Lines starting with `#` are comments. `P <page> <offset>` is a paging test. `S <segment> <offset>` is a segmentation test.

## 3. Headers (exact signatures)

### include/common.h (owner: Surbhi, scaffold writes the first version)

```cpp
#pragma once
#include <map>
#include <string>
#include <vector>

enum class PacketType { TTC = 0, HOUSEKEEPING = 1, SSTV = 2, VOICE = 3 };
enum class Action { TRANSMIT = 0, DEFER = 1, REJECT = 2 };

struct Packet {
    int id;
    PacketType type;
    int urgency;        // 1..5
    int link_quality;   // 0..100
    int battery_level;  // 0..100
    int data_size;      // bytes, 1..1000000
};

const char* type_name(PacketType t);            // "TT&C", "HOUSEKEEPING", "SSTV", "VOICE"
bool parse_type(const std::string& s, PacketType& out);
int  type_priority(PacketType t);               // TTC 4, HOUSEKEEPING 3, VOICE 2, SSTV 1
bool is_critical(PacketType t);                 // TTC or HOUSEKEEPING
const char* action_name(Action a);              // "TRANSMIT", "DEFER", "REJECT"
bool parse_action(const std::string& s, Action& out);

// src/csv_loader.cpp
// Returns false if the file is missing/unreadable (prints a clear error).
// Invalid rows are skipped, counted in rejected_count and logged with line number and reason.
bool load_packets(const std::string& path, const std::string& reject_log_path,
                  std::vector<Packet>& out, int& rejected_count);
bool load_decisions(const std::string& path, std::map<int, Action>& out,
                    std::map<int, std::string>* sources = nullptr);
```

### include/mem_trace.h (owner: Rimjhim)

```cpp
#pragma once
#include <string>

namespace memtrace {
constexpr int PAGE_SIZE = 100;   // bytes (synthetic teaching value)
constexpr int NUM_PAGES = 8;

// The enum value is also the segment number used in Exp 8.
enum Region { CODE = 0, MODEL_PARAMS = 1, INPUT_BUFFER = 2, OUTPUT_LOG = 3, STACK = 4 };

const char* region_name(Region r);
int region_first_page(Region r);   // CODE 0, MODEL_PARAMS 2, INPUT_BUFFER 4, OUTPUT_LOG 6, STACK 7
int region_num_pages(Region r);    // 2, 2, 2, 1, 1
// first_page + (offset % (num_pages * PAGE_SIZE)) / PAGE_SIZE ; a negative offset maps to the first page
int page_of(Region r, int offset);

bool open_trace(const std::string& path);   // writes a '#' header line
void access(Region r, int offset);          // thread-safe; appends page_of(r, offset)
void close_trace();
}
```

### include/resource_manager.h (owner: Surbhi)

```cpp
#pragma once
#include <string>

namespace rm {
constexpr int NUM_TASKS = 5;
constexpr int NUM_RES = 3;   // A = CPU slots, B = buffer units, C = transmitter slots

enum Task { T0_TELEMETRY_HANDLER = 0, T1_PACKET_ROUTER = 1, T2_SSTV_PROCESSOR = 2,
            T3_VOICE_CODEC = 3, T4_DOWNLINK_PREP = 4 };
enum class Result { GRANTED, WAIT, DENIED_UNSAFE, ERROR_EXCEEDS_NEED, ERROR_INVALID };

struct State {
    int allocation[NUM_TASKS][NUM_RES];
    int max[NUM_TASKS][NUM_RES];
    int available[NUM_RES];
};

const char* task_name(int task);
const char* result_name(Result r);
State default_state();                                   // base state S1
bool init(const State& s, std::string* err = nullptr);   // validates, then resets shared state
void init_default();
bool is_safe(const State& s, int safe_sequence[NUM_TASKS]);  // true if safe, fills the sequence
Result request(int task, const int req[NUM_RES]);        // thread-safe (one internal mutex)
void release(int task, const int amount[NUM_RES]);       // thread-safe, clamped to current allocation
State snapshot();
}
```

Base state S1 (used by `default_state()` and by Exp 9):
- Allocation: T0 (1,0,1), T1 (1,1,0), T2 (1,2,1), T3 (0,1,1), T4 (1,0,0)
- Max: T0 (3,2,2), T1 (2,2,1), T2 (3,3,3), T3 (1,2,2), T4 (2,1,1)
- Available: (2,2,1)

### src/packet_queue.h (owner: Nidhi, internal, may change freely)

```cpp
class PacketQueue {
public:
    struct Stats { int max_depth; long blocked_full; long blocked_empty; };
    explicit PacketQueue(int capacity = 8, int reserved_for_critical = 2);
    ~PacketQueue();
    void push(const Packet& p);   // blocks when full (critical packets may use the reserved slots)
    Packet pop();                 // blocks when empty; returns the highest priority packet
    Stats stats() const;
};
```

## 4. Command-line contracts

Exit codes everywhere: `0` success, `1` invalid usage or invalid input, `2` audit or self-test failed, `77` stub not implemented yet (scaffold only).

| Binary | Owner | Flags |
|---|---|---|
| `build/satellite_sync` | Nidhi | `--demo=pc\|rw` (default pc), `--mode=safe\|unsafe` (default safe), `--csv PATH`, `--iterations N` (unsafe, default 200000), `--producer-delay-ms N`, `--consumer-delay-ms N` |
| `build/router_app` | Nidhi | `--csv PATH`, `--decisions PATH`, `--workers N` (default 2), `--out PATH`, `--trace PATH`, `--summary PATH` |
| `build/memory_management` | Rimjhim | no flags = interactive menu, `--test FILE`, `--selftest` |
| `build/page_replacement` | Rimjhim | `--ref-file PATH`, `--frames 1,3,4,5`, `--algo all\|fifo\|lru\|opt`, `--verbose`, `--selftest` |
| `build/bankers` | Surbhi | `--scenario S1\|S2\|S3\|all`, `--interactive`, `--selftest` |
| `build/csv_check` | Surbhi | `csv_check FILE [--reject-log PATH]` |
| `ml/gen_data.py` | Surbhi | `--seed 42 --packets 60 --train 600` |
| `ml/train_router.py` | Surbhi | `--train PATH --packets PATH --out outputs/decisions.csv --seed 42` |
| `ml/baseline.py` | Surbhi | importable `baseline_action(row)` and CLI writing `outputs/decisions_baseline.csv` |

`make` targets (owner Rimjhim): `all`, `test`, `tsan` (builds `build/satellite_sync_tsan` with `-fsanitize=thread`), `clean`.

## 5. Task mapping used by the router (Exp 9 link)

- TT&C and HOUSEKEEPING packets are processed as T0 TelemetryHandler.
- SSTV packets are processed as T2 SSTVProcessor. VOICE packets as T3 VoiceCodec.
- Routing decision step is T1 PacketRouter. The transmit step is T4 DownlinkPrep.
- Each step requests resources with `rm::request`, holds them briefly, then calls `rm::release`.
- Critical packets are retried until granted. They are never dropped because of a resource wait.

## 6. Report sections

All in `report/sections/`, one file per section, written by the owner listed in section 1:
`01_intro_scope`, `02_architecture_tools`, `03_data_ml`, `04_exp7`, `05_exp8`, `06_exp9`, `07_exp10`, `08_testing_performance`, `09_limitations_refs_ai`.

Each experiment section uses the lab format: Aim, Use-case connection, Program and steps, Commands, Screenshots, Output and results, Post-lab answers, Outcomes, Conclusion.

Screenshots go in `screenshots/` named `exp<N>_<what>_<NN>.png` (for example `exp7_unsafe_01.png`) and are taken by a human after a real run.
