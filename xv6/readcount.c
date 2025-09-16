#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

int
main(void)
{
    char buf[100];
    int fd;
    int i;

    // Create a 100-byte buffer of 'A' characters
    for(i = 0; i < 100; i++){
      buf[i] = 'A';
    }

    printf("Read Count Test Program\n");

    // 1. Get the initial read count
    int initial_count = getreadcount();
    printf("Initial read count: %d\n", initial_count);

    // 2. Create and write 100 bytes to a dummy file
    fd = open("dummyread.txt", O_CREATE | O_WRONLY);
    if(fd < 0){
        printf("error: cannot create dummyread.txt\n");
        exit(1);
    }
    write(fd, buf, 100);
    close(fd);

    // 3. Read 100 bytes from the file to increase the count
    fd = open("dummyread.txt", O_RDONLY);
    if(fd < 0){
        printf("error: cannot open dummyread.txt for reading\n");
        exit(1);
    }
    read(fd, buf, 100);
    close(fd);
    printf("Just read 100 bytes from a file.\n");

    // 4. Get the final read count and print the difference
    int final_count = getreadcount();
    printf("Final read count: %d\n", final_count);
    printf("Difference (bytes read by this program): %d\n", final_count - initial_count);

    exit(0);
}