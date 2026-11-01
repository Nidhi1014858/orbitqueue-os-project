// OWNER: Nidhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#include "packet_queue.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

// Helper to wait on a POSIX semaphore, retrying only on EINTR.
// On any other failure, prints a system error and terminates safely.
static void wait_sem_or_die(sem_t* sem, const char* msg) {
    while (sem_wait(sem) != 0) {
        if (errno == EINTR) {
            continue;
        }
        std::perror(msg);
        std::exit(EXIT_FAILURE);
    }
}

// Helper to try acquiring a semaphore without blocking.
// Retries on EINTR. Returns true if acquired immediately, false if EAGAIN.
// Any other failure is fatal and terminates safely.
static bool trywait_sem_or_die(sem_t* sem, const char* msg) {
    while (true) {
        if (sem_trywait(sem) == 0) {
            return true; // Acquired immediately without blocking
        }
        if (errno == EINTR) {
            continue; // Interrupted by signal, retry check
        }
        if (errno == EAGAIN) {
            return false; // Not immediately available; must block next
        }
        std::perror(msg);
        std::exit(EXIT_FAILURE);
    }
}

PacketQueue::PacketQueue(int capacity, int reserved_for_critical)
    : capacity_(capacity > 0 ? capacity : 8),
      reserved_for_critical_(0),
      stats_{0, 0, 0, 0} {
    // Clamp reserved_for_critical_ strictly within [0, capacity_]
    // Handles custom inputs such as PacketQueue(1, 2) -> 1, PacketQueue(1, -1) -> 0, PacketQueue(8, 99) -> 8
    reserved_for_critical_ = std::max(0, std::min(reserved_for_critical, capacity_));

    buffer_.reserve(capacity_);

    // 1. Initialize mutex
    pthread_mutex_init(&mutex_, nullptr);

    // 2. Initialize POSIX semaphores
    // sem_empty: tracks total available slots in the buffer (starts at capacity_)
    sem_init(&sem_empty_, 0, capacity_);

    // sem_full: tracks number of items currently in the buffer (starts at 0)
    sem_init(&sem_full_, 0, 0);

    // sem_bulk: tracks slots available for bulk (non-critical) packets (starts at capacity_ - reserved_for_critical_)
    // Because reserved_for_critical_ is clamped to [0, capacity_], this count is never negative.
    sem_init(&sem_bulk_, 0, capacity_ - reserved_for_critical_);
}

PacketQueue::~PacketQueue() {
    pthread_mutex_destroy(&mutex_);
    sem_destroy(&sem_empty_);
    sem_destroy(&sem_full_);
    sem_destroy(&sem_bulk_);
}

void PacketQueue::push(const Packet& p) {
    bool bulk = !is_critical(p.type);

    // Strict acquisition order: bulk (if applicable) -> empty -> mutex.
    // This ordering prevents deadlocks between competing producers and consumers.
    if (bulk) {
        // Bulk packets (SSTV, VOICE) must acquire a bulk permit first.
        // Waiting for sem_bulk_ alone is NOT counted as blocked_full because the queue
        // as a whole may still have reserved slots free for critical packets.
        wait_sem_or_die(&sem_bulk_, "PacketQueue::push sem_wait(sem_bulk_) failed");
    }

    // Acquire a general empty slot.
    // blocked_full increments only when a producer actually has to wait for sem_empty_.
    bool blocked_on_empty = false;
    if (!trywait_sem_or_die(&sem_empty_, "PacketQueue::push sem_trywait(sem_empty_) failed")) {
        blocked_on_empty = true;
        wait_sem_or_die(&sem_empty_, "PacketQueue::push sem_wait(sem_empty_) failed");
    }

    // Acquire mutex to safely update the queue buffer and statistics
    pthread_mutex_lock(&mutex_);

    if (blocked_on_empty) {
        stats_.blocked_full++;
    }

    buffer_.push_back(p);
    int cur_depth = static_cast<int>(buffer_.size());
    stats_.current_depth = cur_depth;
    if (cur_depth > stats_.max_depth) {
        stats_.max_depth = cur_depth;
    }

    pthread_mutex_unlock(&mutex_);

    // Notify consumers that a new item is available
    sem_post(&sem_full_);
}

Packet PacketQueue::pop() {
    bool blocked_on_empty_queue = false;

    // Wait until at least one packet is present in the queue (sem_full)
    if (!trywait_sem_or_die(&sem_full_, "PacketQueue::pop sem_trywait(sem_full_) failed")) {
        blocked_on_empty_queue = true;
        wait_sem_or_die(&sem_full_, "PacketQueue::pop sem_wait(sem_full_) failed");
    }

    pthread_mutex_lock(&mutex_);

    if (blocked_on_empty_queue) {
        stats_.blocked_empty++;
    }

    // Priority Selection:
    // By the semaphore invariant, sem_full_ guarantees at least one packet is in buffer_.
    // Find the packet with the highest type_priority().
    // Using strict '>' preserves arrival order for packets with equal priority.
    size_t best_idx = 0;
    int best_priority = type_priority(buffer_[0].type);

    for (size_t i = 1; i < buffer_.size(); ++i) {
        int prio = type_priority(buffer_[i].type);
        if (prio > best_priority) {
            best_priority = prio;
            best_idx = i;
        }
    }

    Packet chosen = buffer_[best_idx];
    buffer_.erase(buffer_.begin() + best_idx);
    stats_.current_depth = static_cast<int>(buffer_.size());

    bool was_bulk = !is_critical(chosen.type);

    pthread_mutex_unlock(&mutex_);

    // Release permits:
    // If a bulk packet was consumed, release sem_bulk before sem_empty per contract.
    if (was_bulk) {
        sem_post(&sem_bulk_);
    }
    sem_post(&sem_empty_);

    return chosen;
}

PacketQueue::Stats PacketQueue::stats() const {
    pthread_mutex_lock(&mutex_);
    Stats snapshot = stats_;
    snapshot.current_depth = static_cast<int>(buffer_.size());
    pthread_mutex_unlock(&mutex_);
    return snapshot;
}
