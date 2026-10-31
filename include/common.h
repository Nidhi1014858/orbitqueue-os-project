// OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner.
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
