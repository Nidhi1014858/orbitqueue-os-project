// OWNER: Rimjhim - Experiment 8: Paging and Segmentation Simulation
#include "mem_trace.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int PAGE_SIZE = 100;    // Synthetic teaching page size (bytes)
constexpr int NUM_PAGES = 8;      // 8 logical pages (0 to 7)
constexpr int NUM_SEGMENTS = 5;   // 5 logical segments (0 to 4)

struct PageEntry {
    int page;
    int frame; // -1 indicates page fault (page absent from physical memory)
    const char* region;
    const char* description;
};

// Page table mapping as specified in TASK_RIMJHIM.md
static const PageEntry PAGE_TABLE[NUM_PAGES] = {
    {0,  4, "CODE",         "Executable instruction memory (part 1)"},
    {1,  2, "CODE",         "Executable instruction memory (part 2)"},
    {2, -1, "MODEL_PARAMS", "ML weights and parameters (absent, PAGE FAULT)"},
    {3,  5, "MODEL_PARAMS", "ML weights and parameters (part 2)"},
    {4,  7, "INPUT_BUFFER", "Downlink packet staging buffer (part 1)"},
    {5,  1, "INPUT_BUFFER", "Downlink packet staging buffer (part 2)"},
    {6,  6, "OUTPUT_LOG",   "Routing log and telemetry buffer"},
    {7,  0, "STACK",        "Call stack and local frames"}
};

struct SegmentEntry {
    int segment;
    const char* name;
    int base;
    int limit;
    const char* description;
};

// Segment table (base, limit) as specified in TASK_RIMJHIM.md
static const SegmentEntry SEGMENT_TABLE[NUM_SEGMENTS] = {
    {0, "CODE",         1000, 400, "Executable instructions"},
    {1, "MODEL_PARAMS", 2000, 600, "ML model weights and parameters"},
    {2, "INPUT_BUFFER", 3000, 300, "Staging buffer for incoming downlink chunks"},
    {3, "OUTPUT_LOG",   4000, 500, "Telemetry and routing logs"},
    {4, "STACK",        5000, 200, "Execution stack frames"}
};

} // namespace

enum class PagingStatus {
    OK,
    INVALID_PAGE,
    INVALID_OFFSET,
    PAGE_FAULT
};

struct PagingResult {
    PagingStatus status;
    int page;
    int offset;
    int frame;
    int physical_address;
};

enum class SegmentStatus {
    OK,
    INVALID_SEGMENT,
    INVALID_OFFSET,
    SEGMENTATION_VIOLATION
};

struct SegmentResult {
    SegmentStatus status;
    int segment;
    int offset;
    int base;
    int limit;
    int physical_address;
};

void print_tables() {
    std::cout << "======================================================================\n";
    std::cout << "  OrbitQueue Memory Management Tables (Teaching Simulation Model)\n";
    std::cout << "  Note: All values and limits are synthetic teaching examples.\n";
    std::cout << "======================================================================\n";

    std::cout << "\n--- Page Table (Page Size: " << PAGE_SIZE << " bytes, Total Pages: " << NUM_PAGES << ") ---\n";
    std::cout << std::left << std::setw(8) << "Page"
              << std::setw(10) << "Frame"
              << std::setw(16) << "Region"
              << "Description / Status\n";
    std::cout << std::string(68, '-') << "\n";
    for (int i = 0; i < NUM_PAGES; ++i) {
        std::cout << std::left << std::setw(8) << PAGE_TABLE[i].page;
        if (PAGE_TABLE[i].frame == -1) {
            std::cout << std::setw(10) << "-1 (Fault)";
        } else {
            std::cout << std::setw(10) << PAGE_TABLE[i].frame;
        }
        std::cout << std::setw(16) << PAGE_TABLE[i].region
                  << PAGE_TABLE[i].description << "\n";
    }

    std::cout << "\n--- Segment Table (Total Segments: " << NUM_SEGMENTS << ") ---\n";
    std::cout << std::left << std::setw(10) << "Segment"
              << std::setw(16) << "Name"
              << std::setw(10) << "Base"
              << std::setw(10) << "Limit"
              << "Description\n";
    std::cout << std::string(68, '-') << "\n";
    for (int i = 0; i < NUM_SEGMENTS; ++i) {
        std::cout << std::left << std::setw(10) << SEGMENT_TABLE[i].segment
                  << std::setw(16) << SEGMENT_TABLE[i].name
                  << std::setw(10) << SEGMENT_TABLE[i].base
                  << std::setw(10) << SEGMENT_TABLE[i].limit
                  << SEGMENT_TABLE[i].description << "\n";
    }
    std::cout << "======================================================================\n\n";
}

