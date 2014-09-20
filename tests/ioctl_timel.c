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

    // set display length
    ioctl(fd, IOCTL_LEDLOCK_SHOW,         700); 
    
    return 0;
}
