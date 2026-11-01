// OWNER: Rimjhim - Experiment 10: Page Replacement Algorithms (FIFO, LRU, Optimal)
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <climits>
#include <cctype>

namespace {

enum class Algorithm {
    FIFO,
    LRU,
    OPTIMAL
};

const char* algo_name(Algorithm a) {
    switch (a) {
        case Algorithm::FIFO: return "FIFO";
        case Algorithm::LRU: return "LRU";
        case Algorithm::OPTIMAL: return "Optimal";
    }
    return "UNKNOWN";
}

struct StepRecord {
    int step;
    int page;
    bool hit;
    int replaced_page; // -1 if no page was replaced
    std::vector<int> frames;
};

struct SimulationResult {
    Algorithm algo;
    int num_frames;
    int total_references;
    int page_faults;
    int page_hits;
    double hit_ratio;
    double fault_rate;
    std::vector<StepRecord> trace_steps;
};

// ----------------------------------------------------------------------------
// Algorithm Implementations
// ----------------------------------------------------------------------------

SimulationResult simulate_fifo(const std::vector<int>& refs, int num_frames) {
    SimulationResult res{};
    res.algo = Algorithm::FIFO;
    res.num_frames = num_frames;
    res.total_references = static_cast<int>(refs.size());
    res.page_faults = 0;
    res.page_hits = 0;

    struct FrameSlot {
        int page;
        int arrival_order;
    };
    std::vector<FrameSlot> frames;
    int arrival_counter = 0;

    for (size_t i = 0; i < refs.size(); ++i) {
        int page = refs[i];
        StepRecord step{};
        step.step = static_cast<int>(i + 1);
        step.page = page;
        step.replaced_page = -1;

        auto it = std::find_if(frames.begin(), frames.end(),
                               [page](const FrameSlot& f) { return f.page == page; });
        if (it != frames.end()) {
            step.hit = true;
            res.page_hits++;
        } else {
            step.hit = false;
            res.page_faults++;
            arrival_counter++;

            if (static_cast<int>(frames.size()) < num_frames) {
                frames.push_back({page, arrival_counter});
            } else {
                auto oldest = std::min_element(frames.begin(), frames.end(),
                    [](const FrameSlot& a, const FrameSlot& b) { return a.arrival_order < b.arrival_order; });
                step.replaced_page = oldest->page;
                oldest->page = page;
                oldest->arrival_order = arrival_counter;
            }
        }

        for (const auto& f : frames) {
            step.frames.push_back(f.page);
        }
        res.trace_steps.push_back(step);
    }

    res.hit_ratio = refs.empty() ? 0.0 : static_cast<double>(res.page_hits) / refs.size();
    res.fault_rate = refs.empty() ? 0.0 : static_cast<double>(res.page_faults) / refs.size();
    return res;
}

SimulationResult simulate_lru(const std::vector<int>& refs, int num_frames) {
    SimulationResult res{};
    res.algo = Algorithm::LRU;
    res.num_frames = num_frames;
    res.total_references = static_cast<int>(refs.size());
    res.page_faults = 0;
    res.page_hits = 0;

    struct FrameSlot {
        int page;
        int last_used;
    };
    std::vector<FrameSlot> frames;

    for (size_t i = 0; i < refs.size(); ++i) {
        int page = refs[i];
        int time_now = static_cast<int>(i + 1);
        StepRecord step{};
        step.step = time_now;
        step.page = page;
        step.replaced_page = -1;

        auto it = std::find_if(frames.begin(), frames.end(),
                               [page](const FrameSlot& f) { return f.page == page; });
        if (it != frames.end()) {
            step.hit = true;
            res.page_hits++;
            it->last_used = time_now;
        } else {
            step.hit = false;
            res.page_faults++;

            if (static_cast<int>(frames.size()) < num_frames) {
                frames.push_back({page, time_now});
            } else {
                auto lru_it = std::min_element(frames.begin(), frames.end(),
                    [](const FrameSlot& a, const FrameSlot& b) { return a.last_used < b.last_used; });
                step.replaced_page = lru_it->page;
                lru_it->page = page;
                lru_it->last_used = time_now;
            }
        }

        for (const auto& f : frames) {
            step.frames.push_back(f.page);
        }
        res.trace_steps.push_back(step);
    }

    res.hit_ratio = refs.empty() ? 0.0 : static_cast<double>(res.page_hits) / refs.size();
    res.fault_rate = refs.empty() ? 0.0 : static_cast<double>(res.page_faults) / refs.size();
    return res;
}

SimulationResult simulate_optimal(const std::vector<int>& refs, int num_frames) {
    SimulationResult res{};
    res.algo = Algorithm::OPTIMAL;
    res.num_frames = num_frames;
    res.total_references = static_cast<int>(refs.size());
    res.page_faults = 0;
    res.page_hits = 0;

    struct FrameSlot {
        int page;
        int arrival_order;
    };
    std::vector<FrameSlot> frames;
    int arrival_counter = 0;

    for (size_t i = 0; i < refs.size(); ++i) {
        int page = refs[i];
        StepRecord step{};
        step.step = static_cast<int>(i + 1);
        step.page = page;
        step.replaced_page = -1;

        auto it = std::find_if(frames.begin(), frames.end(),
                               [page](const FrameSlot& f) { return f.page == page; });
        if (it != frames.end()) {
            step.hit = true;
            res.page_hits++;
        } else {
            step.hit = false;
            res.page_faults++;
            arrival_counter++;

            if (static_cast<int>(frames.size()) < num_frames) {
                frames.push_back({page, arrival_counter});
            } else {
                int victim_idx = 0;
                int farthest_next_use = -1;
                int victim_arrival = INT_MAX;

                for (size_t f = 0; f < frames.size(); ++f) {
                    int next_use = INT_MAX;
                    for (size_t j = i + 1; j < refs.size(); ++j) {
                        if (refs[j] == frames[f].page) {
                            next_use = static_cast<int>(j);
                            break;
                        }
                    }

                    if (next_use > farthest_next_use) {
                        farthest_next_use = next_use;
                        victim_idx = static_cast<int>(f);
                        victim_arrival = frames[f].arrival_order;
                    } else if (next_use == farthest_next_use) {
                        // Tie-breaker: FIFO among tied items (earliest arrival into memory)
                        if (frames[f].arrival_order < victim_arrival) {
                            victim_idx = static_cast<int>(f);
                            victim_arrival = frames[f].arrival_order;
                        }
                    }
                }

                step.replaced_page = frames[victim_idx].page;
                frames[victim_idx].page = page;
                frames[victim_idx].arrival_order = arrival_counter;
            }
        }

        for (const auto& f : frames) {
            step.frames.push_back(f.page);
        }
        res.trace_steps.push_back(step);
    }

    res.hit_ratio = refs.empty() ? 0.0 : static_cast<double>(res.page_hits) / refs.size();
    res.fault_rate = refs.empty() ? 0.0 : static_cast<double>(res.page_faults) / refs.size();
    return res;
}

SimulationResult run_simulation(Algorithm algo, const std::vector<int>& refs, int num_frames) {
    switch (algo) {
        case Algorithm::FIFO: return simulate_fifo(refs, num_frames);
        case Algorithm::LRU: return simulate_lru(refs, num_frames);
        case Algorithm::OPTIMAL: return simulate_optimal(refs, num_frames);
    }
    return simulate_fifo(refs, num_frames);
}

// ----------------------------------------------------------------------------
// Trace File Parsing & Validation
// ----------------------------------------------------------------------------

bool parse_trace_file(const std::string& path, std::vector<int>& out_pages, std::string& err_msg) {
    std::ifstream file(path);
    if (!file.is_open()) {
        err_msg = "Cannot open trace file: " + path;
        return false;
    }

    std::string line;
    int line_num = 0;
    out_pages.clear();

    while (std::getline(file, line)) {
        line_num++;
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue; // Blank line
        }
        if (line[first] == '#') {
            continue; // Comment line
        }

        std::istringstream iss(line.substr(first));
        std::string token;
        while (iss >> token) {
            if (token.empty()) continue;
            if (token[0] == '#') {
                break; // Inline comment, ignore rest of line
            }

            // Validate integer format
            size_t start_idx = (token[0] == '-' || token[0] == '+') ? 1 : 0;
            if (start_idx >= token.size()) {
                err_msg = "Non-numeric token '" + token + "' found on line " + std::to_string(line_num);
                return false;
            }
            for (size_t k = start_idx; k < token.size(); ++k) {
                if (!std::isdigit(static_cast<unsigned char>(token[k]))) {
                    err_msg = "Non-numeric token '" + token + "' found on line " + std::to_string(line_num);
                    return false;
                }
            }

            int page = 0;
            try {
                page = std::stoi(token);
            } catch (...) {
                err_msg = "Failed to parse integer token '" + token + "' on line " + std::to_string(line_num);
                return false;
            }

            if (page < 0 || page > 7) {
                err_msg = "Page number " + std::to_string(page) + " out of valid range (0-7) on line " + std::to_string(line_num);
                return false;
            }

            out_pages.push_back(page);
        }
    }

