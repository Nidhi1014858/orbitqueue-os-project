# OrbitQueue: Memory Access Pattern Specification
<!-- OWNER: Nidhi -->

## 1. Overview and Purpose

This document specifies the deterministic logical memory access patterns executed by the SomaiyaSat Downlink Router (`router_app.cpp`). As the router processes telemetry and payload packets, each pipeline stage logs its logical memory access through the trace module (`mem_trace.h`).

> **Educational Disclaimer (Synthetic Teaching Model):**
> This access pattern generates a synthetic logical trace designed specifically for operating systems education. It connects the multi-threaded synchronization of **Experiment 7** to the memory translation of **Experiment 8** (segmentation and paging) and the page replacement algorithms of **Experiment 10** (FIFO, LRU, Optimal). It represents simulated software-level memory accesses, not physical hardware MMU or kernel-level page-fault traps.

---

## 2. Memory Regions and Logical Page Architecture

The memory layout consists of **8 logical pages** of **100 bytes each** (`PAGE_SIZE = 100`, total virtual address space = 800 bytes), divided into five logical regions defined in `include/mem_trace.h`:

| Region | Enum ID | Page Range | First Page | Number of Pages | Valid Offsets | Max Capacity | Description |
|---|:---:|:---:|:---:|:---:|:---:|:---:|---|
| `CODE` | 0 | Pages 0 – 1 | 0 | 2 | 0 – 199 | 200 B | Router execution instructions, queue handlers, classification logic, Banker's check |
| `MODEL_PARAMS` | 1 | Pages 2 – 3 | 2 | 2 | 0 – 199 | 200 B | Decision tables and routing heuristic parameters indexed by packet type |
| `INPUT_BUFFER` | 2 | Pages 4 – 5 | 4 | 2 | 0 – 199 | 200 B | Staging buffer for virtual input slots derived from packet IDs (8 slots &times; 25 B) |
| `OUTPUT_LOG` | 3 | Page 6 | 6 | 1 | 0 – 99 | 100 B | Circular downlink dispatch output buffer (10 records &times; 10 B) |
| `STACK` | 4 | Page 7 | 7 | 1 | 0 – 99 | 100 B | Worker thread activation records and local execution frames (2 workers &times; 50 B) |

### Page Mapping Formula

Per `include/mem_trace.h`, the page number for any given `(region, offset)` pair is calculated deterministically as:

$$\text{page} = \text{region\_first\_page}(r) + \frac{\text{offset} \pmod{\text{region\_num\_pages}(r) \times \text{PAGE\_SIZE}}}{\text{PAGE\_SIZE}}$$

- Negative offsets default to `region_first_page(r)`.
- Offsets within `[0, 99]` map to the region's first page; offsets within `[100, 199]` map to the region's second page (for 2-page regions).

---

## 3. Page-Trace Lifecycle

The lifecycle of the memory trace across router execution is strictly defined:

1. **Initialization (`memtrace::open_trace`):**
   Before router worker threads are spawned, the main thread opens the trace file configured via `--trace PATH` (default: `outputs/page_trace.txt`):
   ```cpp
   memtrace::open_trace(trace_path);
   ```
   This opens the destination file and writes the initial metadata header (`# OrbitQueue Memory Access Trace`).
2. **Access Recording (`memtrace::access`):**
   During packet processing, worker threads invoke:
   ```cpp
   memtrace::access(Region r, int offset);
   ```
   at each documented stage. The function computes `page_of(r, offset)` and appends the resulting page number (one per line) to the active trace file. Access logging is thread-safe, protected internally by a mutex.
3. **Termination (`memtrace::close_trace`):**
   After all packets have been processed and worker threads have joined, the main thread calls:
   ```cpp
   memtrace::close_trace();
   ```
   This flushes remaining buffers and closes the trace file, leaving it ready for Experiment 10 analysis.

---

## 4. Pipeline Stages and Deterministic Offset Formulas

Each packet handled by a worker thread generates an ordered sequence of memory accesses across the five regions.

### Stage 1: Worker Activation & Frame Initialization
When worker thread `worker_id` ($0$ or $1$) begins processing a packet, it allocates its stack frame at call depth $1$, then executes the dispatch entry point.

- **Stack Access:**
  - **Region:** `STACK`
  - **Formula:** $\text{offset} = (\text{worker\_id} \pmod 2) \times 50 + (\text{call\_depth} \times 10) \pmod{50}$
  - **Evaluation:** For $\text{worker\_id} = 0$, $\text{offset} = 10$; for $\text{worker\_id} = 1$, $\text{offset} = 60$.
  - **Page:** **Page 7**
