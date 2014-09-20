#include "ledlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;
    
    if ((fd = open("/dev/ledlock0", O_RDWR)) == -1) {
        printf("Error: ioctld opening file\n");
        return -1;
    }
    // toggle display
    ioctl(fd, IOCTL_LEDLOCK_DOFF,   42); 
    sleep(3);
    ioctl(fd, IOCTL_LEDLOCK_DON,    42); 
    
    return 0;
}
