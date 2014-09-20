/*  Code by Preston Hamlin
This file contains code which will allow for a timer device as described in
    "Linux Device Drivers" (LDD3) by Jonathan Corbet, Alessandro Rubini and
    Greg Kroah-Hartman which connects to a standard parallel port and reads
    port 0x378. 


See the included README file for explanations beyond the comments herein.
*/




#include <linux/ioctl.h>



// constants
#define LEDLOCK_MAJOR       399

// IOCTL definitions
#define IOCTL_LEDLOCK_PON   _IO(LEDLOCK_MAJOR, 0)   // pause timer
#define IOCTL_LEDLOCK_POFF  _IO(LEDLOCK_MAJOR, 1)   // unpause timer
#define IOCTL_LEDLOCK_DON   _IO(LEDLOCK_MAJOR, 2)   // turn display on
#define IOCTL_LEDLOCK_DOFF  _IO(LEDLOCK_MAJOR, 3)   // turn display off
#define IOCTL_LEDLOCK_WON   _IO(LEDLOCK_MAJOR, 4)   // turn wrap on
#define IOCTL_LEDLOCK_WOFF  _IO(LEDLOCK_MAJOR, 5)   // turn wrap off

// set the length of time to show digit
#define IOCTL_LEDLOCK_SHOW  _IOR(LEDLOCK_MAJOR, 6, unsigned int) 

// set length of time to have blank display between digits
#define IOCTL_LEDLOCK_BLANK_DIGIT _IOR(LEDLOCK_MAJOR, 7, unsigned int) 

// set length of time to have blank display between digit sequences
#define IOCTL_LEDLOCK_BLANK_VALUE _IOR(LEDLOCK_MAJOR, 8, unsigned int) 


