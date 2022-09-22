#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <vector>
// Check for memory reuse: at most 5 active allocations.

int main() {
    for (int i = 0; i != 10000; ++i) {
        void* ptrs[5];
        for (int j = 0; j != 5; ++j) {
            ptrs[j] = m61_malloc(1000);
            //printf("%p is going to be free, %i\n", (void*)((size_t)ptrs[j] + 1000), i);
            assert(ptrs[j]);
        }
        for (int j = 5; j != 0; --j) {
            //printf("%p is going to be free, %i\n", (void*)((size_t)ptrs[j-1] + 1000), i);
            m61_free(ptrs[j - 1]);
        }
    }
    m61_print_statistics();
}

//! alloc count: active          0   total      50000   fail          0
//! alloc size:  active        ???   total   50000000   fail          0
