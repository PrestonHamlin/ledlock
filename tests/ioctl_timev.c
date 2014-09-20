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

    // set length of blank between digit sequences
    ioctl(fd, IOCTL_LEDLOCK_BLANK_VALUE,  1500); 
    return 0;
}
