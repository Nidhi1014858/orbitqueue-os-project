// OWNER: Surbhi - replace this stub with the real implementation. Do not edit if you are not the owner.
// STUB, TODO(Surbhi)
#include "resource_manager.h"

namespace rm {

const char* task_name(int task) {
    switch (task) {
        case T0_TELEMETRY_HANDLER: return "T0 TelemetryHandler";
        case T1_PACKET_ROUTER: return "T1 PacketRouter";
        case T2_SSTV_PROCESSOR: return "T2 SSTVProcessor";
        case T3_VOICE_CODEC: return "T3 VoiceCodec";
        case T4_DOWNLINK_PREP: return "T4 DownlinkPrep";
    }
    return "UNKNOWN_TASK";
}

const char* result_name(Result r) {
    switch (r) {
        case Result::GRANTED: return "GRANTED";
        case Result::WAIT: return "WAIT";
        case Result::DENIED_UNSAFE: return "DENIED_UNSAFE";
        case Result::ERROR_EXCEEDS_NEED: return "ERROR_EXCEEDS_NEED";
        case Result::ERROR_INVALID: return "ERROR_INVALID";
    }
    return "UNKNOWN_RESULT";
}

State default_state() {
    State s;
    // S1 Allocation: T0 (1,0,1), T1 (1,1,0), T2 (1,2,1), T3 (0,1,1), T4 (1,0,0)
    int alloc[NUM_TASKS][NUM_RES] = {
        {1, 0, 1},
        {1, 1, 0},
        {1, 2, 1},
        {0, 1, 1},
        {1, 0, 0}
    };
    // S1 Max: T0 (3,2,2), T1 (2,2,1), T2 (3,3,3), T3 (1,2,2), T4 (2,1,1)
    int mx[NUM_TASKS][NUM_RES] = {
        {3, 2, 2},
        {2, 2, 1},
        {3, 3, 3},
        {1, 2, 2},
        {2, 1, 1}
    };
    // S1 Available: (2,2,1)
    int avail[NUM_RES] = {2, 2, 1};

    for (int i = 0; i < NUM_TASKS; ++i) {
        for (int j = 0; j < NUM_RES; ++j) {
            s.allocation[i][j] = alloc[i][j];
            s.max[i][j] = mx[i][j];
        }
    }
    for (int j = 0; j < NUM_RES; ++j) {
        s.available[j] = avail[j];
    }
    return s;
}

bool init(const State& /*s*/, std::string* /*err*/) {
    // STUB, TODO(Surbhi)
    return true;
}

void init_default() {
    // STUB, TODO(Surbhi)
}

bool is_safe(const State& /*s*/, int safe_sequence[NUM_TASKS]) {
    // STUB, TODO(Surbhi): return trivial safe sequence
    for (int i = 0; i < NUM_TASKS; ++i) {
        safe_sequence[i] = i;
    }
    return true;
}

Result request(int /*task*/, const int /*req*/[NUM_RES]) {
    // STUB, TODO(Surbhi): always granted in stub
    return Result::GRANTED;
}

void release(int /*task*/, const int /*amount*/[NUM_RES]) {
    // STUB, TODO(Surbhi): no-op in stub
}

State snapshot() {
    // STUB, TODO(Surbhi): returns base state S1
    return default_state();
}

} // namespace rm