PagingResult translate_paging(int page, int offset, bool print_steps = true) {
    PagingResult res{};
    res.page = page;
    res.offset = offset;
    res.frame = -1;
    res.physical_address = -1;

    if (print_steps) {
        std::cout << "[PAGING TRANSLATION] Input Logical Address: (Page: " << page << ", Offset: " << offset << ")\n";
    }

    // 1. Validate page number
    if (page < 0 || page >= NUM_PAGES) {
        res.status = PagingStatus::INVALID_PAGE;
        if (print_steps) {
            std::cout << "  -> ERROR: Invalid page number " << page
                      << " (valid range: 0 to " << NUM_PAGES - 1 << ").\n\n";
        }
        return res;
    }

    // 2. Validate offset within page
    if (offset < 0 || offset >= PAGE_SIZE) {
        res.status = PagingStatus::INVALID_OFFSET;
        if (print_steps) {
            std::cout << "  -> ERROR: Invalid offset " << offset
                      << " (valid range: 0 to " << PAGE_SIZE - 1 << " for page size " << PAGE_SIZE << ").\n\n";
        }
        return res;
    }

    // 3. Lookup frame in page table
    int frame = PAGE_TABLE[page].frame;
    res.frame = frame;
    const char* region = PAGE_TABLE[page].region;

    if (print_steps) {
        std::cout << "  Step 1: Page Lookup -> Page " << page << " belongs to region " << region << "\n";
        std::cout << "  Step 2: Table Lookup -> Page " << page << " maps to Frame " << frame << "\n";
    }

    // 4. Check for page fault
    if (frame == -1) {
        res.status = PagingStatus::PAGE_FAULT;
        if (print_steps) {
            std::cout << "  -> PAGE FAULT: Page " << page << " (" << region
                      << ") is absent from memory (not loaded yet).\n\n";
        }
        return res;
    }

    // 5. Calculate physical address
    int physical = frame * PAGE_SIZE + offset;
    res.physical_address = physical;
    res.status = PagingStatus::OK;

    if (print_steps) {
        std::cout << "  Step 3: Physical Address Calculation\n";
        std::cout << "          physical = frame x page_size + offset\n";
        std::cout << "                   = " << frame << " x " << PAGE_SIZE << " + " << offset << "\n";
        std::cout << "                   = " << (frame * PAGE_SIZE) << " + " << offset << "\n";
        std::cout << "                   = " << physical << "\n";
        std::cout << "  -> RESULT: Frame " << frame << ", Physical Address " << physical << "\n\n";
    }

    return res;
}

SegmentResult translate_segment(int segment, int offset, bool print_steps = true) {
    SegmentResult res{};
    res.segment = segment;
    res.offset = offset;
    res.base = -1;
    res.limit = -1;
    res.physical_address = -1;

    if (print_steps) {
        std::cout << "[SEGMENTATION TRANSLATION] Input Logical Address: (Segment: " << segment << ", Offset: " << offset << ")\n";
    }

    // 1. Validate segment number
    if (segment < 0 || segment >= NUM_SEGMENTS) {
        res.status = SegmentStatus::INVALID_SEGMENT;
        if (print_steps) {
            std::cout << "  -> ERROR: Invalid segment number " << segment
                      << " (valid range: 0 to " << NUM_SEGMENTS - 1 << ").\n\n";
        }
        return res;
    }

    // 2. Validate non-negative offset
    if (offset < 0) {
        res.status = SegmentStatus::INVALID_OFFSET;
        if (print_steps) {
            std::cout << "  -> ERROR: Invalid offset " << offset
                      << " (offset cannot be negative).\n\n";
        }
        return res;
    }

    int base = SEGMENT_TABLE[segment].base;
    int limit = SEGMENT_TABLE[segment].limit;
    const char* name = SEGMENT_TABLE[segment].name;
    res.base = base;
    res.limit = limit;

    if (print_steps) {
        std::cout << "  Step 1: Segment Lookup -> Segment " << segment << " (" << name << ")\n";
        std::cout << "          Base = " << base << ", Limit = " << limit << "\n";
    }

    // 3. Boundary check: offset < limit
    if (print_steps) {
        std::cout << "  Step 2: Boundary Check -> check if offset (" << offset << ") < limit (" << limit << ")\n";
    }

    if (offset >= limit) {
        res.status = SegmentStatus::SEGMENTATION_VIOLATION;
        if (print_steps) {
            std::cout << "  -> SEGMENTATION VIOLATION: Offset " << offset
                      << " exceeds or equals limit " << limit
                      << " for segment " << segment << " (" << name << ").\n\n";
        }
        return res;
    }

    // 4. Calculate physical address
    int physical = base + offset;
    res.physical_address = physical;
    res.status = SegmentStatus::OK;

    if (print_steps) {
        std::cout << "  Step 3: Physical Address Calculation\n";
        std::cout << "          physical = base + offset\n";
        std::cout << "                   = " << base << " + " << offset << "\n";
        std::cout << "                   = " << physical << "\n";
        std::cout << "  -> RESULT: Physical Address " << physical << "\n\n";
    }

    return res;
}

