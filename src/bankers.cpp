#include "resource_manager.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

void print_vector(const int values[rm::NUM_RES]) {
    std::cout << '(' << values[0] << ',' << values[1] << ',' << values[2] << ')';
}

void print_state(const rm::State& state) {
    std::cout << "Task  Allocation  Max      Need\n";
    for (int task = 0; task < rm::NUM_TASKS; ++task) {
        int need[rm::NUM_RES];
        for (int resource = 0; resource < rm::NUM_RES; ++resource) {
            need[resource] = state.max[task][resource] -
                             state.allocation[task][resource];
        }
        std::cout << 'T' << task << "    ";
        print_vector(state.allocation[task]);
        std::cout << "     ";
        print_vector(state.max[task]);
        std::cout << "  ";
        print_vector(need);
        std::cout << '\n';
    }
    std::cout << "Available: ";
    print_vector(state.available);
    std::cout << '\n';
}

bool print_safety_steps(const rm::State& state) {
    int work[rm::NUM_RES];
    bool finished[rm::NUM_TASKS] = {false, false, false, false, false};
    int sequence[rm::NUM_TASKS];
    int completed = 0;
    for (int resource = 0; resource < rm::NUM_RES; ++resource) {
        work[resource] = state.available[resource];
    }

    bool progress = true;
    while (completed < rm::NUM_TASKS && progress) {
        progress = false;
        for (int task = 0; task < rm::NUM_TASKS; ++task) {
            if (finished[task]) continue;
            int need[rm::NUM_RES];
            bool can_finish = true;
            for (int resource = 0; resource < rm::NUM_RES; ++resource) {
                need[resource] = state.max[task][resource] -
                                 state.allocation[task][resource];
                if (need[resource] > work[resource]) can_finish = false;
            }
            if (!can_finish) continue;

            std::cout << "Step " << (completed + 1) << ": T" << task
                      << " can finish; Work ";
            print_vector(work);
            std::cout << " + Allocation ";
            print_vector(state.allocation[task]);
            for (int resource = 0; resource < rm::NUM_RES; ++resource) {
                work[resource] += state.allocation[task][resource];
            }
            std::cout << " -> ";
            print_vector(work);
            std::cout << '\n';
            finished[task] = true;
            sequence[completed++] = task;
            progress = true;
        }
    }

    if (completed != rm::NUM_TASKS) {
        std::cout << "UNSAFE: no unfinished task has Need <= Work.\n"
                  << "This does not prove a deadlock already exists; it means a "
                     "safe completion order cannot be guaranteed.\n";
        return false;
    }

    std::cout << "SAFE sequence: ";
    for (int index = 0; index < rm::NUM_TASKS; ++index) {
        if (index != 0) std::cout << " -> ";
        std::cout << 'T' << sequence[index];
    }
    std::cout << '\n';
    return true;
}

rm::State state_with_available(int a, int b, int c) {
    rm::State state = rm::default_state();
    state.available[0] = a;
    state.available[1] = b;
    state.available[2] = c;
    return state;
}

bool run_s1() {
    std::cout << "\n=== S1: safe base state ===\n";
    const rm::State state = rm::default_state();
    print_state(state);
    return print_safety_steps(state);
}

bool run_s3() {
    std::cout << "\n=== S3: unsafe state ===\n";
    const rm::State state = state_with_available(0, 1, 0);
    print_state(state);
    return !print_safety_steps(state);
}

bool run_request_case(int task, const int request[rm::NUM_RES],
                      rm::Result expected) {
    rm::State state = state_with_available(1, 2, 1);
    std::string error;
    if (!rm::init(state, &error)) {
        std::cerr << "Initialization error: " << error << '\n';
        return false;
    }

    std::cout << "Request T" << task << ' ';
    print_vector(request);
    const rm::Result actual = rm::request(task, request);
    std::cout << " -> " << rm::result_name(actual) << '\n';
    if (actual == rm::Result::GRANTED) print_safety_steps(rm::snapshot());
    return actual == expected;
}

bool run_s2() {
    std::cout << "\n=== S2: independent request cases ===\n";
    const int grant[rm::NUM_RES] = {1, 1, 1};
    const int unsafe[rm::NUM_RES] = {1, 2, 1};
    const int wait[rm::NUM_RES] = {2, 1, 2};
    const int exceeds[rm::NUM_RES] = {1, 2, 0};
    bool ok = true;
    ok = run_request_case(rm::T1_PACKET_ROUTER, grant, rm::Result::GRANTED) && ok;
    ok = run_request_case(rm::T0_TELEMETRY_HANDLER, unsafe,
                          rm::Result::DENIED_UNSAFE) && ok;
    ok = run_request_case(rm::T2_SSTV_PROCESSOR, wait, rm::Result::WAIT) && ok;
    ok = run_request_case(rm::T2_SSTV_PROCESSOR, exceeds,
                          rm::Result::ERROR_EXCEEDS_NEED) && ok;
    return ok;
}