    if (out_pages.empty()) {
        err_msg = "Trace file '" + path + "' is empty or contains no valid page references";
        return false;
    }

    return true;
}

bool parse_frame_list(const std::string& flist, std::vector<int>& out_frames, std::string& err_msg) {
    size_t f_start = flist.find_first_not_of(" \t\r\n");
    if (f_start == std::string::npos) {
        err_msg = "Frame list is empty.";
        return false;
    }
    size_t f_end = flist.find_last_not_of(" \t\r\n");
    std::string trimmed_list = flist.substr(f_start, f_end - f_start + 1);

    if (trimmed_list.front() == ',' || trimmed_list.back() == ',') {
        err_msg = "Malformed frame list '" + flist + "': leading or trailing comma found.";
        return false;
    }

    std::stringstream ss(trimmed_list);
    std::string item;
    out_frames.clear();

    while (std::getline(ss, item, ',')) {
        size_t first = item.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            err_msg = "Malformed frame list '" + flist + "': empty or whitespace-only entry found.";
            return false;
        }
        size_t last = item.find_last_not_of(" \t\r\n");
        std::string token = item.substr(first, last - first + 1);

        size_t start_idx = (token[0] == '+' || token[0] == '-') ? 1 : 0;
        if (start_idx >= token.size()) {
            err_msg = "Invalid non-numeric frame count '" + token + "'.";
            return false;
        }

        for (size_t k = start_idx; k < token.size(); ++k) {
            if (!std::isdigit(static_cast<unsigned char>(token[k]))) {
                err_msg = "Invalid non-numeric frame count '" + token + "'.";
                return false;
            }
        }

        int val = 0;
        try {
            val = std::stoi(token);
        } catch (...) {
            err_msg = "Frame count '" + token + "' is out of integer range.";
            return false;
        }

        if (val < 1) {
            err_msg = "Invalid frame count " + std::to_string(val) + ". Frame count must be at least 1.";
            return false;
        }

        out_frames.push_back(val);
    }

    return true;
}

