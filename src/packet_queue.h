// OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#pragma once
#include "common.h"
#include <pthread.h>
#include <semaphore.h>
#include <vector>

/**
 * @brief Thread-safe bounded priority queue for satellite packet routing.
 *
 * Implements a bounded buffer using POSIX semaphores and a mutex lock.
 * Supports priority scheduling and slot reservation for critical packets
 * (TT&C and HOUSEKEEPING) so bulk packets (SSTV and VOICE) cannot monopolize
 * the queue.
 */
class PacketQueue {
public:
    struct Stats {
        int current_depth;
        int max_depth;
        long blocked_full;
        long blocked_empty;
    };

    explicit PacketQueue(int capacity = 8, int reserved_for_critical = 2);
    ~PacketQueue();

    // Prevent copying because synchronization primitives (mutex/semaphores) cannot be copied
    PacketQueue(const PacketQueue&) = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;

    void push(const Packet& p);   // blocks when full (critical packets may use the reserved slots)
    Packet pop();                 // blocks when empty; returns highest priority packet (arrival order for ties)
    Stats stats() const;

private:
    int capacity_;
    int reserved_for_critical_;
    std::vector<Packet> buffer_;

    // Synchronization primitives
    mutable pthread_mutex_t mutex_;
    sem_t sem_empty_;  // Tracks total free slots (initially capacity)
    sem_t sem_full_;   // Tracks available occupied slots (initially 0)
    sem_t sem_bulk_;   // Tracks slots available to non-critical packets (capacity - reserved)

    // Statistics tracking
    Stats stats_;
};
