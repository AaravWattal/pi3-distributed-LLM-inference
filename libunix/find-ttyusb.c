// engler, cs140e: your code to find the tty-usb device on your laptop.
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "libunix.h"

#define _SVID_SOURCE
#include <dirent.h>
static const char *ttyusb_prefixes[] = {
    "ttyUSB",	// linux
    "ttyACM",   // linux
    "cu.SLAB_USB", // mac os
    "cu.usbserial", // mac os
    // if your system uses another name, add it.
	0
};

static int filter(const struct dirent *d) {
    // scan through the prefixes, returning 1 when you find a match.
    // 0 if there is no match.
    
    for (int i = 0; ttyusb_prefixes[i] != 0; i++) {
        if (prefix_cmp(d->d_name, ttyusb_prefixes[i])) {
            return 1;
        }
    }

    return 0;
}

// find the TTY-usb device (if any) by using <scandir> to search for
// a device with a prefix given by <ttyusb_prefixes> in /dev
// returns:
//  - device name.
// error: panic's if 0 or more than 1 devices.
char *find_ttyusb(void) {
    // use <alphasort> in <scandir>
    // return a malloc'd name so doesn't corrupt.
    
    struct dirent **namelist;

    int n = scandir("/dev", &namelist, filter, alphasort);

    if (n < 0) {
        panic("scandir error");
    }

    if (n == 0) {
        panic("no device found");
    }

    if (n > 1) {
        panic("more than one device found");
    }

    char *device_name = strdupf("/dev/%s", namelist[0]->d_name);

    for (int i = 0; i < n; i++) {
        free(namelist[i]);
    }
    free(namelist);

    return device_name;
}

// return the most recently mounted ttyusb (the one
// mounted last).  use the modification time 
// returned by state.
char *find_ttyusb_last(void) {
    struct dirent **namelist;

    int n = scandir("/dev", &namelist, filter, alphasort);

    if (n < 0) {
        panic("scandir error");
    }

    if (n == 0) {
        panic("no device found");
    }

    char *selected_device = NULL;
    time_t latest_mod_time = 0;

    for (int i = 0; i < n; i++) {
        char *path = strdupf("/dev/%s", namelist[i]->d_name);

        struct stat s;

        // Check if stat failed
        if (stat(path, &s) < 0) {
            sys_die(stat, "could not stat file: %s", path);
        }

        if (selected_device == NULL || s.st_mtime > latest_mod_time) {
            if (selected_device != NULL) {
                free(selected_device);
            }

            selected_device = path;
            latest_mod_time = s.st_mtime;
        } else {
            free(path);
        }
    }

    for (int i = 0; i < n; i++) {
        free(namelist[i]);
    }
    free(namelist);

    if (selected_device == NULL) {
        panic("no device found");
    }

    return selected_device;
}

// return the oldest mounted ttyusb (the one mounted
// "first") --- use the modification returned by
// stat()
char *find_ttyusb_first(void) {
    struct dirent **namelist;

    int n = scandir("/dev", &namelist, filter, alphasort);

    if (n < 0) {
        panic("scandir error");
    }

    if (n == 0) {
        panic("no device found");
    }

    char *selected_device = NULL;
    time_t latest_mod_time = 0;

    for (int i = 0; i < n; i++) {
        char *path = strdupf("/dev/%s", namelist[i]->d_name);

        struct stat s;

        // Check if stat failed
        if (stat(path, &s) < 0) {
            sys_die(stat, "could not stat file: %s", path);
        }

        if (selected_device == NULL || s.st_mtime < latest_mod_time) {
            if (selected_device != NULL) {
                free(selected_device);
            }

            selected_device = path;
            latest_mod_time = s.st_mtime;
        } else {
            free(path);
        }
    }

    for (int i = 0; i < n; i++) {
        free(namelist[i]);
    }
    free(namelist);

    if (selected_device == NULL) {
        panic("no device found");
    }

    return selected_device;
}