// ----------------------------------------------------------------------------
// Output Formatting
// ----------------------------------------------------------------------------

void print_verbose_steps(const SimulationResult& res) {
    std::cout << "\n--- Detailed Execution: " << algo_name(res.algo)
              << " (" << res.num_frames << " frames) ---\n";
    for (const auto& s : res.trace_steps) {
        std::cout << "Step " << std::setw(3) << s.step
                  << ": Page " << s.page << " -> ";
        if (s.hit) {
            std::cout << "HIT   ";
        } else {
            std::cout << "FAULT ";
            if (s.replaced_page != -1) {
                std::cout << "(Replaced " << s.replaced_page << ") ";
            } else {
                std::cout << "(Loaded)     ";
            }
        }

        std::cout << "Frames: [";
        for (size_t f = 0; f < s.frames.size(); ++f) {
            std::cout << s.frames[f] << (f + 1 < s.frames.size() ? ", " : "");
        }
        for (int empty = static_cast<int>(s.frames.size()); empty < res.num_frames; ++empty) {
            std::cout << (empty > 0 ? ", " : "") << "-";
        }
        std::cout << "]\n";
    }
}

void print_comparison_table(const std::vector<SimulationResult>& results,
                            const std::string& trace_source,
                            int total_refs) {
    std::cout << "========================================================================================\n";
    std::cout << "       OrbitQueue Exp 10: Page Replacement Algorithm Performance Comparison\n";
    std::cout << "========================================================================================\n";
    std::cout << "Trace Source     : " << trace_source << "\n";
    std::cout << "Total References : " << total_refs << "\n\n";

    std::cout << "+------------+--------+--------+------+-----------+------------+\n";
    std::cout << "| Algorithm  | Frames | Faults | Hits | Hit Ratio | Fault Rate |\n";
    std::cout << "+------------+--------+--------+------+-----------+------------+\n";

    for (const auto& r : results) {
        std::cout << "| " << std::left << std::setw(10) << algo_name(r.algo)
                  << " | " << std::right << std::setw(6) << r.num_frames
                  << " | " << std::setw(6) << r.page_faults
                  << " | " << std::setw(4) << r.page_hits
                  << " | " << std::setw(9) << std::fixed << std::setprecision(4) << r.hit_ratio
                  << " | " << std::setw(10) << std::fixed << std::setprecision(4) << r.fault_rate
                  << " |\n";
    }
    std::cout << "+------------+--------+--------+------+-----------+------------+\n";
}

