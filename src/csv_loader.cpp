#include "common.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {

const char* PACKET_HEADER =
    "packet_id,type,urgency,link_quality,battery_level,data_size";
const char* DECISION_HEADER = "packet_id,action,source";

std::string trim(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) return "";
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) fields.push_back(trim(field));
    if (!line.empty() && line.back() == ',') fields.push_back("");
    return fields;
}

bool parse_integer(const std::string& text, int& value) {
    if (text.empty()) return false;
    try {
        std::size_t used = 0;
        const long parsed = std::stol(text, &used);
        if (used != text.size() || parsed < -2147483647L - 1L ||
            parsed > 2147483647L) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void reject(std::ofstream& log, int line_number, const std::string& reason,
            int& rejected_count) {
    ++rejected_count;
    log << "line " << line_number << ": " << reason << '\n';
}

bool in_range(int value, int low, int high) {
    return value >= low && value <= high;
}

}  // namespace

bool load_packets(const std::string& path, const std::string& reject_log_path,
                  std::vector<Packet>& out, int& rejected_count) {
    out.clear();
    rejected_count = 0;

    std::ifstream input(path);
    if (!input.is_open()) {
        std::cerr << "Error: cannot open packets CSV: " << path << '\n';
        return false;
    }

    std::ofstream reject_log(reject_log_path, std::ios::trunc);
    if (!reject_log.is_open()) {
        std::cerr << "Error: cannot open rejection log: " << reject_log_path << '\n';
        return false;
    }

    std::string line;
    int line_number = 0;
    bool header_seen = false;
    std::set<int> packet_ids;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] == '#') continue;

        if (!header_seen) {
            header_seen = true;
            if (cleaned != PACKET_HEADER) {
                std::cerr << "Error: invalid packets CSV header in " << path
                          << "; expected: " << PACKET_HEADER << '\n';
                return false;
            }
            continue;
        }

        const std::vector<std::string> fields = split_csv(cleaned);
        if (fields.size() != 6) {
            reject(reject_log, line_number, "expected 6 fields", rejected_count);
            continue;
        }

        bool has_empty = false;
        for (const std::string& field : fields) {
            if (field.empty()) has_empty = true;
        }
        if (has_empty) {
            reject(reject_log, line_number, "empty field", rejected_count);
            continue;
        }

        Packet packet{};
        if (!parse_integer(fields[0], packet.id)) {
            reject(reject_log, line_number, "packet_id is not an integer",
                   rejected_count);
            continue;
        }
        if (packet.id <= 0) {
            reject(reject_log, line_number, "packet_id must be positive",
                   rejected_count);
            continue;
        }
        if (packet_ids.count(packet.id) != 0) {
            reject(reject_log, line_number, "duplicate packet_id", rejected_count);
            continue;
        }
        if (!parse_type(fields[1], packet.type)) {
            reject(reject_log, line_number, "unknown packet type: " + fields[1],
                   rejected_count);
            continue;
        }
        if (!parse_integer(fields[2], packet.urgency)) {
            reject(reject_log, line_number, "urgency is not an integer",
                   rejected_count);
            continue;
        }
        if (!parse_integer(fields[3], packet.link_quality)) {
            reject(reject_log, line_number, "link_quality is not an integer",
                   rejected_count);
            continue;
        }
        if (!parse_integer(fields[4], packet.battery_level)) {
            reject(reject_log, line_number, "battery_level is not an integer",
                   rejected_count);
            continue;
        }
        if (!parse_integer(fields[5], packet.data_size)) {
            reject(reject_log, line_number, "data_size is not an integer",
                   rejected_count);
            continue;
        }
        if (!in_range(packet.urgency, 1, 5)) {
            reject(reject_log, line_number, "urgency must be in 1..5",
                   rejected_count);
            continue;
        }
        if (!in_range(packet.link_quality, 0, 100)) {
            reject(reject_log, line_number, "link_quality must be in 0..100",
                   rejected_count);
            continue;
        }
        if (!in_range(packet.battery_level, 0, 100)) {
            reject(reject_log, line_number, "battery_level must be in 0..100",
                   rejected_count);
            continue;
        }
        if (!in_range(packet.data_size, 1, 1000000)) {
            reject(reject_log, line_number, "data_size must be in 1..1000000",
                   rejected_count);
            continue;
        }

        packet_ids.insert(packet.id);
        out.push_back(packet);
    }

    if (!header_seen) {
        std::cerr << "Error: packets CSV has no header: " << path << '\n';
        return false;
    }
    return true;
}

bool load_decisions(const std::string& path, std::map<int, Action>& out,
                    std::map<int, std::string>* sources) {
    out.clear();
    if (sources != nullptr) sources->clear();

    std::ifstream input(path);
    if (!input.is_open()) {
        std::cerr << "Error: cannot open decisions CSV: " << path << '\n';
        return false;
    }

    std::string line;
    int line_number = 0;
    bool header_seen = false;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] == '#') continue;

        if (!header_seen) {
            header_seen = true;
            if (cleaned != DECISION_HEADER) {
                std::cerr << "Error: invalid decisions CSV header in " << path
                          << "; expected: " << DECISION_HEADER << '\n';
                return false;
            }
            continue;
        }

        const std::vector<std::string> fields = split_csv(cleaned);
        int packet_id = 0;
        Action action = Action::TRANSMIT;
        if (fields.size() != 3 || !parse_integer(fields[0], packet_id) ||
            packet_id <= 0 || !parse_action(fields[1], action) || fields[2].empty()) {
            std::cerr << "Error: invalid decision row at line " << line_number
                      << " in " << path << '\n';
            return false;
        }
        if (out.count(packet_id) != 0) {
            std::cerr << "Error: duplicate packet_id at line " << line_number
                      << " in " << path << '\n';
            return false;
        }
        out[packet_id] = action;
        if (sources != nullptr) (*sources)[packet_id] = fields[2];
    }

    if (!header_seen) {
        std::cerr << "Error: decisions CSV has no header: " << path << '\n';
        return false;
    }
    return true;
}
