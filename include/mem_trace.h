// OWNER: Rimjhim - memory trace interface specification
#pragma once
#include <string>

namespace memtrace {
constexpr int PAGE_SIZE = 100;   // bytes (synthetic teaching value)
constexpr int NUM_PAGES = 8;

// The enum value is also the segment number used in Exp 8.
enum Region { CODE = 0, MODEL_PARAMS = 1, INPUT_BUFFER = 2, OUTPUT_LOG = 3, STACK = 4 };

const char* region_name(Region r);
int region_first_page(Region r);   // CODE 0, MODEL_PARAMS 2, INPUT_BUFFER 4, OUTPUT_LOG 6, STACK 7
int region_num_pages(Region r);    // 2, 2, 2, 1, 1
// first_page + (offset % (num_pages * PAGE_SIZE)) / PAGE_SIZE ; a negative offset maps to the first page
int page_of(Region r, int offset);

bool open_trace(const std::string& path);   // writes a '#' header line
void access(Region r, int offset);          // thread-safe; appends page_of(r, offset)
void close_trace();
}
