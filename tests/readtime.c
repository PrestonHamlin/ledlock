// test program which reads the value of the timer

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    int fd;
    unsigned int val=0;

// testing scullsort
    if ((fd = open ("/dev/ledlock0", O_RDWR )) == -1) {
        perror("readtime opening file");
        return -1;
    }

    read (fd, &val, sizeof(val));
    fprintf (stdout, "\nreadtime: \"%u\"\n", val);
    close(fd);

    return 0;
}
