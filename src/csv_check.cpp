#include "common.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

void usage(const char* program) {
    std::cerr << "Usage: " << program << " FILE [--reject-log PATH]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        usage(argv[0]);
        return 1;
    }

    std::string reject_log = "logs/rejected_rows.log";
    if (argc == 4) {
        if (std::string(argv[2]) != "--reject-log") {
            usage(argv[0]);
            return 1;
        }
        reject_log = argv[3];
    }

    std::vector<Packet> packets;
    int rejected = 0;
    if (!load_packets(argv[1], reject_log, packets, rejected)) return 1;

    std::cout << "valid=" << packets.size() << " rejected=" << rejected << '\n';
    std::cout << "rejection_log=" << reject_log << '\n';
    return 0;
}