bool run_test_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open test file: " << path << "\n";
        return false;
    }

    std::cout << "=== Running Test Cases from " << path << " ===\n";
    print_tables();

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        // Trim leading spaces
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue; // Empty line
        }
        if (line[first] == '#') {
            continue; // Comment line
        }

        std::istringstream iss(line.substr(first));
        char type;
        if (!(iss >> type)) {
            continue;
        }

        if (type == 'P' || type == 'p') {
            int page = 0, offset = 0;
            std::string extra;
            bool valid_line = false;
            if (iss >> page >> offset) {
                if (!(iss >> extra) || extra[0] == '#') {
                    valid_line = true;
                }
            }
            if (valid_line) {
                translate_paging(page, offset, true);
            } else {
                std::cerr << "Line " << line_num << ": Malformed paging test (invalid syntax or unexpected trailing tokens): " << line << "\n";
            }
        } else if (type == 'S' || type == 's') {
            int segment = 0, offset = 0;
            std::string extra;
            bool valid_line = false;
            if (iss >> segment >> offset) {
                if (!(iss >> extra) || extra[0] == '#') {
                    valid_line = true;
                }
            }
            if (valid_line) {
                translate_segment(segment, offset, true);
            } else {
                std::cerr << "Line " << line_num << ": Malformed segmentation test (invalid syntax or unexpected trailing tokens): " << line << "\n";
            }
        } else {
            std::cerr << "Line " << line_num << ": Unknown test type '" << type << "' in line: " << line << "\n";
        }
    }

    std::cout << "=== Test File Execution Finished ===\n";
    return true;
}

