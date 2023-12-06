CS 61 Problem Set 3
===================

**Fill out both this file and `AUTHORS.md` before submitting.** We grade
anonymously, so put all personally identifying information, including
collaborators, in `AUTHORS.md`.

Grading notes (if any)
----------------------



Extra credit attempted (if any)
-------------------------------
- Copy-on-write fork
- `kill` system call
- `sleep` system call

`p-sleepkill` test file: parent forks a child, sleeps for a bit, and then kills the child once it runs out of memory. It can be accessed by pressing the `s` key after `make run`. 

**Additional extra credit in this commit:**
- `mmap` system call: extended version of `sys_page_alloc` which supports multi-page allocations and page protections for newly allocated pages
- `munmap` system call: removes mapping for a given region
- replaced implementation of `sys_page_alloc` with `sys_mmap`

`p-map` test file: parent forks 3 children, then each process iteratively calls `sys_mmap(nullptr, PAGESIZE*rand_factor, PTE_PWU)` to get page mappings in chunks (of pseudo-random size). `sys_mmap(heap_top, PAGESIZE, PTE_PWU) == nullptr)` is also called to test `mmap` starting at specified address. After they run of memory, the processes call `sys_munmap(heap_bottom, heap_top - heap_bottom)` to unmap all heap memory, then do nothing forever. It can be run by pressing the `m` key. 