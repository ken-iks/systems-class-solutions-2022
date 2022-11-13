#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.cc
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;     // file descriptor

    static constexpr off_t bufsize = 4096;
    unsigned char cbuf[bufsize];
    int mode;

    off_t tag;      // file offset of first byte in cache (0 when file is opened)
    off_t end_tag;  // file offset one past last valid byte in cache
    off_t pos_tag;  // file offset of next char to read in cache
};

int io61_fill(io61_file* f) {
    // Fill the read cache with new data, starting from file offset `end_tag`.
    // Only called for read caches.

    // Check invariants
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }

    // Reset the cache to empty.
    f->tag = f->pos_tag = f->end_tag;
    // Read data.
    ssize_t n = read(f->fd, f->cbuf, f->bufsize);
    if (n >= 0) {
        f->end_tag = f->tag + n;
    } 
    else {
        return -1;
    }

    // Recheck invariants (good practice!).
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }

    return 0;
}

// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }
    unsigned char buf[1];
    if (f->pos_tag == f->end_tag) {
        int r = io61_fill(f);
        if (r < 0) {
            return -1;
        }
        if (f->pos_tag == f->end_tag) {
            return -1;
        }
    }
    buf[0] = f->cbuf[f->pos_tag - f->tag];
    f->pos_tag++;
    return buf[0];
}


// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”


ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    // Check invariants.
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }

    size_t pos = 0;
    while (pos < sz) {
        if (f->pos_tag == f->end_tag) {
            int r = io61_fill(f);
            if (r < 0) {
                return -1;
            }
            if (f->pos_tag == f->end_tag) {
                break;
            }
        }
        size_t ch = sz - pos;
        if (ch > (size_t) (f->end_tag - f->pos_tag)) {
            ch = (size_t) (f->end_tag - f->pos_tag);
        }
        memcpy(&buf[pos], &f->cbuf[f->pos_tag - f->tag], ch);
        f->pos_tag += ch;
        pos += ch;
    }
    return pos;
}

// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success and
//    -1 on error.


int io61_writec(io61_file* f, int ch) {
    unsigned char buf[1];
    buf[0] = ch;
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }
    // Write cache invariant.
    if (f->pos_tag != f->end_tag) {
        return -1;
    }

    if (f->end_tag == f->tag + f->bufsize) {
        int r = io61_flush(f);
        if (r < 0){
            return -1;
        }
    }

    f->cbuf[f->pos_tag - f->tag] = buf[0];
    f->pos_tag++;
    f->end_tag++;
    return 0;
}

// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.


ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    // Check invariants.
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }
    // Write cache invariant.
    if (f->pos_tag != f->end_tag) {
        return -1;
    }

    size_t pos = 0;
    while (pos < sz) {
        if (f->end_tag == f->tag + f->bufsize) {
            int r = io61_flush(f);
            if (r < 0){
                return -1;
            }
        }

        size_t ch = sz - pos;
        if (ch > (size_t) (f->bufsize + f->tag - f->pos_tag)){
            ch = (size_t) (f->bufsize + f->tag - f->pos_tag);
        }
        memcpy(&f->cbuf[f->pos_tag - f->tag], &buf[pos], ch);
        f->pos_tag += ch;
        f->end_tag += ch;
        pos += ch;
    }
    return pos;
}

// io61_flush(f)
//    Forces a write of any cached data written to `f`. Returns 0 on
//    success. Returns -1 if an error is encountered before all cached
//    data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. If may also
//    drop any data cached for reading.


int io61_flush(io61_file* f) {
    // Check invariants.
    if (!(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag) ||
       (f->end_tag - f->pos_tag > f->bufsize)) {
        return -1;
    }
    // Write cache invariant.
    if (f->pos_tag != f->end_tag) {
        return -1;
    }

    ssize_t n = write(f->fd, f->cbuf, f->pos_tag - f->tag);
    if (n < 0) {
        return -1;
    }
    f->tag = f->pos_tag;
    return 0;
}

// io61_seek(f, pos)
//    Changes the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.
 
int io61_seek(io61_file* f, off_t pos) {

    if (pos <= f->end_tag && pos >= f->tag) {
        f->pos_tag = pos;
        return 0;
    }

    if (f->mode == O_RDONLY){
        off_t r = (off_t) pos - (pos % f->bufsize);
        off_t new_seek = lseek(f->fd, r, SEEK_SET);
        if (new_seek == r) {
            if (io61_fill(f) < 0) {
                return -1;
            }
            f->pos_tag = pos;
            f->tag = new_seek;
            f->end_tag = new_seek + f->bufsize;
            return 0;
        }
    }

    if (f->mode == O_WRONLY) {
        if (io61_flush(f) < 0) {
            return -1;
        }
        off_t new_seek = lseek(f->fd, pos, SEEK_SET);
        if (new_seek == pos) {
            f->tag = (off_t) pos - (pos % f->bufsize);
            f->pos_tag = pos;
            f->end_tag = new_seek + (new_seek % f->bufsize);
            return 0;
        }
    }
    return -1;
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
