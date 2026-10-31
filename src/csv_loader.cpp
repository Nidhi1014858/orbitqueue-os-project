// OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner.
// STUB: no validation yet, TODO(Surbhi)
#include "common.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool load_packets(const std::string& path, const std::string& reject_log_path,
                  std::vector<Packet>& out, int& rejected_count) {
    (void)reject_log_path; // STUB: no validation logging yet
    rejected_count = 0;
    out.clear();

    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open packets CSV at " << path << "\n";
        return false;
    }

    std::string line;
    bool first_line = true;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (first_line && line.rfind("packet_id", 0) == 0) {
            first_line = false;
            continue;
        }
        first_line = false;

        std::stringstream ss(line);
        std::string s_id, s_type, s_urg, s_lq, s_bat, s_size;
        if (std::getline(ss, s_id, ',') &&
            std::getline(ss, s_type, ',') &&
            std::getline(ss, s_urg, ',') &&
            std::getline(ss, s_lq, ',') &&
            std::getline(ss, s_bat, ',') &&
            std::getline(ss, s_size, ',')) {
            Packet p;
            p.id = std::stoi(s_id);
            if (!parse_type(s_type, p.type)) {
                p.type = PacketType::TTC;
            }
            p.urgency = std::stoi(s_urg);
            p.link_quality = std::stoi(s_lq);
            p.battery_level = std::stoi(s_bat);
            p.data_size = std::stoi(s_size);
            out.push_back(p);
        }
    }
    return true;
}

bool load_decisions(const std::string& path, std::map<int, Action>& out,
                    std::map<int, std::string>* sources) {
    out.clear();
    if (sources) sources->clear();

    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open decisions CSV at " << path << "\n";
        return false;
    }

    std::string line;
    bool first_line = true;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (first_line && line.rfind("packet_id", 0) == 0) {
            first_line = false;
            continue;
        }
        first_line = false;

        std::stringstream ss(line);
        std::string s_id, s_action, s_source;
        if (std::getline(ss, s_id, ',') && std::getline(ss, s_action, ',')) {
            int id = std::stoi(s_id);
            Action act = Action::TRANSMIT;
            parse_action(s_action, act);
            out[id] = act;
            if (sources && std::getline(ss, s_source, ',')) {
                (*sources)[id] = s_source;
            }
        }
    }
    return true;
}