// ----------------------------------------------------------------------------
// Verification Self-Test
// ----------------------------------------------------------------------------

int run_selftest() {
    std::cout << "======================================================================\n";
    std::cout << "  Running Experiment 10 Verification Self-Test (Hand-Verified Values)\n";
    std::cout << "======================================================================\n\n";

    int failures = 0;

    auto check_case = [&](Algorithm algo, const std::vector<int>& refs, int frames,
                          int exp_faults, int exp_hits, const std::string& desc) {
        SimulationResult r = run_simulation(algo, refs, frames);
        if (r.page_faults != exp_faults || r.page_hits != exp_hits) {
            std::cerr << "[FAIL] " << desc << ": expected faults=" << exp_faults
                      << ", hits=" << exp_hits << "; got faults=" << r.page_faults
                      << ", hits=" << r.page_hits << "\n";
            failures++;
        } else {
            std::cout << "[PASS] " << desc << " (Faults: " << r.page_faults << ", Hits: " << r.page_hits << ")\n";
        }
    };

    // Standard Lab Reference String: 7 0 1 2 0 3 0 4 2 3 0 3
    const std::vector<int> lab_trace = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3};

    std::cout << "--- 1. Reference String: 7 0 1 2 0 3 0 4 2 3 0 3 (1 Frame) ---\n";
    check_case(Algorithm::FIFO,    lab_trace, 1, 12, 0, "FIFO 1 frame");
    check_case(Algorithm::LRU,     lab_trace, 1, 12, 0, "LRU 1 frame");
    check_case(Algorithm::OPTIMAL, lab_trace, 1, 12, 0, "Optimal 1 frame");

    std::cout << "\n--- 2. Reference String: 7 0 1 2 0 3 0 4 2 3 0 3 (3 Frames) ---\n";
    check_case(Algorithm::FIFO,    lab_trace, 3, 10, 2, "FIFO 3 frames");
    check_case(Algorithm::LRU,     lab_trace, 3, 9,  3, "LRU 3 frames");
    // Corrected Optimal result: 7 faults, 5 hits (replaces page 0 at step 8)
    check_case(Algorithm::OPTIMAL, lab_trace, 3, 7,  5, "Optimal 3 frames (Corrected: 7 faults, 5 hits)");

    std::cout << "\n--- 3. Reference String: 7 0 1 2 0 3 0 4 2 3 0 3 (4 Frames) ---\n";
    check_case(Algorithm::FIFO,    lab_trace, 4, 7,  5, "FIFO 4 frames");
    check_case(Algorithm::LRU,     lab_trace, 4, 6,  6, "LRU 4 frames");
    check_case(Algorithm::OPTIMAL, lab_trace, 4, 6,  6, "Optimal 4 frames");

    std::cout << "\n--- 4. Belady's Anomaly Verification (String: 1 2 3 4 1 2 5 1 2 3 4 5) ---\n";
    const std::vector<int> belady_trace = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    SimulationResult b_fifo_3 = simulate_fifo(belady_trace, 3);
    SimulationResult b_fifo_4 = simulate_fifo(belady_trace, 4);

    if (b_fifo_3.page_faults != 9) {
        std::cerr << "[FAIL] Belady FIFO 3 frames: expected 9 faults, got " << b_fifo_3.page_faults << "\n";
        failures++;
    } else {
        std::cout << "[PASS] Belady FIFO 3 frames: exactly 9 faults\n";
    }

    if (b_fifo_4.page_faults != 10) {
        std::cerr << "[FAIL] Belady FIFO 4 frames: expected 10 faults, got " << b_fifo_4.page_faults << "\n";
        failures++;
    } else {
        std::cout << "[PASS] Belady FIFO 4 frames: exactly 10 faults\n";
    }

    if (b_fifo_4.page_faults > b_fifo_3.page_faults) {
        std::cout << "[PASS] Belady's Anomaly confirmed: faults increased from 9 to 10 when frames increased from 3 to 4\n";
    } else {
        std::cerr << "[FAIL] Belady's Anomaly not triggered\n";
        failures++;
    }

    std::cout << "\n======================================================================\n";
    if (failures > 0) {
        std::cerr << "SELF-TEST RESULT: FAILED (" << failures << " errors)\n";
        return 2;
    }

    std::cout << "SELF-TEST RESULT: ALL TESTS PASSED\n";
    return 0;
}

} // namespace

