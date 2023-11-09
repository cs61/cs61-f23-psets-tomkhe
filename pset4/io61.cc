#include "io61.hh" 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <climits> 
#include <cerrno> 
#include <sys/mman.h> 
#include <fcntl.h>
#include <algorithm>
#include <list>
#include <unordered_map>
#include <map>
#include <stack>

// io61.cc
//    YOUR CODE HERE!

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    static const off_t bufsize = 1 << 12; 

    int fd = -1;                // file descriptor
    int mode;                   // open mode (O_RDONLY or O_WRONLY)
    char buf[bufsize];          // fully associative buffer
    off_t size;                 // file size
    char* map;                  // memory-mapped IO
    bool is_seq = true;         // if access pattern is sequential

    off_t tag = 0;              // file offset of first byte of cache data (0 when file opened)
    off_t pos_tag = 0;          // file offset of cache (in read, this is the file offset of next char to be read)
    off_t end_tag = 0;          // file offset one past last byte of cached data (0 when file opened)
};

// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    f->size = io61_filesize(f); 
    if (f->mode == O_RDONLY)
    {
        f->map = (char*) mmap(nullptr, f->size, PROT_READ | PROT_WRITE, MAP_PRIVATE, f->fd, 0); 
        if (f->map != MAP_FAILED)
        {
            f->tag = 0; 
            f->end_tag = f->size; 
            posix_fadvise(f->fd, 0, f->size, POSIX_FADV_SEQUENTIAL);
        }
    }
    return f;
}

// io61_close(f)
// Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    if (f->map != MAP_FAILED) munmap(f->map, f->size); 
    delete f;
    return r;
}

int io61_fill(io61_file* f)
{
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);

    // Reset the cache to empty.
    f->tag = f->end_tag; 
    f->pos_tag = f->end_tag; 

    ssize_t nread = read(f->fd, f->buf, f->bufsize); 
    if (nread != -1) f->end_tag = f->tag + nread; 
    else return -1; 

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);
    return 0; 
}

// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);

    if (f->map != MAP_FAILED)
    {
        // mappable file
        if (f->pos_tag < f->end_tag)
        {
            f->pos_tag++; 
            assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
            assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 
            return (unsigned char) f->map[f->pos_tag-1]; 
        }else return -1; 
    }

    // non-mappable file
    if (f->pos_tag == f->end_tag)
    {
        if(io61_fill(f) < 0 || f->pos_tag == f->end_tag) return -1; 
    }

    unsigned char ch = f->buf[f->pos_tag - f->tag]; 
    f->pos_tag++; 
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 
    return ch; 
}

// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a Ã¢â‚¬Å“short read.Ã¢â‚¬Â

ssize_t io61_read_cache(io61_file* f, unsigned char* buf, size_t sz) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 

    size_t nread = 0; 
    while (nread < sz)
    {
        if (f->pos_tag == f->end_tag)
        {
            //buffer refill
            if (io61_fill(f) == -1 || f->pos_tag == f->end_tag) break; 
        }
        size_t curr_read = std::min((size_t) (f->end_tag - f->pos_tag), (size_t) (sz - nread)); 
        memcpy(&buf[nread], &f->buf[f->pos_tag - f->tag], curr_read); 
        f->pos_tag += curr_read; 
        nread += curr_read; 
    }

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);
    if (nread != 0 || sz == 0 || errno == 0) {
        return nread;
    } else {
        return -1;
    }
}

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);

    if (f->map == MAP_FAILED) return io61_read_cache(f, buf, sz); 

    // Copy `sz` number of bytes from `f` into `buf`
    size_t nread = std::min(sz, (size_t) (f->size - f->pos_tag)); 
    memcpy(buf, &f->map[f->pos_tag], nread); 
    f->pos_tag += nread; 

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);
    return nread; 
}


// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.

int io61_writec(io61_file* f, int c) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 

    if (f->end_tag == f->tag + f->bufsize)
    {
        if (io61_flush(f) < 0) return -1; 
    }

    f->buf[f->pos_tag - f->tag] = c; 
    f->pos_tag++; 
    f->end_tag++; 

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 
    return 0; 
}

// io61_flush(f)
//    If `f` was opened write-only, `io61_flush(f)` forces a write of any
//    cached data written to `f`. Returns 0 on success; returns -1 if an error
//    is encountered before all cached data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. It may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {
    // check invariants
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 

    if (f->mode == O_RDONLY) return 0; 

    // if clean, 
    if (f->pos_tag - f->tag == 0) return 0; 

    ssize_t nwrite = 0; 
    do
    {
        ssize_t curr_write = write(f->fd, &f->buf[nwrite], f->pos_tag - f->tag - nwrite); 
        nwrite += std::max(curr_write, (ssize_t) 0); 
    } while (nwrite < f->pos_tag - f->tag && (errno == EAGAIN || errno == EINTR)); 

    // flush failed
    if (nwrite != f->pos_tag - f->tag) return -1; 
    f->tag = f->pos_tag; 
    return 0; 
}


// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);

    size_t nwritten = 0; 
    while (nwritten < sz)
    {
        if (f->end_tag == f->tag + f->bufsize)
        {
            // flush buffer
            if (io61_flush(f) == -1) break; 
        }

        size_t curr_write = std::min((size_t) (f->bufsize + f->tag - f->pos_tag), (size_t) (sz-nwritten)); 
        if (curr_write == 0) break; 
        memcpy(&f->buf[f->pos_tag - f->tag], &buf[nwritten], curr_write); 
        nwritten += curr_write; 
        f->pos_tag += curr_write; 
        f->end_tag += curr_write; 
    }

    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size);
    if (nwritten != 0 || sz == 0) {
        return nwritten;
    } else {
        return -1;
    }
}


// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t off) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= f->bufsize || f->end_tag - f->pos_tag <= f->size); 
    if (f->mode == O_RDONLY && f->tag <= off && f->end_tag > off)
    {
        f->pos_tag = off; 
        return 0; 
    }

    if (f->mode == O_WRONLY)
    {
        if (io61_flush(f) < 0) return -1; 
    }

    // update kernal and IO position
    if (lseek(f->fd, (off_t) off, SEEK_SET) == -1) return -1; 
    f->pos_tag = off; 

    if (f->mode == O_WRONLY)
    {
        // don't consider mapping if write only
        f->tag = f->end_tag = off; 
        return 0; 
    }

    if (f->map == MAP_FAILED)
    {
        // file not mappable
        f->tag = f->end_tag = off; 
        return 0; 
    }
    
    if (f->is_seq) posix_fadvise(f->fd, 0, f->size, POSIX_FADV_NORMAL);
    f->is_seq = false; 
    return 0; 
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}