- **Code Execution:**
  - **Region:** `CODE`
  - **Formula:** $\text{offset} = 10$
  - **Page:** **Page 0**

### Stage 2: Packet Arrival & Queue Handling
The worker retrieves the highest-priority packet from `PacketQueue`.

- **Code Execution:**
  - **Region:** `CODE` (Queue pop & lock release)
  - **Formula:** $\text{offset} = 25$
  - **Page:** **Page 0**

### Stage 3: Input Buffer Staging
The worker accesses the packet's raw payload in the input staging area. Rather than tracking physical, volatile queue indices, a deterministic virtual input-buffer slot is derived directly from the positive packet ID:

$$\text{virtual\_slot} = (\text{packet\_id} - 1) \pmod 8$$

> **Design Rationale:** This virtual slot represents a logical staging location in the teaching memory model rather than a physical queue position. Because the priority queue reorders packets upon dequeue (TT&C and HOUSEKEEPING preempt SSTV and VOICE), using `(packet_id - 1) % 8` guarantees a completely deterministic, reproducible offset for each packet regardless of runtime scheduling or concurrency order.

- **Buffer Read:**
  - **Region:** `INPUT_BUFFER`
  - **Formula:** $\text{offset} = \text{virtual\_slot} \times 25 + (\text{data\_size} \pmod{25})$
  - **Bounds:** Since $\text{virtual\_slot} \in [0, 7]$ and $(\text{data\_size} \pmod{25}) \in [0, 24]$, minimum $\text{offset} = 0$, maximum $\text{offset} = 7 \times 25 + 24 = 199$.
  - **Page Mapping:**
    - Virtual slots $0 – 3$ ($\text{offset} \in [0, 99]$) $\rightarrow$ **Page 4**
    - Virtual slots $4 – 7$ ($\text{offset} \in [100, 199]$) $\rightarrow$ **Page 5**

### Stage 4: ML Decision Lookup
The worker looks up the classification policy weights corresponding to the packet's type:
$\text{type} \in \{ \text{TT\&C}: 0, \text{HOUSEKEEPING}: 1, \text{SSTV}: 2, \text{VOICE}: 3 \}$.

- **Code Execution:**
  - **Region:** `CODE` (Inference routine entry)
  - **Formula:** $\text{offset} = 60$
  - **Page:** **Page 0**
- **Model Parameter Read:**
  - **Region:** `MODEL_PARAMS`
  - **Formula:** $\text{offset} = (\text{type} \times 50) + (\text{data\_size} \pmod{50})$
  - **Bounds:** Minimum $\text{offset} = 0$, maximum $\text{offset} = 3 \times 50 + 49 = 199$.
  - **Page Mapping:**
    - Types $0$ and $1$ (TT&C, HOUSEKEEPING: $\text{offset} \in [0, 99]$) $\rightarrow$ **Page 2**
    - Types $2$ and $3$ (SSTV, VOICE: $\text{offset} \in [100, 199]$) $\rightarrow$ **Page 3**

### Stage 5: Safety Override & Banker's Resource Check
The router checks the hard safety invariant (TT&C / HOUSEKEEPING cannot be deferred or rejected) and evaluates resource availability via the Banker's safety algorithm (`rm::request`). This nested call runs at call depth $2$.

- **Stack Allocation (Nested Call Frame):**
  - **Region:** `STACK`
  - **Formula:** $\text{offset} = (\text{worker\_id} \pmod 2) \times 50 + (2 \times 10) = (\text{worker\_id} \pmod 2) \times 50 + 20$
  - **Evaluation:** Worker 0 accesses offset $20$; Worker 1 accesses offset $70$.
  - **Page:** **Page 7**
