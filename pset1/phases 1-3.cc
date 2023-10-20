#include "m61.hh"
#include <cstdlib>
#include <iostream>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <algorithm>
#include <map>
#include <set>
#include <typeinfo>

using namespace std;

m61_statistics global_stats; //variable that tracks stats
const int ALIGNMENT = alignof(std::max_align_t); 
const int MARKER = '!'; 
char* curr_file; 

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

void* itop(size_t x) { return &default_buffer.buffer[x]; }

size_t ptoi(void* x) { return (uintptr_t) x - (uintptr_t) itop(0); }

struct metadata
{
    size_t sz; 
    size_t alignment_sz; 
    size_t line_allocated; 
};

metadata make_metadata(size_t sz, size_t alignment_sz, size_t line_allocated)
{
    metadata temp = {sz, alignment_sz, line_allocated}; 
    return temp; 
}

map<size_t, metadata> allocated_blocks; //stores allocations as (key: starting position; value: {total size, alignment buffer size})
map<size_t, size_t> free_blocks = {{0, default_buffer.size}}; //stores free space as (key: ptr to starting position; value: block size)
set<void*> freed; //tracks already freed pointers

/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.
void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // printf("%d %p %d; ", default_buffer.pos, itop(default_buffer.pos), default_buffer.size);

    curr_file = (char*) file;  

    long long block_start = -1; 
    size_t alignment_sz = 0; //memory allocated including buffer
    for (auto block_start_it = free_blocks.begin(); block_start_it != free_blocks.end(); block_start_it++)
    {
        //check for diabolical sz
        if (sz >= default_buffer.size) break; 

        long long curr_pos = block_start_it->first; 
        if (block_start_it->second >= sz + (!(sz%ALIGNMENT))*ALIGNMENT)
        {
            block_start = curr_pos; 
            long long block_end = curr_pos + block_start_it->second; //exclusive
            long long new_start = curr_pos + sz; 

            //ensure starting position satisfies alignment and calculates buffer needed to insert at back of allocated block
            alignment_sz = (ALIGNMENT - new_start % ALIGNMENT) % ALIGNMENT; 
            alignment_sz += (!(sz%ALIGNMENT))*ALIGNMENT; //extra padding IF alignment_sz = 0
            memset(itop(new_start), MARKER, alignment_sz); //marker for boundary write error
            new_start += alignment_sz; 

            //update free blocks
            free_blocks.erase(block_start_it); 
            if (new_start < block_end) free_blocks.insert({new_start, block_end-new_start}); 
            break; 
        }
    }

    if (block_start == -1)
    {
        //not enough space; update failed stats
        global_stats.nfail++; 
        global_stats.fail_size += sz; 
        return nullptr;
    }

    // Otherwise there is enough space; claim the next `sz` bytes
    void* ptr = itop(block_start);
    allocated_blocks.insert({block_start, make_metadata(sz, alignment_sz, line)}); 

    //delete from list of freed points
    if (freed.find(ptr) != freed.end()) freed.erase(freed.find(ptr)); 

    //Update relevant stats
    global_stats.nactive++; 
    global_stats.ntotal++; 
    global_stats.total_size += sz; 
    global_stats.active_size += sz; 
    if (global_stats.ntotal == 1)
    {
        global_stats.heap_min = (uintptr_t) ptr; 
        global_stats.heap_max = ((uintptr_t) ptr)+sz; 
    }else
    {
        global_stats.heap_min = min(global_stats.heap_min, (uintptr_t) ptr); 
        global_stats.heap_max = max(global_stats.heap_max, ((uintptr_t) ptr)+sz); 
    }
    // cerr << ((uintptr_t) ptr) - ((uintptr_t) itop(0)) << "; "; 
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
    if (ptr == nullptr) return; 

    //free up memory if it's a valid call
    auto allocated_it = allocated_blocks.find(ptoi(ptr)); 
    if (allocated_it == allocated_blocks.end())
    {
        if (freed.find(ptr) != freed.end())
        {
            //already freed ptr
            cerr << "MEMORY BUG: " << file << ":" << line << ": invalid free of pointer " << ptr << ", double free\n"; 
        }else if ((char*) ptr - (char*) itop(0) >= (long long) default_buffer.size || (char*) ptr - (char*) itop(0) < 0)
        {
            //not in heap
            cerr << "MEMORY BUG: " << file << ":" << line << ": invalid free of pointer " << ptr << ", not in heap\n"; 
        }else
        {
            // cerr << "here\n"; 
            //not allocated
            cerr << "MEMORY BUG: " << file << ":" << line << ": invalid free of pointer " << ptr << ", not allocated\n"; 
            
            // checks if inside an allocated region
            auto it = allocated_blocks.upper_bound(ptoi(ptr)); 
            it--; 
            // cerr << ptoi(ptr) << " " << it->first << " " << it->first + it->second.sz << "\n"; 
            if (it->first < ptoi(ptr) && it->first + it->second.sz > ptoi(ptr))
            {
                cerr << file << ":" << it->second.line_allocated << ": " << ptr << " is " << ptoi(ptr)-it->first << " bytes inside a " << it->second.sz << " byte region allocated here\n"; 
            }
        }
        abort(); 
    }
    size_t start_pos = allocated_it->first; 
    size_t block_sz = allocated_it->second.sz; 
    size_t alignment_sz = allocated_it->second.alignment_sz; 

    //add it to the list of freed pointers
    freed.insert(ptr); 

    //check for out-of-bounds write error
    for (size_t i = block_sz; i < block_sz + alignment_sz; i++)
    {
        // cerr << *((char*) ptr + i) << " "; 
        if (*((char*) ptr + i) != MARKER)
        {
            //boundary write error
            cerr << "MEMORY BUG: " << file << ":" << line << ": detected wild write during free of pointer " << ptr << "\n"; 
            abort(); 
        }
    }

    //update stats
    global_stats.active_size -= block_sz; 
    global_stats.nactive--; 
    
    //free up memory
    allocated_blocks.erase(allocated_it); 
    free_blocks.insert({start_pos, block_sz + alignment_sz}); 

    //coalesce memory if needed
    auto curr_it = free_blocks.find(start_pos); 
    auto m_ptr = curr_it; 
    auto r_ptr = (++curr_it); 
    curr_it--; curr_it--; 
    if (r_ptr != free_blocks.end())
    {
        size_t dist = (r_ptr->first) - (m_ptr->first); 
        if (m_ptr->second == dist)
        {
            m_ptr->second += r_ptr->second; 
            free_blocks.erase(r_ptr); 
        }
    }
    if (m_ptr != free_blocks.begin())
    {
        auto l_ptr = curr_it; 
        size_t dist = (m_ptr->first) - (l_ptr->first); 
        if (l_ptr->second == dist)
        {
            l_ptr->second += m_ptr->second; 
            free_blocks.erase(m_ptr); 
        }
    }
}

