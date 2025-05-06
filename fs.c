#include "fs.h"
#include "disk.h"
#include <string.h>
#include <time.h>

// Global variables
static int disk_fd = -1;
static superblock_t sb;
static uint32_t *fat = NULL;
static dir_entry_t *dir = NULL;
static file_desc_t fd_table[MAX_FD];

// Helper functions
static int find_free_fd() {
    for (int i = 0; i < MAX_FD; i++) {
        if (!fd_table[i].used) return i;
    }
    return -1;
}

static int find_file(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (dir[i].used && strcmp(dir[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_free_dir_entry() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!dir[i].used) return i;
    }
    return -1;
}

static uint32_t find_free_block() {
    for (uint32_t i = 0; i < DATA_BLOCKS; i++) {
        if (fat[i] == 0) return i;
    }
    return (uint32_t)-1;
}

static int write_superblock() {
    char block[BLOCK_SIZE];
    memcpy(block, &sb, sizeof(superblock_t));
    return block_write(0, block);
}

static int read_superblock() {
    char block[BLOCK_SIZE];
    if (block_read(0, block) == -1) return -1;
    memcpy(&sb, block, sizeof(superblock_t));
    return 0;
}

static int write_fat() {
    for (uint32_t i = 0; i < sb.fat_blocks; i++) {
        char block[BLOCK_SIZE];
        memcpy(block, fat + i * (BLOCK_SIZE / sizeof(uint32_t)), BLOCK_SIZE);
        if (block_write(sb.fat_start + i, block) == -1) return -1;
    }
    return 0;
}

static int read_fat() {
    for (uint32_t i = 0; i < sb.fat_blocks; i++) {
        char block[BLOCK_SIZE];
        if (block_read(sb.fat_start + i, block) == -1) return -1;
        memcpy(fat + i * (BLOCK_SIZE / sizeof(uint32_t)), block, BLOCK_SIZE);
    }
    return 0;
}

static int write_dir() {
    for (uint32_t i = 0; i < sb.dir_blocks; i++) {
        char block[BLOCK_SIZE];
        memcpy(block, dir + i * (BLOCK_SIZE / sizeof(dir_entry_t)), BLOCK_SIZE);
        if (block_write(sb.dir_start + i, block) == -1) return -1;
    }
    return 0;
}

static int read_dir() {
    for (uint32_t i = 0; i < sb.dir_blocks; i++) {
        char block[BLOCK_SIZE];
        if (block_read(sb.dir_start + i, block) == -1) return -1;
        memcpy(dir + i * (BLOCK_SIZE / sizeof(dir_entry_t)), block, BLOCK_SIZE);
    }
    return 0;
}

// File system management
int make_fs(char *disk_name) {
    if (make_disk(disk_name) == -1) return -1;
    if (open_disk(disk_name) == -1) return -1;
    
    // Initialize superblock
    sb.magic = MAGIC_NUMBER;
    sb.fat_start = 1;
    sb.fat_blocks = 4;
    sb.dir_start = sb.fat_start + sb.fat_blocks;
    sb.dir_blocks = 1;
    sb.data_start = sb.dir_start + sb.dir_blocks;
    sb.free_blocks = DATA_BLOCKS;
    sb.created = time(NULL);
    sb.last_mounted = sb.created;
    
    // Allocate memory for FAT and directory
    fat = malloc(sb.fat_blocks * BLOCK_SIZE);
    dir = malloc(sb.dir_blocks * BLOCK_SIZE);
    
    if (!fat || !dir) {
        free(fat);
        free(dir);
        close_disk();
        return -1;
    }
    
    // Initialize FAT (all blocks free)
    memset(fat, 0, sb.fat_blocks * BLOCK_SIZE);
    
    // Initialize directory (all entries free)
    memset(dir, 0, sb.dir_blocks * BLOCK_SIZE);
    
    // Write metadata to disk
    if (write_superblock() == -1 || write_fat() == -1 || write_dir() == -1) {
        free(fat);
        free(dir);
        close_disk();
        return -1;
    }
    
    free(fat);
    free(dir);
    close_disk();
    return 0;
}

int mount_fs(char *disk_name) {
    if (disk_fd != -1) return -1; // Already mounted
    
    if (open_disk(disk_name) == -1) return -1;
    disk_fd = 1; // Mark as mounted
    
    // Read superblock
    if (read_superblock() == -1) {
        close_disk();
        disk_fd = -1;
        return -1;
    }
    
    // Verify magic number
    if (sb.magic != MAGIC_NUMBER) {
        close_disk();
        disk_fd = -1;
        return -1;
    }
    
    // Allocate memory for FAT and directory
    fat = malloc(sb.fat_blocks * BLOCK_SIZE);
    dir = malloc(sb.dir_blocks * BLOCK_SIZE);
    
    if (!fat || !dir) {
        free(fat);
        free(dir);
        close_disk();
        disk_fd = -1;
        return -1;
    }
    
    // Read FAT and directory
    if (read_fat() == -1 || read_dir() == -1) {
        free(fat);
        free(dir);
        close_disk();
        disk_fd = -1;
        return -1;
    }
    
    // Initialize file descriptor table
    memset(fd_table, 0, sizeof(fd_table));
    
    // Update last mounted time
    sb.last_mounted = time(NULL);
    write_superblock();
    
    return 0;
}

int umount_fs(char *disk_name) {
    if (disk_fd == -1) return -1; // Not mounted
    
    // Close all open file descriptors
    for (int i = 0; i < MAX_FD; i++) {
        if (fd_table[i].used) {
            fd_table[i].used = 0;
        }
    }
    
    // Write metadata to disk
    if (write_fat() == -1 || write_dir() == -1 || write_superblock() == -1) {
        free(fat);
        free(dir);
        close_disk();
        disk_fd = -1;
        return -1;
    }
    
    free(fat);
    free(dir);
    fat = NULL;
    dir = NULL;
    
    if (close_disk() == -1) {
        disk_fd = -1;
        return -1;
    }
    
    disk_fd = -1;
    return 0;
}

// File operations
int fs_create(char *name) {
    if (disk_fd == -1) return -1;
    if (strlen(name) > MAX_FILE_NAME) return -1;
    if (find_file(name) != -1) return -1;
    
    int dir_index = find_free_dir_entry();
    if (dir_index == -1) return -1;
    
    // Initialize directory entry
    strncpy(dir[dir_index].name, name, MAX_FILE_NAME);
    dir[dir_index].name[MAX_FILE_NAME] = '\0';
    dir[dir_index].size = 0;
    dir[dir_index].first_block = (uint32_t)-1;
    dir[dir_index].created = time(NULL);
    dir[dir_index].modified = dir[dir_index].created;
    dir[dir_index].used = 1;
    
    if (write_dir() == -1) {
        dir[dir_index].used = 0;
        return -1;
    }
    
    return 0;
}

int fs_delete(char *name) {
    if (disk_fd == -1) return -1;
    
    int dir_index = find_file(name);
    if (dir_index == -1) return -1;
    
    // Check if file is open
    for (int i = 0; i < MAX_FD; i++) {
        if (fd_table[i].used && fd_table[i].dir_index == dir_index) {
            return -1;
        }
    }
    
    // Free all blocks in the FAT chain
    uint32_t block = dir[dir_index].first_block;
    while (block != (uint32_t)-1 && block < DATA_BLOCKS) {
        uint32_t next_block = fat[block];
        fat[block] = 0;
        sb.free_blocks++;
        block = next_block;
    }
    
    // Mark directory entry as free
    dir[dir_index].used = 0;
    
    // Update metadata
    if (write_fat() == -1 || write_dir() == -1 || write_superblock() == -1) {
        return -1;
    }
    
    return 0;
}

int fs_open(char *name) {
    if (disk_fd == -1) return -1;
    
    int dir_index = find_file(name);
    if (dir_index == -1) return -1;
    
    int fd = find_free_fd();
    if (fd == -1) return -1;
    
    fd_table[fd].dir_index = dir_index;
    fd_table[fd].offset = 0;
    fd_table[fd].used = 1;
    
    return fd;
}

int fs_close(int fildes) {
    if (disk_fd == -1) return -1;
    if (fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used) return -1;
    
    fd_table[fildes].used = 0;
    return 0;
}

int fs_read(int fildes, void *buf, size_t nbyte) {
    if (disk_fd == -1) return -1;
    if (fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used) return -1;
    
    int dir_index = fd_table[fildes].dir_index;
    uint32_t offset = fd_table[fildes].offset;
    uint32_t size = dir[dir_index].size;
    
    if (offset >= size) return 0;
    
    // Calculate how many bytes we can actually read
    size_t remaining = size - offset;
    size_t to_read = nbyte < remaining ? nbyte : remaining;
    size_t read = 0;
    
    // Find starting block and offset within block
    uint32_t block = dir[dir_index].first_block;
    uint32_t block_offset = offset / BLOCK_SIZE;
    uint32_t byte_in_block = offset % BLOCK_SIZE;
    
    // Skip to the starting block
    for (uint32_t i = 0; i < block_offset && block != (uint32_t)-1; i++) {
        block = fat[block];
    }
    
    // Read data block by block
    while (read < to_read && block != (uint32_t)-1) {
        char data[BLOCK_SIZE];
        if (block_read(sb.data_start + block, data) == -1) return -1;
        
        size_t bytes_in_this_block = BLOCK_SIZE - byte_in_block;
        size_t bytes_needed = to_read - read;
        size_t bytes_to_copy = bytes_in_this_block < bytes_needed ? bytes_in_this_block : bytes_needed;
        
        memcpy((char*)buf + read, data + byte_in_block, bytes_to_copy);
        read += bytes_to_copy;
        byte_in_block = 0;
        block = fat[block];
    }
    
    fd_table[fildes].offset += read;
    return read;
}

int fs_write(int fildes, void *buf, size_t nbyte) {
    if (disk_fd == -1) return -1;
    if (fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used) return -1;
    
    int dir_index = fd_table[fildes].dir_index;
    uint32_t offset = fd_table[fildes].offset;
    uint32_t size = dir[dir_index].size;
    
    // Check if we're extending the file
    if (offset + nbyte > size) {
        if (offset + nbyte > DATA_BLOCKS * BLOCK_SIZE) {
            nbyte = DATA_BLOCKS * BLOCK_SIZE - offset;
        }
        dir[dir_index].size = offset + nbyte;
    }
    
    size_t written = 0;
    
    // Find starting block and offset within block
    uint32_t block = dir[dir_index].first_block;
    uint32_t block_offset = offset / BLOCK_SIZE;
    uint32_t byte_in_block = offset % BLOCK_SIZE;
    
    // If file is empty, allocate first block
    if (block == (uint32_t)-1 && nbyte > 0) {
        block = find_free_block();
        if (block == (uint32_t)-1) return -1;
        dir[dir_index].first_block = block;
        fat[block] = (uint32_t)-1;
        sb.free_blocks--;
        block_offset = 0;
    }
    
    // Skip to the starting block
    uint32_t prev_block = (uint32_t)-1;
    for (uint32_t i = 0; i < block_offset && block != (uint32_t)-1; i++) {
        prev_block = block;
        block = fat[block];
    }
    
    // If we need to extend the block chain
    if (block == (uint32_t)-1 && nbyte > written) {
        block = find_free_block();
        if (block == (uint32_t)-1) {
            dir[dir_index].size = offset + written;
            goto done;
        }
        fat[prev_block] = block;
        fat[block] = (uint32_t)-1;
        sb.free_blocks--;
        byte_in_block = 0;
    }
    
    // Write data block by block
    while (written < nbyte && block != (uint32_t)-1) {
        char data[BLOCK_SIZE] = {0};
        
        // Read existing block if we're not writing it completely
        if (byte_in_block != 0 || (nbyte - written) < BLOCK_SIZE) {
            if (block_read(sb.data_start + block, data) == -1) {
                dir[dir_index].size = offset + written;
                goto done;
            }
        }
        
        size_t bytes_in_this_block = BLOCK_SIZE - byte_in_block;
        size_t bytes_needed = nbyte - written;
        size_t bytes_to_copy = bytes_in_this_block < bytes_needed ? bytes_in_this_block : bytes_needed;
        
        memcpy(data + byte_in_block, (char*)buf + written, bytes_to_copy);
        if (block_write(sb.data_start + block, data) == -1) {
            dir[dir_index].size = offset + written;
            goto done;
        }
        
        written += bytes_to_copy;
        byte_in_block = 0;
        
        // If we need another block
        if (written < nbyte && fat[block] == (uint32_t)-1) {
            uint32_t new_block = find_free_block();
            if (new_block == (uint32_t)-1) {
                dir[dir_index].size = offset + written;
                goto done;
            }
            fat[block] = new_block;
            fat[new_block] = (uint32_t)-1;
            sb.free_blocks--;
        }
        
        block = fat[block];
    }

done:
    fd_table[fildes].offset += written;
    dir[dir_index].modified = time(NULL);
    return written;
}

int fs_get_filesize(int fildes) {
    if (disk_fd == -1) return -1;
    if (fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used) return -1;
    
    int dir_index = fd_table[fildes].dir_index;
    return dir[dir_index].size;
}

int fs_lseek(int fildes, off_t offset) {
    if (disk_fd == -1) return -1;
    if (fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used) return -1;
    
    int dir_index = fd_table[fildes].dir_index;
    if (offset < 0 || offset > dir[dir_index].size) return -1;
    
    fd_table[fildes].offset = offset;
    return 0;
}

int fs_truncate(int fildes, off_t length) { // off_t 
    if (disk_fd == -1) return -1;
    if (fildes < 0 || fildes >= MAX_FD || !fd_table[fildes].used) return -1;
    
    int dir_index = fd_table[fildes].dir_index;
    uint32_t size = dir[dir_index].size;
    
    if (length > size) return -1;
    if (length == size) return 0;
    
    // Find the block where truncation happens
    uint32_t block = dir[dir_index].first_block;
    uint32_t trunc_block = length / BLOCK_SIZE;
    uint32_t i = 0;
    uint32_t prev_block = (uint32_t)-1;
    
    while (i < trunc_block && block != (uint32_t)-1) {
        prev_block = block;
        block = fat[block];
        i++;
    }
    
    // Free all remaining blocks
    if (block != (uint32_t)-1) {
        if (prev_block != (uint32_t)-1) {
            fat[prev_block] = (uint32_t)-1;
        } else {
            dir[dir_index].first_block = (uint32_t)-1;
        }
        
        while (block != (uint32_t)-1) {
            uint32_t next_block = fat[block];
            fat[block] = 0;
            sb.free_blocks++;
            block = next_block;
        }
    }
    
    // Update file size and offset if needed
    dir[dir_index].size = length;
    if (fd_table[fildes].offset > length) {
        fd_table[fildes].offset = length;
    }
    
    dir[dir_index].modified = time(NULL);
    return 0;
}