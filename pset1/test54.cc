#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
// Check detection of diabolical wild free.

int main() {
    const char* file = __FILE__; 
    int line = __LINE__; 

    void* ptr1 = m61_malloc(20, file, line); 
    if (ptr1 == nullptr) return 1; 

    // check realloc to smaller sizes: success
    void* ptr2 = m61_realloc(ptr1, 10, file, line); 
    assert(ptr1 == ptr2); 

    // check nullptr
    void* ptr3 = m61_realloc(nullptr, 10, file, line); 
    assert(ptr3); 

    // check memory bug detection
    (void) m61_realloc((char*) ptr1+1, 10, file, line); // not allocated
    m61_free(ptr1); 
    (void) m61_realloc(ptr1, 10, file, line); // already freed

    ptr1 = m61_malloc(20, file, line); 

    // check realloc to bigger sizes
    void* ptr5 = m61_realloc(ptr1, 30, file, line); 
    assert(ptr5); 

    // check realloc to diabolical sizes
    void* ptr6 = m61_realloc(ptr5, -1, file, line); 
    assert(ptr6 == nullptr); 
    return 0; 
}

//! MEMORY BUG???: invalid realloc of pointer ???, not allocated
//! MEMORY BUG???: invalid realloc of pointer ???, already freed
