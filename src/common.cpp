// OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#include "common.h"

const char* type_name(PacketType t) {
    switch (t) {
        case PacketType::TTC: return "TT&C";
        case PacketType::HOUSEKEEPING: return "HOUSEKEEPING";
        case PacketType::SSTV: return "SSTV";
        case PacketType::VOICE: return "VOICE";
    }
    return "UNKNOWN";
}

bool parse_type(const std::string& s, PacketType& out) {
    if (s == "TT&C") {
        out = PacketType::TTC;
        return true;
    }
    if (s == "HOUSEKEEPING") {
        out = PacketType::HOUSEKEEPING;
        return true;
    }
    if (s == "SSTV") {
        out = PacketType::SSTV;
        return true;
    }
    if (s == "VOICE") {
        out = PacketType::VOICE;
        return true;
    }
    return false;
}

int type_priority(PacketType t) {
    switch (t) {
        case PacketType::TTC: return 4;
        case PacketType::HOUSEKEEPING: return 3;
        case PacketType::VOICE: return 2;
        case PacketType::SSTV: return 1;
    }
    return 0;
}

bool is_critical(PacketType t) {
    return (t == PacketType::TTC || t == PacketType::HOUSEKEEPING);
}

const char* action_name(Action a) {
    switch (a) {
        case Action::TRANSMIT: return "TRANSMIT";
        case Action::DEFER: return "DEFER";
        case Action::REJECT: return "REJECT";
    }
    return "UNKNOWN";
}

bool parse_action(const std::string& s, Action& out) {
    if (s == "TRANSMIT") {
        out = Action::TRANSMIT;
        return true;
    }
    if (s == "DEFER") {
        out = Action::DEFER;
        return true;
    }
    if (s == "REJECT") {
        out = Action::REJECT;
        return true;
    }
    return false;
}
