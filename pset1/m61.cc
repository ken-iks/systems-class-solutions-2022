#include "m61.hh"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <iostream>
#include <map>
static void * m61_find_free_space(size_t, size_t);
const size_t SIZECONST = sizeof('#');

// # is my special character that I have at the end of every allocation
// That is for checking for boundary-write errors

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
    unsigned long long freed_bytes = 0;
    uintptr_t smallest_bytes_location = SIZE_MAX;
    uintptr_t largest_bytes_location = 0;
};

static allocation_statistics my_data;


// Map for global set of freed allocations
// Maps a buffer position to a size 

// Other map is for active allocations
// Again maps a bufer position to a size

std::map<size_t, size_t> freed_allocations;
using freemap_iter = std::map<size_t, size_t>::iterator;
std::map<size_t, size_t> active_allocations;

// Extra info stores the file and line for a given active allocation
struct extra_info {
    const char* file;
    int line;
};

// Map for storing some extra metadata for active allocations
std::map<size_t, extra_info> allocation_info;
using extramap_iter = std::map<size_t, extra_info>::iterator;

// Function which computes the amount of padding that needs to be added
size_t add_padding(size_t sz) {
    size_t pad = alignof(max_align_t) - (sz % alignof(max_align_t));
    if (pad == alignof(max_align_t)) {
        return 0;
    }
    return pad;
}

// Initially setting up the freed allocation to have the 8Mib buffer
// This is to make it easy to coalesce never-freed memory
// Assertions are to ensure alignments are correct

int setup() {
    while (default_buffer.pos % alignof(max_align_t) != 0) {
        default_buffer.pos++;
    } 
    assert(default_buffer.pos % 16 == 0);
    size_t starting_pos = (size_t) &default_buffer.buffer[default_buffer.pos];
    assert(starting_pos % 16 == 0);
    freed_allocations.insert({starting_pos, (8<<20)});
    auto testing2 = (char*) (starting_pos + (8<<20) - SIZECONST);
    *testing2 = '#';
    return 0;
}

// This is for debugging purposes -> prints out the freed allocations
void check_freed() {
    for (auto p = freed_allocations.begin();
    p != freed_allocations.end();
    p++) {
        printf("%zx is of size %zx\n", p->first, p->second);
        printf("%zx mod 16 = %li\n", p->first, p->first%alignof(max_align_t));
    }
}

auto starter = setup();

// Function to update statistics every time a new malloc is performed

static void new_malloc(void* ptr, size_t sz, const char* file, int line) {
    my_data.n_mallocs++;
    my_data.allocation_bytes += sz;
    assert((size_t) ptr % 16 == 0);

    // Check if heap min or max
    uintptr_t ptr_addr = (uintptr_t) ptr;

    if (ptr_addr >= my_data.largest_bytes_location) {
        my_data.largest_bytes_location = (uintptr_t) ptr + sz;
    }
    if (ptr_addr <= my_data.smallest_bytes_location) {
        my_data.smallest_bytes_location = (uintptr_t) ptr;
    }

    // Update active allocations with original information
    active_allocations[(size_t) ptr] = sz + SIZECONST;
    allocation_info[(size_t) ptr] = {file, line};

    // Space_im_taking is the actual size that the malloc has
    // This is original size + the size of our special character
    // We then add padding to it for alignement
    size_t space_im_taking = sz + SIZECONST;
    space_im_taking += add_padding(space_im_taking);

    // Checking if the malloc is on the original buffer
    // If so we may need to break it up into smaller pieces
    auto it = freed_allocations.find((size_t) ptr);
    if (it != freed_allocations.end()) {
        size_t spare_space = it->second - space_im_taking;
        assert(spare_space % alignof(max_align_t) == 0);
        if (spare_space > 0) {
            auto temp1 = it->first;
            freed_allocations.erase(it);
            assert(temp1 % 16 == 0);
            freed_allocations.insert({temp1 + space_im_taking, spare_space});
        }
        else {
            freed_allocations.erase(it);
        }
    }

}

// Function for checking coalescance

bool can_coalesce_up(freemap_iter it) {
    assert(it != freed_allocations.end());
    auto next = it;
    ++next;
    if (next == freed_allocations.end()) {
        return false;
    }
    return it->first + it->second == next->first;
}

bool can_coalesce_down(freemap_iter it) {
    assert(it != freed_allocations.end());
    if (it == freed_allocations.begin()) {
        return false;
    }
    auto prev = it;
    --prev;
    return (prev->first + prev->second == it->first);
}

void coalesce_up(freemap_iter it) {
    assert(can_coalesce_up(it));
    auto next = it;
    ++next;
    it->second += next->second;
    auto testing2 = (char*) (it->first + it->second - SIZECONST);
    *testing2 = '#';
    freed_allocations.erase(next);
}

// Function to look through the free spots for some space to allocate

static void * m61_find_free_space(size_t sz, size_t partial_size) {
    // First check if there are any gaps in freed space big enough to
    // store something of size sz. If so, return a pointer to that, if
    // not, then return nullptr

    for (auto p = freed_allocations.begin();
        p != freed_allocations.end();
        p++) {

        if (sz <= p->second) {
            void* ptr = (void*) p->first;
            // Spare memory in the block that will not be used
            size_t spare_space = p->second - sz;
            assert(p->first % 16 == 0);
            assert(p->second % 16 == 0);
            assert(spare_space % 16 == 0);
            assert(sz % 16 == 0);
            if (spare_space > 0) {
                freed_allocations[p->first + sz] = spare_space ;
            }
            auto testing2 = (char*) (p->first + partial_size);
            *testing2 = '#';
            freed_allocations.erase(p->first);
            return ptr;               
        }
    }
    return nullptr;
}

