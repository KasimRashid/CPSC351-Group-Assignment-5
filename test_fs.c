#include "fs.h"
#include <stdio.h>

int main() {
    // Create and mount the file system
    if (make_fs("test.disk") == -1) {
        printf("Error creating file system\n");
        return 1;
    }
    
    if (mount_fs("test.disk") == -1) {
        printf("Error mounting file system\n");
        return 1;
    }
    
    // Test basic operations
    if (fs_create("test.txt") == -1) {
        printf("Error creating file\n");
        return 1;
    }
    
    int fd = fs_open("test.txt");
    if (fd == -1) {
        printf("Error opening file\n");
        return 1;
    }
    
    char data[] = "Hello, File System!";
    if (fs_write(fd, data, sizeof(data)) == -1) {
        printf("Error writing to file\n");
        return 1;
    }
    
    if (fs_close(fd) == -1) {
        printf("Error closing file\n");
        return 1;
    }
    
    // Verify the data
    fd = fs_open("test.txt");
    char buffer[100];
    int bytes_read = fs_read(fd, buffer, sizeof(buffer));
    if (bytes_read == -1) {
        printf("Error reading file\n");
        return 1;
    }
    
    printf("Read %d bytes: %s\n", bytes_read, buffer);
    
    // Clean up
    if (umount_fs("test.disk") == -1) {
        printf("Error unmounting file system\n");
        return 1;
    }
    
    return 0;
}