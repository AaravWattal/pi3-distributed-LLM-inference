#include "uart.h"
#include "rpi.h"
#include "fat32.h"
#include "pi-sd.h"
#include "mbr.h"
#include "libc/demand.h"

void notmain(void) {
    printk("Initializing SD card...\r\n");

    if (pi_sd_init() != 1) {
        panic("SD card init failed\r\n");
    }

    mbr_t *mbr = mbr_read();
    mbr_partition_ent_t partition = mbr_get_partition(mbr, 0);

    fat32_fs_t fs = fat32_mk(&partition);
    pi_dirent_t root = fat32_get_root(&fs);

    printk("Root directory:\r\n");
    pi_directory_t dir = fat32_readdir(&fs, &root);

    for (unsigned i = 0; i < dir.ndirents; i++) {
        pi_dirent_t *e = &dir.dirents[i];
        if (e->is_dir_p) {
            printk("  [DIR]  %s\r\n", e->name);
        } else {
            printk("  [FILE] %s (%u bytes)\r\n", e->name, e->nbytes);
        }
    }

    printk("Done. %u entries.\r\n", dir.ndirents);
}
