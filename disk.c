#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "fs.h"

static int disk_fd = -1;

int make_disk(const char *name) {
    if (disk_fd != -1) return -1;
    disk_fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (disk_fd == -1) return -1;
    
    // Initialize disk with zeros
    char block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (write(disk_fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
            close(disk_fd);
            disk_fd = -1;
            return -1;
        }
    }
    
    close(disk_fd);
    disk_fd = -1;
    return 0;
}

int open_disk(const char *name) {
    if (disk_fd != -1) return -1;
    disk_fd = open(name, O_RDWR);
    return disk_fd == -1 ? -1 : 0;
}

int close_disk() {
    if (disk_fd == -1) return -1;
    int ret = close(disk_fd);
    disk_fd = -1;
    return ret;
}

int block_read(int block, void *buf) {
    if (disk_fd == -1 || block < 0 || block >= NUM_BLOCKS) return -1;
    if (lseek(disk_fd, block * BLOCK_SIZE, SEEK_SET) == -1) return -1;
    return read(disk_fd, buf, BLOCK_SIZE) == BLOCK_SIZE ? 0 : -1;
}

int block_write(int block, const void *buf) {
    if (disk_fd == -1 || block < 0 || block >= NUM_BLOCKS) return -1;
    if (lseek(disk_fd, block * BLOCK_SIZE, SEEK_SET) == -1) return -1;
    return write(disk_fd, buf, BLOCK_SIZE) == BLOCK_SIZE ? 0 : -1;
}
