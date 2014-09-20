#include "ledlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;
    
    if ((fd = open("/dev/ledlock0", O_RDWR)) == -1) {
        printf("Error: ioctltest opening file\n");
        return -1;
    }
    // toggle wrap
    ioctl(fd, IOCTL_LEDLOCK_WON,    42);
    sleep(4);
    ioctl(fd, IOCTL_LEDLOCK_WOFF,   42); 

    return 0;
}
