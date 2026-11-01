// OWNER: Nidhi - SomaiyaSat Downlink Router (router_app.cpp)
#include "common.h"
#include "mem_trace.h"
#include "packet_queue.h"
#include "resource_manager.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// ============================================================================
// Configuration and CLI Options
// ============================================================================

struct Config {
    std::string csv_path = "data/satellite_data.csv";
    std::string decisions_path = "outputs/decisions.csv";
    int workers = 2;
    std::string out_path = "outputs/routing_log.csv";
    std::string trace_path = "outputs/page_trace.txt";
    std::string summary_path = "outputs/router_summary.txt";
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "Options:\n"
              << "  --csv PATH         Path to input satellite packets CSV (default: data/satellite_data.csv)\n"
              << "  --decisions PATH   Path to ML/baseline decisions CSV (default: outputs/decisions.csv)\n"
              << "  --workers N        Number of concurrent worker threads (default: 2, must be > 0)\n"
              << "  --out PATH         Path to output routing log CSV (default: outputs/routing_log.csv)\n"
              << "  --trace PATH       Path to output memory access page trace (default: outputs/page_trace.txt)\n"
              << "  --summary PATH     Path to output execution summary report (default: outputs/router_summary.txt)\n"
              << "  --help, -h         Show this help message and exit\n";
}

bool parse_positive_int(const std::string& str, int& out) {
    if (str.empty()) return false;
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    try {
        out = std::stoi(str);
        return out > 0;
    } catch (...) {
        return false;
    }
}

bool parse_arguments(int argc, char* argv[], Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg.rfind("--csv=", 0) == 0) {
            cfg.csv_path = arg.substr(6);
        } else if (arg == "--csv" && i + 1 < argc) {
            cfg.csv_path = argv[++i];
        } else if (arg.rfind("--decisions=", 0) == 0) {
            cfg.decisions_path = arg.substr(12);
        } else if (arg == "--decisions" && i + 1 < argc) {
            cfg.decisions_path = argv[++i];
        } else if (arg.rfind("--workers=", 0) == 0) {
            if (!parse_positive_int(arg.substr(10), cfg.workers)) {
                std::cerr << "Error: invalid --workers value: " << arg.substr(10)
                          << " (must be a positive integer)\n";
                return false;
            }
        } else if (arg == "--workers" && i + 1 < argc) {
            if (!parse_positive_int(argv[++i], cfg.workers)) {
                std::cerr << "Error: invalid --workers value: " << argv[i]
                          << " (must be a positive integer)\n";
                return false;
            }
        } else if (arg.rfind("--out=", 0) == 0) {
            cfg.out_path = arg.substr(6);
        } else if (arg == "--out" && i + 1 < argc) {
            cfg.out_path = argv[++i];
        } else if (arg.rfind("--trace=", 0) == 0) {
            cfg.trace_path = arg.substr(8);
        } else if (arg == "--trace" && i + 1 < argc) {
            cfg.trace_path = argv[++i];
        } else if (arg.rfind("--summary=", 0) == 0) {
            cfg.summary_path = arg.substr(10);
        } else if (arg == "--summary" && i + 1 < argc) {
            cfg.summary_path = argv[++i];
        } else {
            std::cerr << "Error: unrecognized option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

// ============================================================================
// Shared State and Metrics Tracking
// ============================================================================

std::mutex g_log_mutex;
std::mutex g_output_mutex;

std::atomic<int> g_log_seq{0};
std::atomic<size_t> g_packets_popped{0};

std::atomic<int> g_count_transmitted{0};
std::atomic<int> g_count_deferred{0};
std::atomic<int> g_count_rejected{0};
std::atomic<int> g_count_safety_overrides{0};
std::atomic<int> g_count_resource_waits{0};
std::atomic<int> g_count_resource_denials{0};

std::chrono::steady_clock::time_point g_start_time;

void safe_log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << msg << "\n";
}

// Log record entry matching routing_log.csv columns
struct RoutingRecord {
    int packet_id;
    std::string type;
    std::string decision;
    std::string source;
    std::string final_action;
    std::string reason;
    std::string resource_result;
    long long timestamp_ms;
};