- **Code Execution (Banker's Algorithm Core):**
  - **Region:** `CODE`
  - **Formula:** $\text{offset} = 120$ (located in second code page)
  - **Page:** **Page 1**

### Stage 6: Routing Log Output Dispatch
The final decision (`TRANSMIT`, `DEFER`, or `REJECT`) is committed to the downlink dispatch log based on the monotonic log record sequence number $\text{log\_seq} \in \{0, 1, 2, \dots\}$.

- **Output Log Write:**
  - **Region:** `OUTPUT_LOG`
  - **Formula:** $\text{offset} = (\text{log\_seq} \times 10) \pmod{100}$
  - **Bounds:** 10 rolling record slots spanning bytes $0 – 99$.
  - **Page:** **Page 6**
- **Code Execution (Dispatch Completion):**
  - **Region:** `CODE`
  - **Formula:** $\text{offset} = 175$
  - **Page:** **Page 1**

---

## 5. Ordered Access Summary Table

Every packet processed generates the following sequential memory accesses:

| Step | Processing Phase | Target Region | Offset Calculation Formula | Value Range | Resulting Page |
|:---:|---|:---:|---|:---:|:---:|
| **1** | Worker Frame Allocation | `STACK` | `(worker_id % 2) * 50 + 10` | 10, 60 | **Page 7** |
| **2** | Queue Pop & Dispatch | `CODE` | `10` | 10 | **Page 0** |
| **3** | Input Staging Read | `INPUT_BUFFER` | `virtual_slot * 25 + (data_size % 25)` with `virtual_slot = (packet_id - 1) % 8` | 0 – 199 | **Page 4 or 5** |
| **4** | ML Lookup Routine | `CODE` | `60` | 60 | **Page 0** |
| **5** | Model Parameters Read | `MODEL_PARAMS` | `(type * 50) + (data_size % 50)` | 0 – 199 | **Page 2 or 3** |
| **6** | Banker Nested Frame | `STACK` | `(worker_id % 2) * 50 + 20` | 20, 70 | **Page 7** |
| **7** | Banker Algorithm Exec | `CODE` | `120` | 120 | **Page 1** |
| **8** | Output Dispatch Log | `OUTPUT_LOG` | `(log_seq * 10) % 100` | 0 – 99 | **Page 6** |
| **9** | Completion & Epilogue | `CODE` | `175` | 175 | **Page 1** |

---

## 6. Worked Example: Processing Packet #17

Consider a representative packet flowing through the router with the following parameters:

- **Packet ID:** 17
- **Packet Type:** `SSTV` ($\text{type} = 2$)
- **Data Size:** 64 bytes
- **Handling Worker:** Worker 0 ($\text{worker\_id} = 0$)
- **Log Sequence Number:** 3 ($\text{log\_seq} = 3$)

### Step-by-Step Access Evaluation

1. **Step 1 (`STACK`):**
   $$\text{offset} = (0 \times 50) + 10 = 10 \implies \text{page} = 7 + \lfloor 10 / 100 \rfloor = \mathbf{7}$$
2. **Step 2 (`CODE`):**
   $$\text{offset} = 10 \implies \text{page} = 0 + \lfloor 10 / 100 \rfloor = \mathbf{0}$$
3. **Step 3 (`INPUT_BUFFER`):**
   $$\text{virtual\_slot} = (17 - 1) \pmod 8 = 16 \pmod 8 = 0$$
   $$\text{offset} = (0 \times 25) + (64 \pmod{25}) = 0 + 14 = 14 \implies \text{page} = 4 + \lfloor 14 / 100 \rfloor = \mathbf{4}$$
4. **Step 4 (`CODE`):**
   $$\text{offset} = 60 \implies \text{page} = 0 + \lfloor 60 / 100 \rfloor = \mathbf{0}$$
5. **Step 5 (`MODEL_PARAMS`):**
   $$\text{offset} = (2 \times 50) + (64 \pmod{50}) = 100 + 14 = 114 \implies \text{page} = 2 + \lfloor 114 / 100 \rfloor = \mathbf{3}$$
6. **Step 6 (`STACK`):**
   $$\text{offset} = (0 \times 50) + 20 = 20 \implies \text{page} = 7 + \lfloor 20 / 100 \rfloor = \mathbf{7}$$
7. **Step 7 (`CODE`):**
   $$\text{offset} = 120 \implies \text{page} = 0 + \lfloor 120 / 100 \rfloor = \mathbf{1}$$
8. **Step 8 (`OUTPUT_LOG`):**
   $$\text{offset} = (3 \times 10) \pmod{100} = 30 \implies \text{page} = 6 + \lfloor 30 / 100 \rfloor = \mathbf{6}$$
9. **Step 9 (`CODE`):**
   $$\text{offset} = 175 \implies \text{page} = 0 + \lfloor 175 / 100 \rfloor = \mathbf{1}$$

### Generated Page Trace String

The sequence of 9 page accesses generated for Packet #17 is:

$$\mathbf{7 \rightarrow 0 \rightarrow 4 \rightarrow 0 \rightarrow 3 \rightarrow 7 \rightarrow 1 \rightarrow 6 \rightarrow 1}$$

This realistic reference string demonstrates immediate spatial and temporal locality across stack, code loops, and data buffers, providing ideal input for FIFO, LRU, and Optimal replacement evaluations in Experiment 10.
