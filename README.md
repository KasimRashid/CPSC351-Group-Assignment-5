# CPSC351-Group-Assignment-5

gcc -c disk.c      # Compile the disk implementation

gcc -c fs.c        # Compile the file system

gcc -c test_fs.c   # Compile the test program

gcc disk.o fs.o test_fs.o -o fs_test  # Link everything together

./fs_test # to run the code