// ----------------------------------------------------------------------------
// Main CLI Driver
// ----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::string ref_file;
    std::string algo_choice = "all";
    std::vector<int> frame_counts;
    bool verbose = false;
    bool selftest_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--selftest") {
            selftest_mode = true;
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --ref-file <PATH>     Path to page reference trace file (required unless --selftest)\n";
            std::cout << "  --frames <LIST>       Comma-separated frame counts (default: 1,3,4,5)\n";
            std::cout << "  --algo <ALGO>         Algorithm: all | fifo | lru | opt (default: all)\n";
            std::cout << "  --verbose             Display step-by-step frame state after each page reference\n";
            std::cout << "  --selftest            Run hand-verified test vectors and exit with code 0 or 2\n";
            std::cout << "  --help, -h            Display this help message\n";
            return 0;
        } else if (arg == "--ref-file") {
            if (i + 1 < argc) {
                ref_file = argv[++i];
            } else {
                std::cerr << "Error: --ref-file requires a PATH argument.\n";
                return 1;
            }
        } else if (arg.rfind("--ref-file=", 0) == 0) {
            ref_file = arg.substr(11);
        } else if (arg == "--frames") {
            if (i + 1 < argc) {
                std::string flist = argv[++i];
                std::string err;
                if (!parse_frame_list(flist, frame_counts, err)) {
                    std::cerr << "Error: " << err << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --frames requires a comma-separated list of integers.\n";
                return 1;
            }
        } else if (arg.rfind("--frames=", 0) == 0) {
            std::string flist = arg.substr(9);
            std::string err;
            if (!parse_frame_list(flist, frame_counts, err)) {
                std::cerr << "Error: " << err << "\n";
                return 1;
            }
        } else if (arg == "--algo") {
            if (i + 1 < argc) {
                algo_choice = argv[++i];
            } else {
                std::cerr << "Error: --algo requires an algorithm name (all|fifo|lru|opt).\n";
                return 1;
            }
        } else if (arg.rfind("--algo=", 0) == 0) {
            algo_choice = arg.substr(7);
        } else {
            std::cerr << "Error: Unrecognized option '" << arg << "'. Use --help for usage.\n";
            return 1;
        }
    }

    if (selftest_mode) {
        return run_selftest();
    }

    if (ref_file.empty()) {
        std::cerr << "Error: Missing required argument --ref-file <PATH>.\n";
        std::cerr << "Example: " << argv[0] << " --ref-file data/page_trace_sample.txt --frames 1,3,4,5\n";
        return 1;
    }

    // Default frames if none specified
    if (frame_counts.empty()) {
        frame_counts = {1, 3, 4, 5};
    }

    // Validate frame counts
    for (int f : frame_counts) {
        if (f < 1) {
            std::cerr << "Error: Invalid frame count " << f << ". Frame count must be at least 1.\n";
            return 1;
        }
    }

    // Validate algorithm choice
    std::string algo_lower = algo_choice;
    std::transform(algo_lower.begin(), algo_lower.end(), algo_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<Algorithm> algos_to_run;
    if (algo_lower == "all") {
        algos_to_run = {Algorithm::FIFO, Algorithm::LRU, Algorithm::OPTIMAL};
    } else if (algo_lower == "fifo") {
        algos_to_run = {Algorithm::FIFO};
    } else if (algo_lower == "lru") {
        algos_to_run = {Algorithm::LRU};
    } else if (algo_lower == "opt" || algo_lower == "optimal") {
        algos_to_run = {Algorithm::OPTIMAL};
    } else {
        std::cerr << "Error: Invalid algorithm choice '" << algo_choice
                  << "'. Supported values: all, fifo, lru, opt.\n";
        return 1;
    }

    // Parse reference trace file
    std::vector<int> references;
    std::string err_msg;
    if (!parse_trace_file(ref_file, references, err_msg)) {
        std::cerr << "Error: " << err_msg << "\n";
        return 1;
    }

    // Label if using sample data
    std::string label = ref_file;
    if (ref_file.find("sample") != std::string::npos || ref_file.find("SAMPLE") != std::string::npos) {
        label += " (SAMPLE DATA - synthetic teaching trace, not real router output)";
    }

    // Execute simulations
    std::vector<SimulationResult> results;
    for (int fc : frame_counts) {
        for (Algorithm alg : algos_to_run) {
            SimulationResult r = run_simulation(alg, references, fc);
            results.push_back(r);
            if (verbose) {
                print_verbose_steps(r);
            }
        }
    }

    // Print summary comparison table
    print_comparison_table(results, label, static_cast<int>(references.size()));
    return 0;
}
