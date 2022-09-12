#include "m61.hh"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>


struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20; /* 8 MiB */

    m61_memory_buffer();
    ~m61_memory_buffer();
};

static m61_memory_buffer default_buffer;


m61_memory_buffer::m61_memory_buffer() {
    void* buf = mmap(nullptr,    // Place the buffer at a random address
        this->size,              // Buffer should be 8 MiB big
        PROT_WRITE,              // We want to read and write the buffer
        MAP_ANON | MAP_PRIVATE, -1, 0);
                                 // We want memory freshly allocated by the OS
    assert(buf != MAP_FAILED);
    this->buffer = (char*) buf;
}

m61_memory_buffer::~m61_memory_buffer() {
    munmap(this->buffer, this->size);
}

// Structure for keeping track of the malloc statistics
// Using for phase 1

struct allocation_statistics {
    unsigned long long n_mallocs = 0;
    unsigned long long n_frees = 0;
    unsigned long long n_fails = 0;
    unsigned long long allocation_bytes = 0;
    unsigned long long failed_bytes = 0;
    unsigned long long smallest_bytes = 0;
    unsigned long long largest_bytes = 0;
    uintptr_t smallest_bytes_location;
    uintptr_t largest_bytes_location;
};

static allocation_statistics my_data;


/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    if (default_buffer.pos + sz > default_buffer.size) {
        // Not enough space left in default buffer for allocation

        //increase failcount
        my_data.n_fails++;
        //increase failbytes
        my_data.failed_bytes += sz;

        return nullptr;
    }

    // Otherwise there is enough space; claim the next `sz` bytes
    void* ptr = &default_buffer.buffer[default_buffer.pos];
    default_buffer.pos += sz;

    // Adding to malloc count
    my_data.n_mallocs ++;
    // Adding to byte count
    my_data.allocation_bytes += sz;

    // Check if heap min or max

    if (sz > my_data.largest_bytes) {
        my_data.largest_bytes = sz;
        my_data.largest_bytes_location = (uintptr_t) ptr;
    }
    if (sz < my_data.smallest_bytes) {
        my_data.smallest_bytes = sz;
        my_data.smallest_bytes_location = (uintptr_t) ptr;
    }
    return ptr;
}


/// m61_free(ptr, file, line)
///    Frees the memory allocation pointed to by `ptr`. If `ptr == nullptr`,
///    does nothing. Otherwise, `ptr` must point to a currently active
///    allocation returned by `m61_malloc`. The free was called at location
///    `file`:`line`.

void m61_free(void* ptr, const char* file, int line) {
    // avoid uninitialized variable warnings
    (void) ptr, (void) file, (void) line;
    // Your code here. The handout code does nothing!
    if (ptr != nullptr) {
        //Adding to free count
        my_data.n_frees ++;
    }
}


/// m61_calloc(count, sz, file, line)
///    Returns a pointer a fresh dynamic memory allocation big enough to
///    hold an array of `count` elements of `sz` bytes each. Returned
///    memory is initialized to zero. The allocation request was at
///    location `file`:`line`. Returns `nullptr` if out of memory; may
///    also return `nullptr` if `count == 0` or `size == 0`.

void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    // Your code here (to fix test019).
    void* ptr = m61_malloc(count * sz, file, line);
    if (ptr) {
        memset(ptr, 0, count * sz);
    }
    return ptr;
}


/// m61_get_statistics()
///    Return the current memory statistics.

m61_statistics m61_get_statistics() {
    // Your code here.
    // The handout code sets all statistics to enormous numbers.
    m61_statistics stats;
    stats.nactive = my_data.n_mallocs - my_data.n_frees;
    stats.ntotal = my_data.n_mallocs;
    stats.total_size = my_data.allocation_bytes;
    stats.nfail = my_data.n_fails;
    stats.fail_size = my_data.failed_bytes;
    stats.heap_max = my_data.largest_bytes_location;
    stats.heap_min = my_data.smallest_bytes_location;

    //memset(&stats, 255, sizeof(m61_statistics));
    return stats;
}


/// m61_print_statistics()
///    Prints the current memory statistics.

void m61_print_statistics() {
    m61_statistics stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_print_leak_report()
///    Prints a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    // Your code here.
}