int run_selftest() {
    std::cout << "========================================================\n";
    std::cout << "  Running Experiment 8 and mem_trace Verification Self-Test\n";
    std::cout << "========================================================\n\n";

    int failures = 0;

    auto check_paging = [&](int page, int offset, PagingStatus expected_status, int expected_frame, int expected_phys, const std::string& label) {
        PagingResult r = translate_paging(page, offset, false);
        if (r.status != expected_status) {
            std::cerr << "[FAIL] " << label << " status mismatch: expected "
                      << static_cast<int>(expected_status) << ", got " << static_cast<int>(r.status) << "\n";
            failures++;
            return;
        }
        if (expected_status == PagingStatus::OK) {
            if (r.frame != expected_frame || r.physical_address != expected_phys) {
                std::cerr << "[FAIL] " << label << " calculation mismatch: expected frame "
                          << expected_frame << ", phys " << expected_phys
                          << "; got frame " << r.frame << ", phys " << r.physical_address << "\n";
                failures++;
                return;
            }
        }
        std::cout << "[PASS] " << label << "\n";
    };

    auto check_segment = [&](int segment, int offset, SegmentStatus expected_status, int expected_phys, const std::string& label) {
        SegmentResult r = translate_segment(segment, offset, false);
        if (r.status != expected_status) {
            std::cerr << "[FAIL] " << label << " status mismatch: expected "
                      << static_cast<int>(expected_status) << ", got " << static_cast<int>(r.status) << "\n";
            failures++;
            return;
        }
        if (expected_status == SegmentStatus::OK) {
            if (r.physical_address != expected_phys) {
                std::cerr << "[FAIL] " << label << " physical address mismatch: expected "
                          << expected_phys << ", got " << r.physical_address << "\n";
                failures++;
                return;
            }
        }
        std::cout << "[PASS] " << label << "\n";
    };

    std::cout << "--- 1. Paging Expected Cases ---\n";
    // P 1 25 gives frame 2, physical 225
    check_paging(1, 25, PagingStatus::OK, 2, 225, "P 1 25 -> Frame 2, Physical 225");
    // P 3 99 gives frame 5, physical 599
    check_paging(3, 99, PagingStatus::OK, 5, 599, "P 3 99 -> Frame 5, Physical 599");
    // P 2 20 gives PAGE FAULT (MODEL_PARAMS page absent)
    check_paging(2, 20, PagingStatus::PAGE_FAULT, -1, -1, "P 2 20 -> PAGE FAULT");
    // P 9 5 gives invalid page number
    check_paging(9, 5, PagingStatus::INVALID_PAGE, -1, -1, "P 9 5 -> Invalid page number");
    // P 4 100 gives invalid offset
    check_paging(4, 100, PagingStatus::INVALID_OFFSET, -1, -1, "P 4 100 -> Invalid offset (>=100)");
    // P 4 -3 gives invalid offset
    check_paging(4, -3, PagingStatus::INVALID_OFFSET, -1, -1, "P 4 -3 -> Invalid offset (<0)");

    std::cout << "\n--- 2. Segmentation Expected Cases ---\n";
    // S 1 150 gives 2150
    check_segment(1, 150, SegmentStatus::OK, 2150, "S 1 150 -> Physical 2150");
    // S 2 299 gives 3299 (last valid byte)
    check_segment(2, 299, SegmentStatus::OK, 3299, "S 2 299 -> Physical 3299");
    // S 2 300 gives SEGMENTATION VIOLATION (oversized chunk into 300-byte INPUT_BUFFER)
    check_segment(2, 300, SegmentStatus::SEGMENTATION_VIOLATION, -1, "S 2 300 -> SEGMENTATION VIOLATION");
    // S 2 450 gives SEGMENTATION VIOLATION
    check_segment(2, 450, SegmentStatus::SEGMENTATION_VIOLATION, -1, "S 2 450 -> SEGMENTATION VIOLATION");
    // S 7 0 gives invalid segment number
    check_segment(7, 0, SegmentStatus::INVALID_SEGMENT, -1, "S 7 0 -> Invalid segment number");
    // S 4 199 gives 5199 (from data/addr_tests.txt)
    check_segment(4, 199, SegmentStatus::OK, 5199, "S 4 199 -> Physical 5199");
    // S 4 200 gives SEGMENTATION VIOLATION (limit is 200)
    check_segment(4, 200, SegmentStatus::SEGMENTATION_VIOLATION, -1, "S 4 200 -> SEGMENTATION VIOLATION");
    // S 0 -5 gives invalid offset
    check_segment(0, -5, SegmentStatus::INVALID_OFFSET, -1, "S 0 -5 -> Invalid offset (<0)");

    std::cout << "\n--- 3. Memory Trace Mapping and Region Checks ---\n";
    // Verify memtrace initial page mappings and region definitions
    if (memtrace::region_first_page(memtrace::CODE) != 0 || memtrace::region_num_pages(memtrace::CODE) != 2 ||
        memtrace::region_first_page(memtrace::MODEL_PARAMS) != 2 || memtrace::region_num_pages(memtrace::MODEL_PARAMS) != 2 ||
        memtrace::region_first_page(memtrace::INPUT_BUFFER) != 4 || memtrace::region_num_pages(memtrace::INPUT_BUFFER) != 2 ||
        memtrace::region_first_page(memtrace::OUTPUT_LOG) != 6 || memtrace::region_num_pages(memtrace::OUTPUT_LOG) != 1 ||
        memtrace::region_first_page(memtrace::STACK) != 7 || memtrace::region_num_pages(memtrace::STACK) != 1) {
        std::cerr << "[FAIL] memtrace region page ranges mismatch\n";
        failures++;
    } else {
        std::cout << "[PASS] memtrace region definitions (CODE 0-1, MODEL 2-3, INPUT 4-5, LOG 6, STACK 7)\n";
    }

    if (memtrace::page_of(memtrace::CODE, 0) != 0 ||
        memtrace::page_of(memtrace::CODE, 99) != 0 ||
        memtrace::page_of(memtrace::CODE, 100) != 1 ||
        memtrace::page_of(memtrace::MODEL_PARAMS, 0) != 2 ||
        memtrace::page_of(memtrace::MODEL_PARAMS, 150) != 3 ||
        memtrace::page_of(memtrace::INPUT_BUFFER, 0) != 4 ||
        memtrace::page_of(memtrace::INPUT_BUFFER, 150) != 5 ||
        memtrace::page_of(memtrace::OUTPUT_LOG, 0) != 6 ||
        memtrace::page_of(memtrace::STACK, 0) != 7) {
        std::cerr << "[FAIL] memtrace page_of mapping calculation mismatch\n";
        failures++;
    } else {
        std::cout << "[PASS] memtrace page_of mapping calculations\n";
    }

    // Negative offset handling (must map to first page)
    if (memtrace::page_of(memtrace::CODE, -10) != 0 ||
        memtrace::page_of(memtrace::INPUT_BUFFER, -1) != 4 ||
        memtrace::page_of(memtrace::STACK, -500) != 7) {
        std::cerr << "[FAIL] memtrace negative offset handling\n";
        failures++;
    } else {
        std::cout << "[PASS] memtrace negative offset handling\n";
    }

    // Huge offset wrapping (wraps within region page span)
    if (memtrace::page_of(memtrace::CODE, 1000) != 0 ||
        memtrace::page_of(memtrace::CODE, 1150) != 1) {
        std::cerr << "[FAIL] memtrace huge offset wrapping\n";
        failures++;
    } else {
        std::cout << "[PASS] memtrace huge offset wrapping\n";
    }

    std::cout << "\n========================================================\n";
    if (failures > 0) {
        std::cerr << "SELF-TEST RESULT: FAILED (" << failures << " errors)\n";
        return 2;
    }

    std::cout << "SELF-TEST RESULT: ALL TESTS PASSED\n";
    return 0;
}

