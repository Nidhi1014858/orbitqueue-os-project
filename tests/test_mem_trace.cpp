// OWNER: Rimjhim - standalone self-test for mem_trace module
#include "mem_trace.h"

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static int s_failed = 0;

#define CHECK_EQ(actual, expected, msg) do { \
    if ((actual) != (expected)) { \
        std::cerr << "[FAIL] " << msg << ": expected " << (expected) << ", got " << (actual) << "\n"; \
        s_failed++; \
    } \
} while(0)

int main() {
    std::cout << "=== Running mem_trace Unit and Concurrency Self-Test ===\n";

    // 1. Check region page ranges and counts
    CHECK_EQ(memtrace::region_first_page(memtrace::CODE), 0, "CODE first page");
    CHECK_EQ(memtrace::region_num_pages(memtrace::CODE), 2, "CODE num pages");

    CHECK_EQ(memtrace::region_first_page(memtrace::MODEL_PARAMS), 2, "MODEL_PARAMS first page");
    CHECK_EQ(memtrace::region_num_pages(memtrace::MODEL_PARAMS), 2, "MODEL_PARAMS num pages");

    CHECK_EQ(memtrace::region_first_page(memtrace::INPUT_BUFFER), 4, "INPUT_BUFFER first page");
    CHECK_EQ(memtrace::region_num_pages(memtrace::INPUT_BUFFER), 2, "INPUT_BUFFER num pages");

    CHECK_EQ(memtrace::region_first_page(memtrace::OUTPUT_LOG), 6, "OUTPUT_LOG first page");
    CHECK_EQ(memtrace::region_num_pages(memtrace::OUTPUT_LOG), 1, "OUTPUT_LOG num pages");

    CHECK_EQ(memtrace::region_first_page(memtrace::STACK), 7, "STACK first page");
    CHECK_EQ(memtrace::region_num_pages(memtrace::STACK), 1, "STACK num pages");

    // 2. Known (region, offset) mappings
    CHECK_EQ(memtrace::page_of(memtrace::CODE, 0), 0, "CODE offset 0 -> page 0");
    CHECK_EQ(memtrace::page_of(memtrace::CODE, 99), 0, "CODE offset 99 -> page 0");
    CHECK_EQ(memtrace::page_of(memtrace::CODE, 100), 1, "CODE offset 100 -> page 1");
    CHECK_EQ(memtrace::page_of(memtrace::CODE, 199), 1, "CODE offset 199 -> page 1");

    CHECK_EQ(memtrace::page_of(memtrace::MODEL_PARAMS, 0), 2, "MODEL_PARAMS offset 0 -> page 2");
    CHECK_EQ(memtrace::page_of(memtrace::MODEL_PARAMS, 150), 3, "MODEL_PARAMS offset 150 -> page 3");

    CHECK_EQ(memtrace::page_of(memtrace::INPUT_BUFFER, 50), 4, "INPUT_BUFFER offset 50 -> page 4");
    CHECK_EQ(memtrace::page_of(memtrace::INPUT_BUFFER, 150), 5, "INPUT_BUFFER offset 150 -> page 5");

    CHECK_EQ(memtrace::page_of(memtrace::OUTPUT_LOG, 0), 6, "OUTPUT_LOG offset 0 -> page 6");
    CHECK_EQ(memtrace::page_of(memtrace::STACK, 50), 7, "STACK offset 50 -> page 7");

    // 3. Negative and huge offsets
    CHECK_EQ(memtrace::page_of(memtrace::CODE, -10), 0, "CODE negative offset -> page 0");
    CHECK_EQ(memtrace::page_of(memtrace::INPUT_BUFFER, -1), 4, "INPUT_BUFFER negative offset -> page 4");
    CHECK_EQ(memtrace::page_of(memtrace::STACK, -500), 7, "STACK negative offset -> page 7");

    CHECK_EQ(memtrace::page_of(memtrace::CODE, 1000), 0, "CODE huge offset 1000 wraps -> page 0");
    CHECK_EQ(memtrace::page_of(memtrace::CODE, 1150), 1, "CODE huge offset 1150 wraps -> page 1");

    // 4. Access before open_trace must not crash
    std::cout << "[INFO] Testing safe access before open_trace...\n";
    memtrace::access(memtrace::CODE, 10);
    memtrace::access(memtrace::STACK, 20);

    // 5. Concurrent 20,000 accesses test (2 threads x 10,000)
    std::string test_path = "outputs/test_mem_trace_concurrent.txt";
    std::cout << "[INFO] Testing concurrent logging into " << test_path << "...\n";
    if (!memtrace::open_trace(test_path)) {
        std::cerr << "[FAIL] Failed to open trace file: " << test_path << "\n";
        return 2;
    }

    constexpr int ITERS_PER_THREAD = 10000;
    std::thread t1([]() {
        for (int i = 0; i < ITERS_PER_THREAD; ++i) {
            memtrace::access(memtrace::CODE, i % 200);
        }
    });

    std::thread t2([]() {
        for (int i = 0; i < ITERS_PER_THREAD; ++i) {
            memtrace::access(memtrace::INPUT_BUFFER, i % 200);
        }
    });

    t1.join();
    t2.join();
    memtrace::close_trace();

    // Verify written file
    std::ifstream in(test_path);
    if (!in.is_open()) {
        std::cerr << "[FAIL] Could not reopen trace file for verification: " << test_path << "\n";
        return 2;
    }

    std::string header;
    if (!std::getline(in, header) || header.empty() || header[0] != '#') {
        std::cerr << "[FAIL] Missing or invalid '#' header line\n";
        s_failed++;
    }

    int line_count = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        int page = std::stoi(line);
        if (page < 0 || page > 7) {
            std::cerr << "[FAIL] Page out of bounds (0..7): " << page << "\n";
            s_failed++;
        }
        line_count++;
    }

    CHECK_EQ(line_count, 2 * ITERS_PER_THREAD, "Total concurrent trace lines");

    if (s_failed > 0) {
        std::cerr << "[FAIL] mem_trace self-test encountered " << s_failed << " failure(s).\n";
        return 2;
    }

    std::cout << "[PASS] All mem_trace checks passed (including 20,000 concurrent accesses).\n";
    return 0;
}

