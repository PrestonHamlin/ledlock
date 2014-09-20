# makefile by Preston Hamlin

obj-m       := ledlock.o


KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

# set to 1 to prevent wrap
l_nowrap    = 0
# set to whatever value is desired, or 0 for default
l_display   = 0
l_blank_d   = 0
l_blank_v   = 0

# compile-time flags for wrap, display time and blank time
ifneq ($(l_nowrap), 0)
  CFLAGS_ledlock.o += -DNOWRAP
endif
#ifneq ($l_wrap, 0)
#  CFLAGS_ledlock.o += -DWRAP
#endif

ifneq ($(l_display), 0)
  CFLAGS_ledlock.o += -DDISPLAY=$(l_display)
endif

ifneq ($(l_blank_d), 0)
  CFLAGS_ledlock.o += -DBLANK_D=$(l_blank_d)
endif

ifneq ($(l_blank_v), 0)
  CFLAGS_ledlock.o += -DBLANK_V=$(l_blank_v)
endif


modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules


tests: write9 write15 readtime ioctltest ioctlp ioctld ioctlw ioctl_timel ioctl_timed ioctl_timev

write9: write9.c
	gcc write9.c -o write9

write15: write15.c
	gcc write15.c -o write15

readtime: readtime.c
	gcc readtime.c -o readtime
	
	
	
	
ioctltest: ioctltest.c
	gcc ioctltest.c -o ioctltest

ioctlp: ioctlp.c
	gcc ioctlp.c -o ioctlp

ioctld: ioctld.c
	gcc ioctld.c -o ioctld

ioctlw: ioctlw.c
	gcc ioctlw.c -o ioctlw

ioctl_timel: ioctl_timel.c
	gcc ioctl_timel.c -o ioctl_timel

ioctl_timed: ioctl_timed.c
	gcc ioctl_timed.c -o ioctl_timed

ioctl_timev: ioctl_timev.c
	gcc ioctl_timev.c -o ioctl_timev



clean:
	rm -rf *.o .depend *.cmd *.ko *.mod.c .tmp_versions *.order *.symvers write9 write15 readtime ioctltest ioctlp ioctld ioctlw ioctl_timel ioctl_timed ioctl_timev

