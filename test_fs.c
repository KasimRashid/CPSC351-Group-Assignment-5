#include "fs.h"
#include <stdio.h>
#include <string.h>

int main() {
    // Create and mount the file system
    if (make_fs("test.disk") == -1) {
        printf("❌ Error creating file system\n");
        return 1;
    }

    if (mount_fs("test.disk") == -1) {
        printf("❌ Error mounting file system\n");
        return 1;
    }

    // Create a new file
    if (fs_create("test.txt") == -1) {
        printf("❌ Error creating file\n");
        return 1;
    }

    // Open the file
    int fd = fs_open("test.txt");
    if (fd == -1) {
        printf("❌ Error opening file\n");
        return 1;
    }

    // Write data to the file
    char data[] = "Hello, File System!";
    if (fs_write(fd, data, strlen(data)) == -1) {
        printf("❌ Error writing to file\n");
        return 1;
    }
    printf("✅ Wrote to file: \"%s\"\n", data);

    // Check file size
    int size = fs_get_filesize(fd);
    if (size == -1) {
        printf("❌ Error getting file size\n");
        return 1;
    }
    printf("📦 File size: %d bytes\n", size);

    // Seek to beginning
    if (fs_lseek(fd, 0) == -1) {
        printf("❌ Error seeking to beginning\n");
        return 1;
    }

    // Read file contents
    char buffer[100] = {0};
    int bytes_read = fs_read(fd, buffer, sizeof(buffer));
    if (bytes_read == -1) {
        printf("❌ Error reading file\n");
        return 1;
    }
    printf("📖 Read %d bytes: \"%s\"\n", bytes_read, buffer);

    // Truncate the file to 5 bytes
    if (fs_truncate(fd, 5) == -1) {
        printf("❌ Error truncating file\n");
        return 1;
    }
    printf("✂️  Truncated file to 5 bytes\n");

    // Read after truncation
    if (fs_lseek(fd, 0) == -1) {
        printf("❌ Error seeking to beginning after truncate\n");
        return 1;
    }

    char short_buf[20] = {0};
    int short_read = fs_read(fd, short_buf, sizeof(short_buf));
    printf("📖 After truncate, read %d bytes: \"%s\"\n", short_read, short_buf);

    // Close file
    if (fs_close(fd) == -1) {
        printf("❌ Error closing file\n");
        return 1;
    }

    // Delete the file
    if (fs_delete("test.txt") == -1) {
        printf("❌ Error deleting file\n");
        return 1;
    }
    printf("🗑️  Deleted file \"test.txt\"\n");

    // Unmount file system
    if (umount_fs("test.disk") == -1) {
        printf("❌ Error unmounting file system\n");
        return 1;
    }
    printf("✅ File system unmounted successfully\n");

    return 0;
}
