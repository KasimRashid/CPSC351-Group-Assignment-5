#ifndef DISK_H
#define DISK_H

int make_disk(const char *name);
int open_disk(const char *name);
int close_disk();
int block_read(int block, void *buf);
int block_write(int block, const void *buf);

#endif