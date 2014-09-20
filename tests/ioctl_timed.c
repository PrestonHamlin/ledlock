#include "ledlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int main() {
    int fd;
    
    if ((fd = open("/dev/ledlock0", O_RDWR)) == -1) {
        printf("Error: ioctltest opening file\n");
        return -1;
    }

    // set length of blank between digits
    ioctl(fd, IOCTL_LEDLOCK_BLANK_DIGIT,  700); 

    return 0;
}
