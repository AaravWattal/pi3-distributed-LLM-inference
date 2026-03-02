#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "libunix.h"

// allocate buffer, read entire file into it, return it.   
// buffer is zero padded to a multiple of 4.
//
//  - <size> = exact nbytes of file.
//  - for allocation: round up allocated size to 4-byte multiple, pad
//    buffer with 0s. 
//
// fatal error: open/read of <name> fails.
//   - make sure to check all system calls for errors.
//   - make sure to close the file descriptor (this will
//     matter for later labs).
// 
void *read_file(unsigned *size, const char *name) {
    // How: 
    //    - use stat() to get the size of the file.
    //    - round up to a multiple of 4.
    //    - allocate a buffer
    //    - zero pads to a multiple of 4.
    //    - read entire file into buffer (read_exact())
    //    - fclose() the file descriptor
    //    - make sure any padding bytes have zeros.
    //    - return it.   
    
    struct stat s;

    if (stat(name, &s) < 0) {
        sys_die(stat, "could not stat file: %s", name);
    }

    // Set file size
    *size = s.st_size;

    // Allocate buffer
    unsigned alloc_size = pi_roundup(s.st_size, 4);

    void *buf = calloc(1, alloc_size);

    if (!buf) {
        panic("could not allocate bytes for file: %s", name);
    }

    // Open file
    int fd = open(name, O_RDONLY);

    if (fd < 0) {
        sys_die(open, "could not open file: %s", name);
    }

    // Read file into buffer
    if (s.st_size > 0) {
        read_exact(fd, buf, s.st_size);
    }

    close(fd);

    return buf;
}
