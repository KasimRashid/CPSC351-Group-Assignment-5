#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define BLOCK_SIZE 4096
#define NUM_BLOCKS 8192
#define DATA_BLOCKS 4096
#define MAX_FILES 64
#define MAX_FILE_NAME 15
#define MAX_FD 32
#define MAGIC_NUMBER 0x46534653 // "FSFS" in hex

// File system structures
typedef struct {
    uint32_t magic;
    uint32_t fat_start;
    uint32_t fat_blocks;
    uint32_t dir_start;
    uint32_t dir_blocks;
    uint32_t data_start;
    uint32_t free_blocks;
    uint32_t created;
    uint32_t last_mounted;
} superblock_t;

typedef struct {
    char name[MAX_FILE_NAME + 1];
    uint32_t size;
    uint32_t first_block;
    uint32_t created;
    uint32_t modified;
    uint8_t used;
} dir_entry_t;

typedef struct {
    int32_t dir_index;
    uint32_t offset;
    uint8_t used;
} file_desc_t;

// File system API
int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);

// File operations
int fs_open(char *name);
int fs_close(int fildes);
int fs_create(char *name);
int fs_delete(char *name);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

#endif // FS_H