///    m61_calloc(count, sz, file, line)
///    Returns a pointer a fresh dynamic memory allocation big enough to
///    hold an array of `count` elements of `sz` bytes each. Returned
///    memory is initialized to zero. The allocation request was at
///    location `file`:`line`. Returns `nullptr` if out of memory; may
///    also return `nullptr` if `count == 0` or `size == 0`.
void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    //avoids integer overflow if (sz + count) is too big
    if (count > default_buffer.size || sz > default_buffer.size)
    {
        global_stats.nfail++; 
        global_stats.fail_size += (count * sz); 
        return nullptr; 
    }

    void* ptr = m61_malloc(count * sz, file, line);
    if (ptr) {
        //there is enough space for malloc to allocate memory
        memset(ptr, 0, count * sz);
    }
    return ptr;
}

/// m61_get_statistics()
///    Return the current memory statistics.
m61_statistics m61_get_statistics() {
    return global_stats;
}

/// m61_print_statistics()
/// Prints the current memory statistics.
void m61_print_statistics() {
    m61_statistics stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

/// m61_print_leak_report()
/// Prints a report of all currently-active allocated blocks of dynamic memory.
void m61_print_leak_report() {
    for (auto block : allocated_blocks)
    {
        cout << "LEAK CHECK: " << curr_file << ":" << block.second.line_allocated << ": allocated object " << itop(block.first) << " with size " << block.second.sz << "\n"; 
    }
}