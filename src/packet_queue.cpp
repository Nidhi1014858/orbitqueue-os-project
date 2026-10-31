// OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner.
// STUB: NOT THREAD SAFE, TODO(Nidhi)
#include "packet_queue.h"
#include <deque>

static std::deque<Packet> s_stub_queue;

PacketQueue::PacketQueue(int /*capacity*/, int /*reserved_for_critical*/) {}

PacketQueue::~PacketQueue() {}

void PacketQueue::push(const Packet& p) {
    s_stub_queue.push_back(p);
}

Packet PacketQueue::pop() {
    if (s_stub_queue.empty()) {
        Packet dummy{0, PacketType::TTC, 1, 100, 100, 100};
        return dummy;
    }
    Packet p = s_stub_queue.front();
    s_stub_queue.pop_front();
    return p;
}

PacketQueue::Stats PacketQueue::stats() const {
    return Stats{static_cast<int>(s_stub_queue.size()), 0, 0};
}
