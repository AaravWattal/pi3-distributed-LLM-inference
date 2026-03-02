#ifndef __LIBUNIX_H__
#define __LIBUNIX_H__

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

// roundup <x> to a multiple of <n>: taken from the lcc compiler.
#define pi_roundup(x,n) (((x)+((n)-1))&(~((n)-1)))

#include "demand.h"

// read and echo the characters from the pi's usbtty to unix
// and from unix to pi until the ttyusb disappears (the hardware got
// pulled out) or we see a <DONE!!!> string, used to indicate 
// a clean shutdown.
void pi_cat(int pi_fd, const char *portname);

// hack-y state machine to indicate when we've seen the special string
// 'DONE!!!' from the pi telling us to shutdown.  pass in the null
// terminated string read from the pi.
int pi_done(unsigned char *s);

// overwrite any unprintable characters with a space.
// otherwise terminals can go haywire/bizarro.
// note, the string can contain 0's, so we send the
// size.
void remove_nonprint(uint8_t *buf, int n);

// opens the ttyusb <device> and returns file descriptor.
int open_tty(const char *device);
int open_tty_n(const char *device, int maxattempts);

// used to set a tty to the 8n1 protocol needed by the tty-serial.
int set_tty_to_8n1(int fd, unsigned speed, double timeout);

// returns 1 if the tty is not there, 0 otherwise.
// used to detect when the user pulls the tty-serial device out.
int tty_gone(const char *ttyname);

// fill in <fmt,..> using <...> and strcat it to <dst>
char *strcatf(char *dst, const char *fmt, ...);

// same but replace <dst>'s contents.
char *strcpyf(char *dst, const char *fmt, ...);

// print the format string into a buffer and return 
// an allocated copy of it.
char *strdupf(const char *fmt, ...);
// print the format string into a buffer and return an
// allocated copy of it 
char *vstrdupf(const char *fmt, va_list ap);
// concat src1 with the result of fmt... and return strdup'd result
char *str2dupf(const char *src1, const char *fmt, ...);

// write exactly <n> bytes: panics if short write.
int write_exact(int fd, const void *data, unsigned n);
// read exactly <n> bytes: panics if short read.
int read_exact(int fd, void *data, unsigned n);

void put_uint8(int fd, uint8_t b);
void put_uint32(int fd, uint32_t u);
uint8_t get_uint8(int fd);
uint32_t get_uint32(int fd);

int suffix_cmp(const char *s, const char *suffix);
int prefix_cmp(const char *s, const char *prefix);

void pi_echo(int unix_fd, int pi_fd, const char *portname);

int exists(const char *name);

// looks in /dev for a ttyusb device. 
// returns:
//  - device name.
// panic's if 0 or more than 1.
char *find_ttyusb(void);
char *find_ttyusb_first(void);
char *find_ttyusb_last(void);

// read in file <name>
// returns:
//  - pointer to the code.  pad code with 0s up to the
//    next multiple of 4.  
//  - bytes of code in <size>
//
// fatal error open/read of <name> fails.
void *read_file(unsigned *size, const char *name);

// waits for <usec>
int can_read_timeout(int fd, unsigned usec);
// doesn't block.
int can_read(int fd);

int read_timeout(int fd, void *data, unsigned n, unsigned timeout);

// print argv style string.
void argv_print(const char *msg, char *argv[]);

// non-blocking check if <pid> exited cleanly.
// returns:
//   - 0 if not exited;
//   - 1 if exited cleanly (exitcode in <status>, 
//   - -1 if exited with a crash (status holds reason)
int child_clean_exit_noblk(int pid, int *status);
// blocking version.
int child_clean_exit(int pid, int *status);

// given a fd in the current process <our_fd>,
// fork/exec <argv> and dup it to <child_fd>
void handoff_to(int our_fd, int child_fd, char *argv[]);

#endif /* __LIBUNIX_H__ */
