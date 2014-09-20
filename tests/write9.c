// test program which writes the value of the timer

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    int fd;
    unsigned int val=9;

// testing scullsort
    if ((fd = open ("/dev/ledlock0", O_RDWR )) == -1) {
        perror("write9 opening file");
        return -1;
    }

    write (fd, &val, sizeof(val));
    close(fd);

    return 0;
}