/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    default_buffer.pos += add_padding(default_buffer.pos);

    assert(default_buffer.pos % 16 == 0);

    if (sz == 0)  {
        return nullptr;
    }   

    size_t whole_size = sz + SIZECONST;
    whole_size+=add_padding(whole_size);

    if (default_buffer.pos + whole_size > default_buffer.size 
        || SIZE_MAX - sz < default_buffer.pos) {

        // Not enough space left in default buffer for allocation
        // Or check for integer overflow

        // Look up allocation sizes to see if it can fit into a freed spot
        // Only do this if it wasn't integer overflow

        if (default_buffer.pos + whole_size > default_buffer.size) {
            assert(whole_size % 16 == 0);
            void* pot_ptr = m61_find_free_space(whole_size, sz);
            // If buffer position can't be moved backwards then check if freed
            // allocations contain enough space
            if (pot_ptr) {
                new_malloc(pot_ptr,sz,file,line);
                return pot_ptr;
            }
        }

        // No spot found
        // increase failcount
        my_data.n_fails++;
        // increase failbytes
        my_data.failed_bytes += sz;

        return nullptr;
    }

    // Otherwise there is enough space; claim the next `sz` bytes
    void* ptr = &default_buffer.buffer[default_buffer.pos];

    size_t total_size = sz + SIZECONST;
    total_size += add_padding(total_size);

    assert(total_size % 16 == 0);


    assert(default_buffer.pos % 16 == 0);

    // Adding special character to the end of buffer
    default_buffer.buffer[default_buffer.pos + sz] = '#';

    default_buffer.pos += total_size;

    assert(default_buffer.pos % 16 ==0);
    
    new_malloc(ptr, sz, file, line);
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
        // Adding to free count
        size_t ptr_pos = (size_t) ptr;
        auto it1 = active_allocations.find(ptr_pos);
        auto it2 = freed_allocations.find(ptr_pos);

        // Check if allocation actually exists - if not raise some errors
        if (ptr_pos > my_data.largest_bytes_location 
        || ptr_pos < my_data.smallest_bytes_location) {
            fprintf(stderr, 
                    "MEMORY BUG: %s:%d: invalid free of pointer %p, not in heap\n", 
                    file, line, ptr);
            abort();             
        }
        if (it2 != freed_allocations.end()) {
            fprintf(stderr, 
                    "MEMORY BUG: %s:%d: invalid free of pointer %p, double free\n", 
                    file, line, ptr);
            abort();    
        }
        if (it1 == active_allocations.end()) {
            fprintf(stderr, 
                    "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", 
                    file, line, ptr);

            for (auto p = active_allocations.begin();
            p != active_allocations.end();
            p++) {
                if (ptr_pos > p->first && ptr_pos < p->first + p->second) {
                    auto it = allocation_info.find(p->first);
                    fprintf(stderr, 
                            "  %s:%d: %p is %zu bytes inside a %zu byte region allocated here\n", 
                            it->second.file, it->second.line, ptr, ptr_pos - p->first, p->second - SIZECONST);
                    abort();
                }
            }
            abort();
        }
        // Testing is checking to see if my special character is still at the end of the malloc
        // If not then it was a wild write
        auto testing = (char*) (it1->first + it1->second - SIZECONST);
        if (*testing != '#') {
             printf("%p does not have #. Instead it has %i\n", testing, *(testing));
             fprintf(stderr, 
                    "MEMORY BUG: %s:%d: detected wild write during free of pointer %p\n", 
                    file, line, ptr);
             abort();           
        }
        assert((ptr_pos + add_padding(ptr_pos)) % 16 == 0);

        // Update statistics
        freed_allocations.insert({ptr_pos + add_padding(ptr_pos), 
                                    it1->second + add_padding(it1->second)});
        my_data.n_frees ++;
        my_data.freed_bytes += it1->second - SIZECONST;

        // Coalesce
        auto it = freed_allocations.find(ptr_pos + add_padding(ptr_pos));
        while (can_coalesce_down(it)) {
            --it;
        }
        while (can_coalesce_up(it)) {
            coalesce_up(it);
        }
        active_allocations.erase(ptr_pos);
        allocation_info.erase(ptr_pos);
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
    if (SIZE_MAX / sz < count) {
        my_data.n_fails++;
        my_data.failed_bytes += sz;
        return nullptr;
    }
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
    stats.active_size = my_data.allocation_bytes - my_data.freed_bytes;
    stats.nfail = my_data.n_fails;
    stats.fail_size = my_data.failed_bytes;
    stats.heap_max = my_data.largest_bytes_location;
    stats.heap_min = my_data.smallest_bytes_location;

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
    for (auto p = active_allocations.begin();
    p != active_allocations.end();
    p++) {
        auto it = allocation_info.find(p->first);
        printf("LEAK CHECK: %s:%d: allocated object %p with size %zu\n", 
                it->second.file, it->second.line, (void*) p->first, p->second - SIZECONST);
    }
}