std::vector<RoutingRecord> g_routing_records;

// ============================================================================
// Producer Threads
// ============================================================================

void producer_thread(const std::string& name,
                     const std::vector<Packet>& packets,
                     PacketQueue& queue) {
    for (const auto& p : packets) {
        queue.push(p);
    }
    std::ostringstream oss;
    oss << "[PRODUCER:" << name << "] finished producing " << packets.size() << " packets";
    safe_log(oss.str());
}

// ============================================================================
// Resource Management Helper (Exp 9 Banker's Safety Link)
// ============================================================================

// Evaluates a resource request step for a task under Banker's algorithm.
// Critical packets retry on WAIT or DENIED_UNSAFE up to 1000 times.
// Non-critical packets retry WAIT with backoff, but DENIED_UNSAFE yields immediate deferral.
rm::Result perform_resource_step(int task, const int req[rm::NUM_RES], bool critical, int packet_id) {
    constexpr int MAX_CRITICAL_RETRIES = 1000;
    constexpr int MAX_NON_CRITICAL_WAITS = 10;

    int retries = 0;
    while (true) {
        rm::Result res = rm::request(task, req);

        if (res == rm::Result::GRANTED) {
            // Simulate brief holding time for resource usage
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            rm::release(task, req);
            return rm::Result::GRANTED;
        }

        if (res == rm::Result::WAIT) {
            g_count_resource_waits++;
            retries++;
            if (critical) {
                if (retries >= MAX_CRITICAL_RETRIES) {
                    safe_log("[ERROR] Critical packet " + std::to_string(packet_id) +
                             " reached resource wait retry cap (1000) on " + rm::task_name(task));
                    return rm::Result::WAIT;
                }
            } else {
                if (retries >= MAX_NON_CRITICAL_WAITS) {
                    // Non-critical packet exceeded wait patience -> treat as resource denial
                    g_count_resource_denials++;
                    return rm::Result::DENIED_UNSAFE;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if (res == rm::Result::DENIED_UNSAFE) {
            if (critical) {
                // Critical packets are never dropped due to resources; retry until system becomes safe
                g_count_resource_waits++;
                retries++;
                if (retries >= MAX_CRITICAL_RETRIES) {
                    safe_log("[ERROR] Critical packet " + std::to_string(packet_id) +
                             " reached retry cap (1000) for DENIED_UNSAFE on " + rm::task_name(task));
                    return rm::Result::DENIED_UNSAFE;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            } else {
                // Non-critical packet denied by Banker's safety check
                g_count_resource_denials++;
                return rm::Result::DENIED_UNSAFE;
            }
        }

        // Any other return code (e.g., error)
        return res;
    }
}

// ============================================================================
// Router Worker Thread
// ============================================================================

void router_worker_thread(int worker_id,
                          PacketQueue& queue,
                          const std::map<int, Action>& decisions,
                          const std::map<int, std::string>& sources) {
    while (true) {
        Packet p = queue.pop();
        if (p.id == -1) {
            // Sentinel packet received: shut down worker cleanly
            break;
        }

        // Mark packet as popped
        g_packets_popped++;

        // 1. ML/Baseline decision lookup or deterministic fallback
        Action decision = Action::TRANSMIT;
        std::string source = "ML";

        auto it_dec = decisions.find(p.id);
        if (it_dec != decisions.end()) {
            decision = it_dec->second;
            auto it_src = sources.find(p.id);
            if (it_src != sources.end() && !it_src->second.empty()) {
                source = it_src->second;
            }
        } else {
            // Deterministic fallback rule for missing decision
            source = "FALLBACK";
            if (is_critical(p.type)) {
                decision = Action::TRANSMIT;
            } else if (p.link_quality >= 50 && p.battery_level >= 50) {
                decision = Action::TRANSMIT;
            } else if (p.battery_level >= 20) {
                decision = Action::DEFER;
            } else {
                decision = Action::REJECT;
            }
        }

        // 2. Initial final action and safety override check
        Action final_action = decision;
        std::string reason = (source == "FALLBACK") ? "FALLBACK_RULE" : "MODEL_DECISION";

        if (is_critical(p.type) && (decision == Action::DEFER || decision == Action::REJECT)) {
            final_action = Action::TRANSMIT;
            reason = "SAFETY_OVERRIDE";
            g_count_safety_overrides++;
        }

        // 3. Banker's resource management checks (Exp 9 link)
        // Task mapping:
        // - TT&C and HOUSEKEEPING -> T0 TelemetryHandler
        // - SSTV -> T2 SSTVProcessor
        // - VOICE -> T3 VoiceCodec
        // - Routing step -> T1 PacketRouter
        // - Downlink transmit step -> T4 DownlinkPrep (if final action is TRANSMIT)
        const int cpu_request[rm::NUM_RES] = {1, 0, 0};
        const int buffer_request[rm::NUM_RES] = {0, 1, 0};
        const int transmitter_request[rm::NUM_RES] = {0, 0, 1};

        int task_proc = rm::T0_TELEMETRY_HANDLER;
        if (p.type == PacketType::SSTV) {
            task_proc = rm::T2_SSTV_PROCESSOR;
        } else if (p.type == PacketType::VOICE) {
            task_proc = rm::T3_VOICE_CODEC;
        }

        bool packet_is_critical = is_critical(p.type);
        rm::Result primary_res = rm::Result::GRANTED;

        // Stage A: Packet processing task with cpu_request
        rm::Result res_proc = perform_resource_step(task_proc, cpu_request, packet_is_critical, p.id);
        if (res_proc != rm::Result::GRANTED) {
            primary_res = res_proc;
            if (!packet_is_critical) {
                final_action = Action::DEFER;
                reason = "RESOURCE_DENIED";
            }
        }

        // Stage B: Routing decision task with buffer_request (only if not already denied for non-critical)
        if (packet_is_critical || (final_action != Action::DEFER || reason != "RESOURCE_DENIED")) {
            rm::Result res_router = perform_resource_step(rm::T1_PACKET_ROUTER, buffer_request, packet_is_critical, p.id);
            if (res_router != rm::Result::GRANTED) {
                primary_res = res_router;
                if (!packet_is_critical) {
                    final_action = Action::DEFER;
                    reason = "RESOURCE_DENIED";
                }
            }
        }

        // Stage C: Downlink transmit step with transmitter_request (only if final_action is TRANSMIT)
        if (final_action == Action::TRANSMIT) {
            rm::Result res_downlink = perform_resource_step(rm::T4_DOWNLINK_PREP, transmitter_request, packet_is_critical, p.id);
            if (res_downlink != rm::Result::GRANTED) {
                primary_res = res_downlink;
                if (!packet_is_critical) {
                    final_action = Action::DEFER;
                    reason = "RESOURCE_DENIED";
                }
            }
        }

        // 4. Memory access trace logging (Exp 8 & 10 link, docs/ACCESS_PATTERN.md)
        // Exact 9-access sequence per packet:
        // Step 1: STACK (worker activation frame)
        memtrace::access(memtrace::STACK, (worker_id % 2) * 50 + 10);

        // Step 2: CODE (queue pop & dispatch routine)
        memtrace::access(memtrace::CODE, 10);

        // Step 3: INPUT_BUFFER (virtual input slot based on packet ID)
        int vslot = ((p.id - 1) % 8 + 8) % 8;
        int offset_ib = vslot * 25 + (p.data_size % 25);
        memtrace::access(memtrace::INPUT_BUFFER, offset_ib);

        // Step 4: CODE (ML lookup routine entry)
        memtrace::access(memtrace::CODE, 60);

        // Step 5: MODEL_PARAMS (type policy weights)
        int type_idx = static_cast<int>(p.type);
        int offset_mp = (type_idx * 50) + (p.data_size % 50);
        memtrace::access(memtrace::MODEL_PARAMS, offset_mp);

        // Step 6: STACK (nested Banker's frame at call depth 2)
        memtrace::access(memtrace::STACK, (worker_id % 2) * 50 + 20);

        // Step 7: CODE (Banker's algorithm core)
        memtrace::access(memtrace::CODE, 120);

        // Step 8: OUTPUT_LOG (circular downlink dispatch slot)
        int seq = g_log_seq++;
        memtrace::access(memtrace::OUTPUT_LOG, (seq * 10) % 100);

        // Step 9: CODE (dispatch completion)
        memtrace::access(memtrace::CODE, 175);

        // 5. Update counts and output records under lock
        long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_start_time).count();

        RoutingRecord rec;
        rec.packet_id = p.id;
        rec.type = type_name(p.type);
        rec.decision = action_name(decision);
        rec.source = source;
        rec.final_action = action_name(final_action);
        rec.reason = reason;
        rec.resource_result = rm::result_name(primary_res);
        rec.timestamp_ms = now_ms;

        {
            std::lock_guard<std::mutex> lock(g_output_mutex);
            g_routing_records.push_back(rec);

            if (final_action == Action::TRANSMIT) {
                g_count_transmitted++;
            } else if (final_action == Action::DEFER) {
                g_count_deferred++;
            } else {
                g_count_rejected++;
            }

            // Ground station console logging: [GS] packet <id> <type> -> <ACTION>
            std::ostringstream oss;
            oss << "[GS] packet " << p.id << " " << type_name(p.type) << " -> "
                << action_name(final_action);
            if (reason != "MODEL_DECISION") {
                oss << " (reason: " << reason << ")";
            }
            safe_log(oss.str());
        }
    }
}

} // namespace

// ============================================================================
// Main Application Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    Config cfg;
    if (!parse_arguments(argc, argv, cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "===================================================================\n"
              << " SomaiyaSat Downlink Router (OS Experiments Integration Pipeline)\n"
              << "===================================================================\n"
              << "Packets CSV:    " << cfg.csv_path << "\n"
              << "Decisions CSV:  " << cfg.decisions_path << "\n"
              << "Worker Threads: " << cfg.workers << "\n"
              << "Routing Log:    " << cfg.out_path << "\n"
              << "Memory Trace:   " << cfg.trace_path << "\n"
              << "Summary Report: " << cfg.summary_path << "\n"
              << "-------------------------------------------------------------------\n";

    // 1. Load input packets
    std::vector<Packet> all_packets;
    int rejected_count = 0;
    std::string reject_log = "logs/rejected_rows.log";

    if (!load_packets(cfg.csv_path, reject_log, all_packets, rejected_count)) {
        std::cerr << "Error: failed to load packets from " << cfg.csv_path << "\n";
        return 1;
    }

    if (all_packets.empty()) {
        std::cerr << "Error: no valid packets found in " << cfg.csv_path << "\n";
        return 1;
    }

    // 2. Load model/baseline decisions
    std::map<int, Action> decisions;
    std::map<int, std::string> sources;
    if (!load_decisions(cfg.decisions_path, decisions, &sources)) {
        std::cerr << "Error: failed to load decisions from " << cfg.decisions_path << "\n";
        return 1;
    }

    std::cout << "Loaded " << all_packets.size() << " valid packets and "
              << decisions.size() << " decision mappings.\n";

    // 3. Initialize subsystems
    rm::init_default();
    if (!memtrace::open_trace(cfg.trace_path)) {
        std::cerr << "Error: failed to open memory trace at " << cfg.trace_path << "\n";
        return 1;
    }

    // 4. Partition packets for two producer streams
    std::vector<Packet> telemetry_packets;
    std::vector<Packet> payload_packets;

    for (const auto& p : all_packets) {
        if (p.type == PacketType::TTC || p.type == PacketType::HOUSEKEEPING) {
            telemetry_packets.push_back(p);
        } else {
            payload_packets.push_back(p);
        }
    }

    std::cout << "Telemetry producer stream: " << telemetry_packets.size() << " packets\n"
              << "Payload producer stream:   " << payload_packets.size() << " packets\n"
              << "Spawning " << cfg.workers << " router worker threads...\n"
              << "-------------------------------------------------------------------\n";

    g_start_time = std::chrono::steady_clock::now();

    // 5. Instantiate bounded priority queue and spawn threads
    PacketQueue queue(8, 2);

    std::thread t_telemetry(producer_thread, "Telemetry", std::cref(telemetry_packets), std::ref(queue));
    std::thread t_payload(producer_thread, "Payload", std::cref(payload_packets), std::ref(queue));

    std::vector<std::thread> workers;
    workers.reserve(cfg.workers);
    for (int w = 0; w < cfg.workers; ++w) {
        workers.emplace_back(router_worker_thread, w, std::ref(queue),
                             std::cref(decisions), std::cref(sources));
    }

    // 6. Wait for all packets to be produced
    t_telemetry.join();
    t_payload.join();

    // 7. Wait until all valid packets have been popped by workers
    const size_t total_packets = all_packets.size();
    while (g_packets_popped.load(std::memory_order_acquire) < total_packets) {
        std::this_thread::yield();
    }

    // 8. Push shutdown sentinel packets (-1) to terminate each worker thread cleanly
    for (int w = 0; w < cfg.workers; ++w) {
        Packet sentinel{};
        sentinel.id = -1;
        sentinel.type = PacketType::TTC; // Use critical type to bypass bulk restrictions
        queue.push(sentinel);
    }

    for (auto& w : workers) {
        w.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - g_start_time).count();
    if (elapsed_ms == 0) elapsed_ms = 1; // Prevent division by zero

    double throughput = (static_cast<double>(total_packets) * 1000.0) / static_cast<double>(elapsed_ms);

    // 9. Close memory access trace
    memtrace::close_trace();

    // 10. Write routing log CSV
    {
        std::ofstream fout(cfg.out_path);
        if (!fout.is_open()) {
            std::cerr << "Error: cannot write routing log to " << cfg.out_path << "\n";
            return 1;
        }

        fout << "packet_id,type,decision,source,final_action,reason,resource_result,timestamp_ms\n";
        for (const auto& rec : g_routing_records) {
            fout << rec.packet_id << ","
                 << rec.type << ","
                 << rec.decision << ","
                 << rec.source << ","
                 << rec.final_action << ","
                 << rec.reason << ","
                 << rec.resource_result << ","
                 << rec.timestamp_ms << "\n";
        }
    }

    PacketQueue::Stats queue_stats = queue.stats();

    // 11. Write summary report to file
    {
        std::ofstream fsum(cfg.summary_path);
        if (!fsum.is_open()) {
            std::cerr << "Error: cannot write summary to " << cfg.summary_path << "\n";
            return 1;
        }

        fsum << "=== SomaiyaSat Downlink Router Summary ===\n"
             << "Total Packets Processed: " << total_packets << "\n"
             << "Transmitted:             " << g_count_transmitted.load() << "\n"
             << "Deferred:                " << g_count_deferred.load() << "\n"
             << "Rejected:                " << g_count_rejected.load() << "\n"
             << "Safety Overrides:        " << g_count_safety_overrides.load() << "\n"
             << "Resource Waits:          " << g_count_resource_waits.load() << "\n"
             << "Resource Denials:        " << g_count_resource_denials.load() << "\n"
             << "Max Queue Depth:         " << queue_stats.max_depth << "\n"
             << "Runtime (ms):            " << elapsed_ms << "\n"
             << std::fixed << std::setprecision(2)
             << "Throughput (packets/s):  " << throughput << "\n";
    }

    // 12. Display summary to console
    std::cout << "-------------------------------------------------------------------\n"
              << "=== Downlink Router Run Complete ===\n"
              << "Total Packets:    " << total_packets << "\n"
              << "  Transmitted:    " << g_count_transmitted.load() << "\n"
              << "  Deferred:       " << g_count_deferred.load() << "\n"
              << "  Rejected:       " << g_count_rejected.load() << "\n"
              << "Safety Overrides: " << g_count_safety_overrides.load() << "\n"
              << "Resource Waits:   " << g_count_resource_waits.load() << "\n"
              << "Resource Denials: " << g_count_resource_denials.load() << "\n"
              << "Max Queue Depth:  " << queue_stats.max_depth << "\n"
              << "Runtime:          " << elapsed_ms << " ms\n"
              << std::fixed << std::setprecision(2)
              << "Throughput:       " << throughput << " packets/s\n"
              << "Routing Log:      " << cfg.out_path << "\n"
              << "Memory Trace:     " << cfg.trace_path << "\n"
              << "Summary File:     " << cfg.summary_path << "\n"
              << "===================================================================\n";

    return 0;
}
