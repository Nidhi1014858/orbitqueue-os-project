// OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner.
#pragma once
#include <string>

namespace rm {
constexpr int NUM_TASKS = 5;
constexpr int NUM_RES = 3;   // A = CPU slots, B = buffer units, C = transmitter slots

enum Task { T0_TELEMETRY_HANDLER = 0, T1_PACKET_ROUTER = 1, T2_SSTV_PROCESSOR = 2,
            T3_VOICE_CODEC = 3, T4_DOWNLINK_PREP = 4 };
enum class Result { GRANTED, WAIT, DENIED_UNSAFE, ERROR_EXCEEDS_NEED, ERROR_INVALID };

struct State {
    int allocation[NUM_TASKS][NUM_RES];
    int max[NUM_TASKS][NUM_RES];
    int available[NUM_RES];
};

const char* task_name(int task);
const char* result_name(Result r);
State default_state();                                   // base state S1
bool init(const State& s, std::string* err = nullptr);   // validates, then resets shared state
void init_default();
bool is_safe(const State& s, int safe_sequence[NUM_TASKS]);  // true if safe, fills the sequence
Result request(int task, const int req[NUM_RES]);        // thread-safe (one internal mutex)
void release(int task, const int amount[NUM_RES]);       // thread-safe, clamped to current allocation
State snapshot();
}
