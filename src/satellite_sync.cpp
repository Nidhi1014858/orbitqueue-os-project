// OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#include "common.h"
#include "packet_queue.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <semaphore.h>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// Thread-safe console logging
std::mutex g_log_mutex;

void safe_log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << msg << "\n";
}

struct Config {
    std::string demo = "pc";
    std::string mode = "safe";
    std::string csv_path = "data/satellite_data.csv";
    int iterations = 200000;
    int producer_delay_ms = 0;
    int consumer_delay_ms = 0;
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "Options:\n"
              << "  --demo=pc|rw             Demo mode: 'pc' (producer-consumer, default), 'rw' (reader-writer)\n"
              << "  --mode=safe|unsafe       Synchronization mode: 'safe' (default), 'unsafe'\n"
              << "  --csv PATH               Path to input packets CSV (default: data/satellite_data.csv)\n"
              << "  --iterations N           Iterations for unsafe mode (default: 200000)\n"
              << "  --producer-delay-ms N    Delay after each produce in ms (default: 0)\n"
              << "  --consumer-delay-ms N    Delay after each consume in ms (default: 0)\n"
              << "  --help                   Show this help message\n";
}

bool parse_non_negative_int(const std::string& str, int& out) {
    if (str.empty()) return false;
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    try {
        out = std::stoi(str);
        return out >= 0;
    } catch (...) {
        return false;
    }
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
        } else if (arg.rfind("--demo=", 0) == 0) {
            cfg.demo = arg.substr(7);
        } else if (arg == "--demo" && i + 1 < argc) {
            cfg.demo = argv[++i];
        } else if (arg.rfind("--mode=", 0) == 0) {
            cfg.mode = arg.substr(7);
        } else if (arg == "--mode" && i + 1 < argc) {
            cfg.mode = argv[++i];
        } else if (arg.rfind("--csv=", 0) == 0) {
            cfg.csv_path = arg.substr(6);
        } else if (arg == "--csv" && i + 1 < argc) {
            cfg.csv_path = argv[++i];
        } else if (arg.rfind("--iterations=", 0) == 0) {
            if (!parse_positive_int(arg.substr(13), cfg.iterations)) {
                std::cerr << "Error: invalid --iterations value: " << arg.substr(13)
                          << " (must be a positive integer)\n";
                return false;
            }
        } else if (arg == "--iterations" && i + 1 < argc) {
            if (!parse_positive_int(argv[++i], cfg.iterations)) {
                std::cerr << "Error: invalid --iterations value: " << argv[i]
                          << " (must be a positive integer)\n";
                return false;
            }
        } else if (arg.rfind("--producer-delay-ms=", 0) == 0) {
            if (!parse_non_negative_int(arg.substr(20), cfg.producer_delay_ms)) {
                std::cerr << "Error: invalid --producer-delay-ms value: " << arg.substr(20) << "\n";
                return false;
            }
        } else if (arg == "--producer-delay-ms" && i + 1 < argc) {
            if (!parse_non_negative_int(argv[++i], cfg.producer_delay_ms)) {
                std::cerr << "Error: invalid --producer-delay-ms value: " << argv[i] << "\n";
                return false;
            }
        } else if (arg.rfind("--consumer-delay-ms=", 0) == 0) {
            if (!parse_non_negative_int(arg.substr(20), cfg.consumer_delay_ms)) {
                std::cerr << "Error: invalid --consumer-delay-ms value: " << arg.substr(20) << "\n";
                return false;
            }
        } else if (arg == "--consumer-delay-ms" && i + 1 < argc) {
            if (!parse_non_negative_int(argv[++i], cfg.consumer_delay_ms)) {
                std::cerr << "Error: invalid --consumer-delay-ms value: " << argv[i] << "\n";
                return false;
            }
        } else {
            std::cerr << "Error: unknown command-line option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

// ============================================================================
// Safe Producer-Consumer Implementation (Part A2)
// ============================================================================

// TelemetryProducer: handles critical streams (TT&C and HOUSEKEEPING)
void telemetry_producer(PacketQueue& queue,
                        const std::vector<Packet>& packets,
                        int delay_ms,
                        std::vector<int>& produced_ids,
                        std::mutex& produced_mutex) {
    for (const auto& p : packets) {
        long b_before = queue.stats().blocked_full;
        queue.push(p);
        PacketQueue::Stats st = queue.stats();

        if (st.blocked_full > b_before) {
            safe_log("[PRODUCER:Telemetry] BLOCKED: queue full");
        }

        {
            std::lock_guard<std::mutex> lock(produced_mutex);
            produced_ids.push_back(p.id);
        }

        std::ostringstream oss;
        oss << "[PRODUCER:Telemetry] packet " << p.id << " " << type_name(p.type)
            << " added (depth snapshot " << st.current_depth << ")";
        safe_log(oss.str());

        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
}

// PayloadProducer: handles bulk streams (SSTV and VOICE)
void payload_producer(PacketQueue& queue,
                      const std::vector<Packet>& packets,
                      int delay_ms,
                      std::vector<int>& produced_ids,
                      std::mutex& produced_mutex) {
    for (const auto& p : packets) {
        long b_before = queue.stats().blocked_full;
        queue.push(p);
        PacketQueue::Stats st = queue.stats();

        if (st.blocked_full > b_before) {
            safe_log("[PRODUCER:Payload] BLOCKED: queue full");
        }

        {
            std::lock_guard<std::mutex> lock(produced_mutex);
            produced_ids.push_back(p.id);
        }

        std::ostringstream oss;
        oss << "[PRODUCER:Payload] packet " << p.id << " " << type_name(p.type)
            << " added (depth snapshot " << st.current_depth << ")";
        safe_log(oss.str());

        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
}

// RouterConsumer: consumes packets with priority extraction
void router_consumer(PacketQueue& queue,
                     size_t total_packets,
                     int delay_ms,
                     std::vector<Packet>& consumed_packets) {
    for (size_t i = 0; i < total_packets; ++i) {
        long b_before = queue.stats().blocked_empty;
        Packet p = queue.pop();
        PacketQueue::Stats st = queue.stats();

        if (st.blocked_empty > b_before) {
            safe_log("[CONSUMER] BLOCKED: queue empty");
        }

        consumed_packets.push_back(p);

        std::ostringstream oss;
        oss << "[CONSUMER] packet " << p.id << " " << type_name(p.type)
            << " removed (depth snapshot " << st.current_depth << ")";
        safe_log(oss.str());

        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
}

int run_safe_demo(const Config& cfg) {
    // Load input packets from CSV
    std::vector<Packet> all_packets;
    int rejected_count = 0;
    std::string reject_log = "logs/rejected_rows.log";

    if (!load_packets(cfg.csv_path, reject_log, all_packets, rejected_count)) {
        std::cerr << "Error: failed to load packets from CSV: " << cfg.csv_path << "\n";
        return 1;
    }

    if (all_packets.empty()) {
        std::cerr << "Error: no valid packets found in CSV: " << cfg.csv_path << "\n";
        return 1;
    }

    // Partition packets into Telemetry and Payload streams
    std::vector<Packet> telemetry_packets;
    std::vector<Packet> payload_packets;
    std::set<int> expected_ids;
    std::set<int> expected_ttc_ids;

    for (const auto& p : all_packets) {
        expected_ids.insert(p.id);
        if (p.type == PacketType::TTC) {
            expected_ttc_ids.insert(p.id);
        }

        if (is_critical(p.type)) {
            telemetry_packets.push_back(p);
        } else {
            payload_packets.push_back(p);
        }
    }

    std::cout << "=== SomaiyaSat Downlink Synchronization (Exp 7 - Safe Mode) ===\n"
              << "Loaded " << all_packets.size() << " valid packets from " << cfg.csv_path << "\n"
              << "TelemetryProducer stream: " << telemetry_packets.size() << " packets (TT&C, HOUSEKEEPING)\n"
              << "PayloadProducer stream:   " << payload_packets.size() << " packets (SSTV, VOICE)\n"
              << "Queue capacity: 8 (2 reserved for critical)\n"
              << "--------------------------------------------------\n";

    // Shared bounded priority queue
    PacketQueue queue(8, 2);

    // Tracking structures for audit
    std::vector<int> produced_ids;
    std::mutex produced_mutex;
    std::vector<Packet> consumed_packets;

    // Launch producer and consumer threads
    std::thread t_telemetry(telemetry_producer,
                           std::ref(queue),
                           std::cref(telemetry_packets),
                           cfg.producer_delay_ms,
                           std::ref(produced_ids),
                           std::ref(produced_mutex));

    std::thread t_payload(payload_producer,
                         std::ref(queue),
                         std::cref(payload_packets),
                         cfg.producer_delay_ms,
                         std::ref(produced_ids),
                         std::ref(produced_mutex));

    std::thread t_consumer(router_consumer,
                          std::ref(queue),
                          all_packets.size(),
                          cfg.consumer_delay_ms,
                          std::ref(consumed_packets));

    // Wait for all threads to complete
    t_telemetry.join();
    t_payload.join();
    t_consumer.join();

    // End-of-Run Audit
    std::cout << "--------------------------------------------------\n"
              << "=== End-of-Run Synchronization Audit ===\n";

    bool audit_ok = true;
    PacketQueue::Stats st = queue.stats();

    // 1. Produced count equals consumed count
    std::cout << "Packets produced: " << produced_ids.size() << "\n";
    std::cout << "Packets consumed: " << consumed_packets.size() << "\n";
    if (produced_ids.size() != consumed_packets.size() ||
        produced_ids.size() != all_packets.size()) {
        std::cerr << "[AUDIT ERROR] Mismatch in produced/consumed counts!\n";
        audit_ok = false;
    }

    // 2. Every expected packet ID was produced exactly once
    std::set<int> produced_set(produced_ids.begin(), produced_ids.end());
    if (produced_set.size() != produced_ids.size()) {
        std::cerr << "[AUDIT ERROR] Duplicate packet IDs detected in production!\n";
        audit_ok = false;
    }
    if (produced_set != expected_ids) {
        std::cerr << "[AUDIT ERROR] Produced IDs set does not match expected input IDs!\n";
        audit_ok = false;
    }

    // 3. Every expected packet ID was consumed exactly once
    std::vector<int> consumed_ids;
    consumed_ids.reserve(consumed_packets.size());
    std::set<int> consumed_ttc_ids;

    for (const auto& p : consumed_packets) {
        consumed_ids.push_back(p.id);
        if (p.type == PacketType::TTC) {
            consumed_ttc_ids.insert(p.id);
        }
    }

    std::set<int> consumed_set(consumed_ids.begin(), consumed_ids.end());
    if (consumed_set.size() != consumed_ids.size()) {
        std::cerr << "[AUDIT ERROR] Duplicate packet IDs detected in consumption!\n";
        audit_ok = false;
    }
    if (consumed_set != expected_ids) {
        std::cerr << "[AUDIT ERROR] Consumed IDs set does not match expected input IDs!\n";
        audit_ok = false;
    }

    // 4. No critical TT&C packet lost
    if (consumed_ttc_ids != expected_ttc_ids) {
        std::cerr << "[AUDIT ERROR] Missing or corrupt TT&C critical packets!\n";
        audit_ok = false;
    } else {
        std::cout << "Critical TT&C packets verified: " << consumed_ttc_ids.size()
                  << " / " << expected_ttc_ids.size() << "\n";
    }

    // 5. Depth limits respected
    std::cout << "Queue max depth reached: " << st.max_depth << " / 8\n";
    std::cout << "Queue final depth:       " << st.current_depth << " / 8\n";
    std::cout << "Producer blocked on full:  " << st.blocked_full << "\n";
    std::cout << "Consumer blocked on empty: " << st.blocked_empty << "\n";

    if (st.max_depth > 8 || st.max_depth < 0) {
        std::cerr << "[AUDIT ERROR] Queue depth exceeded bounded capacity!\n";
        audit_ok = false;
    }

    if (st.current_depth != 0) {
        std::cerr << "[AUDIT ERROR] Queue is not empty at end of run (remaining: "
                  << st.current_depth << ")!\n";
        audit_ok = false;
    }

    if (audit_ok) {
        std::cout << "AUDIT PASS\n";
        return 0;
    } else {
        std::cout << "AUDIT FAIL\n";
        return 2;
    }
}

// ============================================================================
// Unsafe Producer-Consumer Implementation (Part A3 - DEMONSTRATION ONLY)
// Uses NO mutexes, NO semaphores, and NO atomics to expose race conditions.
// Uses modulo indexing to avoid out-of-bounds memory corruption.
// ============================================================================

struct UnsafeQueue {
    static constexpr size_t CAPACITY = 8;
    int buffer[CAPACITY] = {0};
    int in = 0;
    int out = 0;

    void push(int id) {
        buffer[in % CAPACITY] = id;
        in = (in + 1) % CAPACITY;
    }

    int pop() {
        int val = buffer[out % CAPACITY];
        out = (out + 1) % CAPACITY;
        return val;
    }
};

void unsafe_producer_1(UnsafeQueue& q, int start, int end) {
    for (int id = start; id <= end; ++id) {
        q.push(id);
        if ((id & 0x1FF) == 0) {
            std::this_thread::yield();
        }
    }
}

void unsafe_producer_2(UnsafeQueue& q, int start, int end) {
    for (int id = start; id <= end; ++id) {
        q.push(id);
        if ((id & 0x1FF) == 0) {
            std::this_thread::yield();
        }
    }
}

void unsafe_consumer(UnsafeQueue& q, int total_records, std::vector<int>& consumed) {
    for (int i = 0; i < total_records; ++i) {
        consumed[i] = q.pop();
        if ((i & 0x1FF) == 0) {
            std::this_thread::yield();
        }
    }
}

int run_unsafe_demo(int iterations) {
    std::cout << "=== DEMONSTRATION ONLY: Unsafe Concurrent Producer-Consumer ===\n"
              << "WARNING: Running with NO mutex and NO semaphores.\n"
              << "WARNING: Intentional data races are present; results may vary between runs.\n"
              << "Iterations: " << iterations << "\n"
              << "--------------------------------------------------------------\n";

    UnsafeQueue q;
    std::vector<int> consumed(iterations, 0);

    int half = iterations / 2;
    std::thread t1(unsafe_producer_1, std::ref(q), 1, half);
    std::thread t2(unsafe_producer_2, std::ref(q), half + 1, iterations);
    std::thread tc(unsafe_consumer, std::ref(q), iterations, std::ref(consumed));

    t1.join();
    t2.join();
    tc.join();

    // Analyze unsafe run results
    int invalid_ids = 0;
    std::vector<int> freq(iterations + 1, 0);

    for (int val : consumed) {
        if (val < 1 || val > iterations) {
            invalid_ids++;
        } else {
            freq[val]++;
        }
    }

    int unique_consumed_ids = 0;
    int duplicate_ids = 0;
    int duplicate_instances = 0;
    int missing_ids = 0;

    for (int id = 1; id <= iterations; ++id) {
        if (freq[id] == 0) {
            missing_ids++;
        } else {
            unique_consumed_ids++;
            if (freq[id] > 1) {
                duplicate_ids++;
                duplicate_instances += (freq[id] - 1);
            }
        }
    }

    bool count_mismatch = (consumed.size() != static_cast<size_t>(iterations)) ||
                          (unique_consumed_ids != iterations);

    std::cout << "--- Unsafe Execution Statistics ---\n"
              << "Produced attempts:        " << iterations << "\n"
              << "Consumed records:         " << consumed.size() << "\n"
              << "Unique consumed IDs:      " << unique_consumed_ids << "\n"
              << "Duplicate IDs:            " << duplicate_ids << " (" << duplicate_instances << " duplicate instances)\n"
              << "Missing IDs:              " << missing_ids << "\n"
              << "Invalid/out-of-range IDs: " << invalid_ids << "\n"
              << "Count mismatch:           " << (count_mismatch ? "YES" : "NO") << "\n"
              << "--------------------------------------------------------------\n";

    bool anomaly = (duplicate_ids > 0) || (missing_ids > 0) || (invalid_ids > 0) || count_mismatch;

    if (anomaly) {
        std::cout << "UNSAFE ANOMALY DETECTED\n"
                  << "Explanation: Unsynchronized concurrent access caused race conditions leading to corrupted or dropped packet IDs.\n";
    } else {
        std::cout << "No visible anomaly observed in this run.\n"
                  << "Explanation: The OS thread scheduling timing in this specific run did not produce an observable collision.\n"
                  << "Recommendation: Increase --iterations (e.g., --iterations 500000 or 1000000) or verify data races with ThreadSanitizer ('make tsan').\n";
    }

    return 0;
}

// ============================================================================
// Reader-Writer Implementation (Part A5: Reader Preference)
// Shared Telemetry State: battery_level, link_quality
// Synchronization:
//   - count_mutex (pthread_mutex_t): protects read_count
//   - write_lock (sem_t, init 1): held by writer, or by first reader on behalf of readers
// ============================================================================

struct TelemetryState {
    int battery_level = 100;
    int link_quality = 90;
    int update_count = 0;
};

struct RWContext {
    pthread_mutex_t count_mutex;
    sem_t write_lock;
    int read_count = 0;
    TelemetryState telemetry;
};

void rw_reader_thread(const std::string& name, RWContext& ctx, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        // Reader entry section
        pthread_mutex_lock(&ctx.count_mutex);
        ctx.read_count++;
        if (ctx.read_count == 1) {
            // First reader acquires write_lock to block writers
            while (sem_wait(&ctx.write_lock) != 0) {
                if (errno == EINTR) continue;
                std::perror("rw_reader sem_wait write_lock");
                std::exit(EXIT_FAILURE);
            }
        }
        int active_readers = ctx.read_count;
        pthread_mutex_unlock(&ctx.count_mutex);

        // Critical section (Reading) - multiple readers execute concurrently
        int bat = ctx.telemetry.battery_level;
        int lq = ctx.telemetry.link_quality;
        int ver = ctx.telemetry.update_count;

        std::ostringstream oss;
        oss << "[READER:" << name << "] reading telemetry (active readers: " << active_readers
            << ") -> Battery: " << bat << "%, Link: " << lq << "% (update #" << ver << ")";
        safe_log(oss.str());

        // Simulated read duration to demonstrate reader overlap
        std::this_thread::sleep_for(std::chrono::milliseconds(15));

        // Reader exit section
        pthread_mutex_lock(&ctx.count_mutex);
        ctx.read_count--;
        if (ctx.read_count == 0) {
            // Last reader releases write_lock allowing waiting writers to proceed
            sem_post(&ctx.write_lock);
        }
        pthread_mutex_unlock(&ctx.count_mutex);

        // Delay between successive reads
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void rw_writer_thread(RWContext& ctx, int iterations) {
    // Simulated sensor readings
    const int battery_samples[] = {95, 90, 85, 80, 75};
    const int link_samples[]    = {88, 72, 65, 80, 92};

    for (int i = 0; i < iterations; ++i) {
        // Writer entry section - exclusive lock
        while (sem_wait(&ctx.write_lock) != 0) {
            if (errno == EINTR) continue;
            std::perror("rw_writer sem_wait write_lock");
            std::exit(EXIT_FAILURE);
        }

        // Critical section (Writing) - writer is alone
        std::ostringstream oss_start;
        oss_start << "[WRITER:TelemetrySensor] entered critical section (writer alone, active readers: 0)";
        safe_log(oss_start.str());

        ctx.telemetry.battery_level = battery_samples[i % 5];
        ctx.telemetry.link_quality = link_samples[i % 5];
        ctx.telemetry.update_count++;

        std::ostringstream oss_done;
        oss_done << "[WRITER:TelemetrySensor] updated telemetry to Battery: "
                 << ctx.telemetry.battery_level << "%, Link: "
                 << ctx.telemetry.link_quality << "% (update #" << ctx.telemetry.update_count << ")";
        safe_log(oss_done.str());

        // Simulated write duration
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Writer exit section
        sem_post(&ctx.write_lock);

        // Delay between successive sensor updates
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}

int run_reader_writer_demo() {
    std::cout << "=== SomaiyaSat Telemetry Synchronization (Exp 7 - Reader-Writer Demo) ===\n"
              << "Shared state: battery_level, link_quality\n"
              << "Synchronization: Reader-preference (count_mutex + write_lock semaphore)\n"
              << "Threads: 1 Writer (TelemetrySensor), 2 Readers (RouterReader, DashboardReader)\n"
              << "--------------------------------------------------------------------------\n";

    RWContext ctx;
    pthread_mutex_init(&ctx.count_mutex, nullptr);
    sem_init(&ctx.write_lock, 0, 1);

    constexpr int WRITER_UPDATES = 5;
    constexpr int READER_ITERATIONS = 6;

    std::thread t_writer(rw_writer_thread, std::ref(ctx), WRITER_UPDATES);
    std::thread t_reader1(rw_reader_thread, "RouterReader", std::ref(ctx), READER_ITERATIONS);
    std::thread t_reader2(rw_reader_thread, "DashboardReader", std::ref(ctx), READER_ITERATIONS);

    t_writer.join();
    t_reader1.join();
    t_reader2.join();

    pthread_mutex_destroy(&ctx.count_mutex);
    sem_destroy(&ctx.write_lock);

    std::cout << "--------------------------------------------------------------------------\n"
              << "=== Reader-Writer Demonstration Complete ===\n"
              << "Final telemetry state: Battery " << ctx.telemetry.battery_level
              << "%, Link " << ctx.telemetry.link_quality
              << "% (Total updates: " << ctx.telemetry.update_count << ")\n"
              << "READER-WRITER DEMO PASS\n";

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    Config cfg;
    if (!parse_arguments(argc, argv, cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    // Check for demo and mode combinations
    if (cfg.demo == "rw") {
        if (cfg.mode == "unsafe") {
            std::cout << "[STUB] Unsafe Reader-Writer demo (--demo=rw --mode=unsafe) is not implemented yet (owner: Nidhi)\n";
            return 77;
        }
        if (cfg.mode == "safe") {
            return run_reader_writer_demo();
        }
        std::cerr << "Error: invalid --mode '" << cfg.mode << "'. Expected 'safe' or 'unsafe'.\n";
        return 1;
    }

    if (cfg.demo != "pc") {
        std::cerr << "Error: invalid --demo mode '" << cfg.demo << "'. Expected 'pc' or 'rw'.\n";
        return 1;
    }

    if (cfg.mode == "unsafe") {
        return run_unsafe_demo(cfg.iterations);
    }
    if (cfg.mode != "safe") {
        std::cerr << "Error: invalid --mode '" << cfg.mode << "'. Expected 'safe' or 'unsafe'.\n";
        return 1;
    }

    return run_safe_demo(cfg);
}