void interactive_menu() {
    print_tables();

    while (true) {
        std::cout << "========================================================\n";
        std::cout << "    Memory Management Interactive Menu (Exp 8)          \n";
        std::cout << "========================================================\n";
        std::cout << "1. Translate Logical Address using Paging\n";
        std::cout << "2. Translate Logical Address using Segmentation\n";
        std::cout << "3. Display Page Table and Segment Table\n";
        std::cout << "4. Run Address Test File (e.g. data/addr_tests.txt)\n";
        std::cout << "5. Run Self-Test\n";
        std::cout << "6. Exit\n";
        std::cout << "Enter selection (1-6): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            // EOF or input stream error
            break;
        }

        if (choice == 1) {
            int page = 0, offset = 0;
            std::cout << "Enter logical page number (0-7): ";
            if (!(std::cin >> page)) break;
            std::cout << "Enter offset (0-99): ";
            if (!(std::cin >> offset)) break;
            std::cout << "\n";
            translate_paging(page, offset, true);
        } else if (choice == 2) {
            int segment = 0, offset = 0;
            std::cout << "Enter segment number (0-4):\n";
            std::cout << "  0: CODE (limit 400)\n";
            std::cout << "  1: MODEL_PARAMS (limit 600)\n";
            std::cout << "  2: INPUT_BUFFER (limit 300)\n";
            std::cout << "  3: OUTPUT_LOG (limit 500)\n";
            std::cout << "  4: STACK (limit 200)\n";
            std::cout << "Choice: ";
            if (!(std::cin >> segment)) break;
            std::cout << "Enter offset: ";
            if (!(std::cin >> offset)) break;
            std::cout << "\n";
            translate_segment(segment, offset, true);
        } else if (choice == 3) {
            print_tables();
        } else if (choice == 4) {
            std::string path;
            std::cout << "Enter test file path (default: data/addr_tests.txt): ";
            std::cin >> path;
            if (path.empty()) {
                path = "data/addr_tests.txt";
            }
            run_test_file(path);
        } else if (choice == 5) {
            run_selftest();
        } else if (choice == 6) {
            std::cout << "Exiting Memory Management simulator.\n";
            break;
        } else {
            std::cout << "Invalid choice. Please select an option between 1 and 6.\n\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        interactive_menu();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--selftest") {
        return run_selftest();
    }

    if (arg1 == "--test") {
        if (argc < 3) {
            std::cerr << "Error: --test requires a FILE argument.\n";
            std::cerr << "Usage: " << argv[0] << " --test <FILE>\n";
            return 1;
        }
        std::string test_file = argv[2];
        bool ok = run_test_file(test_file);
        return ok ? 0 : 1;
    }

    if (arg1.rfind("--test=", 0) == 0) {
        std::string test_file = arg1.substr(7);
        if (test_file.empty()) {
            std::cerr << "Error: --test= requires a FILE argument.\n";
            return 1;
        }
        bool ok = run_test_file(test_file);
        return ok ? 0 : 1;
    }

    if (arg1 == "--help" || arg1 == "-h") {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << "                    # Interactive menu\n";
        std::cout << "  " << argv[0] << " --test FILE        # Run batch test file (e.g. data/addr_tests.txt)\n";
        std::cout << "  " << argv[0] << " --selftest         # Run verification self-tests (exits 0 or 2)\n";
        return 0;
    }

    std::cerr << "Error: Unrecognized option: " << arg1 << "\n";
    std::cerr << "Run '" << argv[0] << " --help' for usage.\n";
    return 1;
}
