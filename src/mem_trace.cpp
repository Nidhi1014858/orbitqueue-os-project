// OWNER: Rimjhim - replace this stub with the real implementation. Do not edit if you are not the owner.
#include "mem_trace.h"

namespace memtrace {

const char* region_name(Region r) {
    switch (r) {
        case CODE: return "CODE";
        case MODEL_PARAMS: return "MODEL_PARAMS";
        case INPUT_BUFFER: return "INPUT_BUFFER";
        case OUTPUT_LOG: return "OUTPUT_LOG";
        case STACK: return "STACK";
    }
    return "UNKNOWN";
}

int region_first_page(Region r) {
    switch (r) {
        case CODE: return 0;
        case MODEL_PARAMS: return 2;
        case INPUT_BUFFER: return 4;
        case OUTPUT_LOG: return 6;
        case STACK: return 7;
    }
    return 0;
}

int region_num_pages(Region r) {
    switch (r) {
        case CODE: return 2;
        case MODEL_PARAMS: return 2;
        case INPUT_BUFFER: return 2;
        case OUTPUT_LOG: return 1;
        case STACK: return 1;
    }
    return 1;
}

int page_of(Region r, int offset) {
    int first = region_first_page(r);
    if (offset < 0) {
        return first;
    }
    int num = region_num_pages(r);
    int span = num * PAGE_SIZE;
    if (span <= 0) {
        return first;
    }
    return first + (offset % span) / PAGE_SIZE;
}

bool open_trace(const std::string& /*path*/) {
    // TODO(Rimjhim): Real implementation should write '#' header and open file.
    return true;
}

void access(Region /*r*/, int /*offset*/) {
    // TODO(Rimjhim): Thread-safe access trace logging.
}

void close_trace() {
    // TODO(Rimjhim): Flush and close trace file.
}

} // namespace memtrace
