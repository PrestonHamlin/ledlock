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
    // pause and unpause
    ioctl(fd, IOCTL_LEDLOCK_PON,    42);
    ioctl(fd, IOCTL_LEDLOCK_POFF,   42);

    // toggle display
    ioctl(fd, IOCTL_LEDLOCK_DOFF,   42); 
    ioctl(fd, IOCTL_LEDLOCK_DON,    42); 
    
    // toggle wrap
    ioctl(fd, IOCTL_LEDLOCK_WON,    42);
    ioctl(fd, IOCTL_LEDLOCK_WOFF,   42); 

//    ioctl(fd, IOCTL_LEDLOCK_SHOW,         42); 
//    ioctl(fd, IOCTL_LEDLOCK_BLANK_DIGIT,  42); 
//    ioctl(fd, IOCTL_LEDLOCK_BLANK_VALUE,  42); 
    return 0;
}
