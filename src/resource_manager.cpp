#include "resource_manager.h"

#include <algorithm>
#include <mutex>

namespace rm {
namespace {

State shared_state{};
bool initialized = false;
std::mutex state_mutex;

bool validate(const State& state, std::string* error) {
    int total[NUM_RES] = {0, 0, 0};
    for (int resource = 0; resource < NUM_RES; ++resource) {
        if (state.available[resource] < 0) {
            if (error) *error = "Available values cannot be negative";
            return false;
        }
        total[resource] = state.available[resource];
    }

    for (int task = 0; task < NUM_TASKS; ++task) {
        int task_maximum = 0;
        for (int resource = 0; resource < NUM_RES; ++resource) {
            const int allocation = state.allocation[task][resource];
            const int maximum = state.max[task][resource];
            if (allocation < 0 || maximum < 0) {
                if (error) *error = "Allocation and Max values cannot be negative";
                return false;
            }
            if (allocation > maximum) {
                if (error) *error = "Allocation cannot be greater than Max";
                return false;
            }
            total[resource] += allocation;
            task_maximum += maximum;
        }
        if (task_maximum == 0) {
            if (error) *error = "Each task must declare at least one resource";
            return false;
        }
    }

    for (int resource = 0; resource < NUM_RES; ++resource) {
        if (total[resource] <= 0) {
            if (error) *error = "Each resource must have a positive total count";
            return false;
        }
        for (int task = 0; task < NUM_TASKS; ++task) {
            if (state.max[task][resource] > total[resource]) {
                if (error) *error = "A Max claim exceeds the total resource count";
                return false;
            }
        }
    }
    return true;
}

}  // namespace

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

const char* result_name(Result result) {
    switch (result) {
        case Result::GRANTED: return "GRANTED";
        case Result::WAIT: return "WAIT";
        case Result::DENIED_UNSAFE: return "DENIED_UNSAFE";
        case Result::ERROR_EXCEEDS_NEED: return "ERROR_EXCEEDS_NEED";
        case Result::ERROR_INVALID: return "ERROR_INVALID";
    }
    return "UNKNOWN_RESULT";
}

State default_state() {
    State state{};
    const int allocation[NUM_TASKS][NUM_RES] = {
        {1, 0, 1}, {1, 1, 0}, {1, 2, 1}, {0, 1, 1}, {1, 0, 0}};
    const int maximum[NUM_TASKS][NUM_RES] = {
        {3, 2, 2}, {2, 2, 1}, {3, 3, 3}, {1, 2, 2}, {2, 1, 1}};
    const int available[NUM_RES] = {2, 2, 1};

    for (int task = 0; task < NUM_TASKS; ++task) {
        for (int resource = 0; resource < NUM_RES; ++resource) {
            state.allocation[task][resource] = allocation[task][resource];
            state.max[task][resource] = maximum[task][resource];
        }
    }
    for (int resource = 0; resource < NUM_RES; ++resource) {
        state.available[resource] = available[resource];
    }
    return state;
}

bool init(const State& state, std::string* error) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!validate(state, error)) return false;
    shared_state = state;
    initialized = true;
    if (error) error->clear();
    return true;
}

void init_default() {
    std::string ignored;
    init(default_state(), &ignored);
}

bool is_safe(const State& state, int safe_sequence[NUM_TASKS]) {
    int work[NUM_RES];
    bool finished[NUM_TASKS] = {false, false, false, false, false};
    int completed = 0;
    for (int resource = 0; resource < NUM_RES; ++resource) {
        work[resource] = state.available[resource];
    }

    bool progress = true;
    while (completed < NUM_TASKS && progress) {
        progress = false;
        for (int task = 0; task < NUM_TASKS; ++task) {
            if (finished[task]) continue;

            bool can_finish = true;
            for (int resource = 0; resource < NUM_RES; ++resource) {
                const int need = state.max[task][resource] -
                                 state.allocation[task][resource];
                if (need > work[resource]) can_finish = false;
            }
            if (!can_finish) continue;

            for (int resource = 0; resource < NUM_RES; ++resource) {
                work[resource] += state.allocation[task][resource];
            }
            finished[task] = true;
            safe_sequence[completed++] = task;
            progress = true;
        }
    }

    for (int index = completed; index < NUM_TASKS; ++index) {
        safe_sequence[index] = -1;
    }
    return completed == NUM_TASKS;
}

Result request(int task, const int request_values[NUM_RES]) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!initialized || task < 0 || task >= NUM_TASKS ||
        request_values == nullptr) {
        return Result::ERROR_INVALID;
    }

    for (int resource = 0; resource < NUM_RES; ++resource) {
        if (request_values[resource] < 0) return Result::ERROR_INVALID;
        const int need = shared_state.max[task][resource] -
                         shared_state.allocation[task][resource];
        if (request_values[resource] > need) {
            return Result::ERROR_EXCEEDS_NEED;
        }
    }
    for (int resource = 0; resource < NUM_RES; ++resource) {
        if (request_values[resource] > shared_state.available[resource]) {
            return Result::WAIT;
        }
    }

    for (int resource = 0; resource < NUM_RES; ++resource) {
        shared_state.available[resource] -= request_values[resource];
        shared_state.allocation[task][resource] += request_values[resource];
    }

    int sequence[NUM_TASKS];
    if (is_safe(shared_state, sequence)) return Result::GRANTED;

    for (int resource = 0; resource < NUM_RES; ++resource) {
        shared_state.available[resource] += request_values[resource];
        shared_state.allocation[task][resource] -= request_values[resource];
    }
    return Result::DENIED_UNSAFE;
}

void release(int task, const int amount[NUM_RES]) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (!initialized || task < 0 || task >= NUM_TASKS || amount == nullptr) return;

    for (int resource = 0; resource < NUM_RES; ++resource) {
        const int requested = std::max(0, amount[resource]);
        const int released =
            std::min(requested, shared_state.allocation[task][resource]);
        shared_state.allocation[task][resource] -= released;
        shared_state.available[resource] += released;
    }
}

State snapshot() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return shared_state;
}

}  // namespace rm