bool same_sequence(const int actual[rm::NUM_TASKS],
                   const int expected[rm::NUM_TASKS]) {
    for (int index = 0; index < rm::NUM_TASKS; ++index) {
        if (actual[index] != expected[index]) return false;
    }
    return true;
}

bool invalid_state_tests() {
    std::string error;
    rm::State negative = rm::default_state();
    negative.available[0] = -1;
    if (rm::init(negative, &error)) return false;

    rm::State above_max = rm::default_state();
    above_max.allocation[0][0] = above_max.max[0][0] + 1;
    if (rm::init(above_max, &error)) return false;

    rm::State zero_tasks = rm::default_state();
    for (int resource = 0; resource < rm::NUM_RES; ++resource) {
        zero_tasks.allocation[0][resource] = 0;
        zero_tasks.max[0][resource] = 0;
    }
    return !rm::init(zero_tasks, &error);
}

int selftest() {
    int sequence[rm::NUM_TASKS];
    const int expected_s1[rm::NUM_TASKS] = {0, 1, 2, 3, 4};
    const int expected_s2[rm::NUM_TASKS] = {1, 3, 4, 0, 2};

    rm::State s1 = rm::default_state();
    bool ok = rm::is_safe(s1, sequence) && same_sequence(sequence, expected_s1);

    rm::State s3 = state_with_available(0, 1, 0);
    ok = !rm::is_safe(s3, sequence) && ok;

    const int grant[rm::NUM_RES] = {1, 1, 1};
    std::string error;
    ok = rm::init(state_with_available(1, 2, 1), &error) && ok;
    ok = rm::request(rm::T1_PACKET_ROUTER, grant) == rm::Result::GRANTED && ok;
    ok = rm::is_safe(rm::snapshot(), sequence) &&
         same_sequence(sequence, expected_s2) && ok;

    const int unsafe[rm::NUM_RES] = {1, 2, 1};
    ok = rm::init(state_with_available(1, 2, 1), &error) && ok;
    ok = rm::request(rm::T0_TELEMETRY_HANDLER, unsafe) ==
             rm::Result::DENIED_UNSAFE && ok;

    const int wait[rm::NUM_RES] = {2, 1, 2};
    ok = rm::init(state_with_available(1, 2, 1), &error) && ok;
    ok = rm::request(rm::T2_SSTV_PROCESSOR, wait) == rm::Result::WAIT && ok;

    const int exceeds[rm::NUM_RES] = {1, 2, 0};
    ok = rm::request(rm::T2_SSTV_PROCESSOR, exceeds) ==
             rm::Result::ERROR_EXCEEDS_NEED && ok;
    ok = invalid_state_tests() && ok;

    std::cout << (ok ? "SELFTEST PASS\n" : "SELFTEST FAIL\n");
    return ok ? 0 : 2;
}

int interactive() {
    rm::State state{};
    std::cout << "Enter Allocation matrix (5 rows x 3 integers):\n";
    for (int task = 0; task < rm::NUM_TASKS; ++task) {
        for (int resource = 0; resource < rm::NUM_RES; ++resource) {
            if (!(std::cin >> state.allocation[task][resource])) return 1;
        }
    }
    std::cout << "Enter Max matrix (5 rows x 3 integers):\n";
    for (int task = 0; task < rm::NUM_TASKS; ++task) {
        for (int resource = 0; resource < rm::NUM_RES; ++resource) {
            if (!(std::cin >> state.max[task][resource])) return 1;
        }
    }
    std::cout << "Enter Available (3 integers):\n";
    for (int resource = 0; resource < rm::NUM_RES; ++resource) {
        if (!(std::cin >> state.available[resource])) return 1;
    }

    std::string error;
    if (!rm::init(state, &error)) {
        std::cerr << "Invalid state: " << error << '\n';
        return 1;
    }
    print_state(state);
    print_safety_steps(state);
    return 0;
}

void usage(const char* program) {
    std::cerr << "Usage: " << program
              << " --scenario S1|S2|S3|all | --interactive | --selftest\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--interactive") return interactive();
    if (argc == 2 && std::string(argv[1]) == "--selftest") return selftest();
    if (argc == 3 && std::string(argv[1]) == "--scenario") {
        const std::string scenario = argv[2];
        bool ok = true;
        if (scenario == "S1") ok = run_s1();
        else if (scenario == "S2") ok = run_s2();
        else if (scenario == "S3") ok = run_s3();
        else if (scenario == "all") {
            const bool s1_ok = run_s1();
            const bool s2_ok = run_s2();
            const bool s3_ok = run_s3();
            ok = s1_ok && s2_ok && s3_ok;
        }
        else {
            usage(argv[0]);
            return 1;
        }
        return ok ? 0 : 2;
    }
    usage(argv[0]);
    return 1;
}
