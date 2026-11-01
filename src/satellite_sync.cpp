// OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#include "common.h"
#include "packet_queue.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
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
    int producer_delay_ms = 0;
    int consumer_delay_ms = 0;
};

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n"
              << "Options:\n"
              << "  --demo=pc|rw             Demo mode: 'pc' (producer-consumer, default), 'rw' (reader-writer)\n"
              << "  --mode=safe|unsafe       Synchronization mode: 'safe' (default), 'unsafe'\n"
              << "  --csv PATH               Path to input packets CSV (default: data/satellite_data.csv)\n"
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

} // namespace

int main(int argc, char* argv[]) {
    Config cfg;
    if (!parse_arguments(argc, argv, cfg)) {
        print_usage(argv[0]);
        return 1;
    }

    // Check for modes not yet implemented in Part A2
    if (cfg.demo == "rw") {
        std::cout << "[STUB] Reader-Writer demo (--demo=rw) is not implemented yet (owner: Nidhi)\n";
        return 77;
    }
    if (cfg.demo != "pc") {
        std::cerr << "Error: invalid --demo mode '" << cfg.demo << "'. Expected 'pc' or 'rw'.\n";
        return 1;
    }

    if (cfg.mode == "unsafe") {
        std::cout << "[STUB] Unsafe comparison mode (--mode=unsafe) is not implemented yet (owner: Nidhi)\n";
        return 77;
    }
    if (cfg.mode != "safe") {
        std::cerr << "Error: invalid --mode '" << cfg.mode << "'. Expected 'safe' or 'unsafe'.\n";
        return 1;
    }

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

    std::cout << "=== SomaiyaSat Downlink Synchronization (Exp 7) ===\n"
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
