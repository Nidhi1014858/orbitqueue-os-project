// OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#pragma once
#include "common.h"

class PacketQueue {
public:
    struct Stats { int max_depth; long blocked_full; long blocked_empty; };
    explicit PacketQueue(int capacity = 8, int reserved_for_critical = 2);
    ~PacketQueue();
    void push(const Packet& p);   // blocks when full (critical packets may use the reserved slots)
    Packet pop();                 // blocks when empty; returns the highest priority packet
    Stats stats() const;
};
