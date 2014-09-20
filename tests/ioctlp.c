#include "ledlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;
    
    if ((fd = open("/dev/ledlock0", O_RDWR)) == -1) {
        printf("Error: ioctlp opening file\n");
        return -1;
    }
    // pause and unpause
    ioctl(fd, IOCTL_LEDLOCK_PON,    42);
    sleep(5);
    ioctl(fd, IOCTL_LEDLOCK_POFF,   42);

    return 0;
}
