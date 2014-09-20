/*  Code by Preston Hamlin
This file contains code which will allow for a timer device as described in
    "Linux Device Drivers" (LDD3) by Jonathan Corbet, Alessandro Rubini and
    Greg Kroah-Hartman which connects to a standard parallel port and reads
    port 0x378. 

The itoa() function for converting an integer into a cstring is copyright of
    the Free Software Foundation under GPLv2. Many iterations exist, but that
    is the version I chose to use as opposed to rolling it from scratch.

See the included README file for explanations beyond the comments herein.
    
    TODO: handle larger timer modulus
    TODO: handle jiffy wrap
    TODO: 32 bit jiffy reads
*/

#include <linux/module.h>

#include <linux/sched.h>    // scheduling
#include <linux/kernel.h>   // printk and other stuff
#include <linux/fs.h>       // file system interactions
#include <linux/delay.h>    // timing stuff
#include <linux/ioport.h>   // ports, ports and more ports
#include <linux/wait.h>     // more timing stuff
#include <linux/time.h>     // time of day

#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>

#include "ledlock.h"

MODULE_AUTHOR ("Preston Hamlin");
MODULE_LICENSE("Dual BSD/GPL");

// segment bits
#define SEG_B   0b00000001
#define SEG_BL  0b00000010
#define SEG_M   0b00000100
#define SEG_BR  0b00001000
#define SEG_T   0b00010000
#define SEG_TR  0b00100000
#define SEG_TL  0b01000000
#define SEG_INT 0b10000000

// digits
#define L_DIGIT_0   (SEG_B | SEG_BL | SEG_TL | SEG_T | SEG_TR | SEG_BR)
#define L_DIGIT_1   (SEG_TR | SEG_BR)
#define L_DIGIT_2   (SEG_T | SEG_TR | SEG_M | SEG_BL | SEG_B)
#define L_DIGIT_3   (SEG_T | SEG_TR | SEG_BR | SEG_M | SEG_B)
#define L_DIGIT_4   (SEG_TL | SEG_M | SEG_TR | SEG_BR)
#define L_DIGIT_5   (SEG_T | SEG_TL | SEG_M | SEG_BR | SEG_B)
#define L_DIGIT_6   (SEG_T | SEG_TL | SEG_BL | SEG_B | SEG_BR | SEG_M)
#define L_DIGIT_7   (SEG_T | SEG_TR | SEG_BR)
#define L_DIGIT_8   (SEG_B | SEG_BL | SEG_TL | SEG_T | SEG_TR | SEG_BR | SEG_M)
#define L_DIGIT_9   (SEG_B | SEG_TL | SEG_T | SEG_TR | SEG_BR | SEG_M)

int ledlock_open (struct inode* inode, struct file* fp);
int ledlock_release (struct inode* inode, struct file* fp);

ssize_t ledlock_read(struct file *fp, char __user *buffer, size_t count,
                    loff_t *pos);
ssize_t ledlock_write(struct file *fp, char __user *buffer, size_t count,
                     loff_t *pos);

int     ledlock_ioctl(struct file* fp, unsigned int cmd, unsigned int arg);

int     ledlock_init(void);
void    ledlock_cleanup(void);

void    ledlock_display_digit(char val);
void    ledlock_display_clear(void);
void    ledlock_display_value(void);
void    itoa (char *buf, int base, int d);


// flags
static bool LEDLOCK_INITIALIZED = false;
static bool LEDLOCK_PAUSED;
static bool LEDLOCK_WRAP;
static bool LEDLOCK_DISPLAY;
static bool LEDLOCK_DISPLAY_BUSY;
static bool LEDLOCK_SCHEDULE;

static bool LEDLOCK_WRITTEN;

// globals
struct mutex state_mutex, counter_mutex;
struct workqueue_struct * ledlock_wq;
struct work_struct ledlock_work;
static unsigned int LEDLOCK_COUNT;
static unsigned int LEDLOCK_COUNT_CAP;
static unsigned int LEDLOCK_TIME_DISPLAY;       // AKA "dwell time"
static unsigned int LEDLOCK_TIME_BLANK_DIGIT;   // time inbetween digits
static unsigned int LEDLOCK_TIME_BLANK_VALUE;   // time between digit sequences

static unsigned long LEDLOCK_WRITE_JMARKER;     // marks time of last write
static unsigned long LEDLOCK_PAUSE_JCOUNT;      // jiffies elapsed while paused 
static unsigned long LEDLOCK_PAUSE_JMARKER;     // measures pause duration 

char LEDLOCK_LAST_DIGIT;

struct file_operations ledlock_fops = {
    .owner      = THIS_MODULE,
    .read       = ledlock_read,
    .write      = ledlock_write,
//    .poll       = ledlock_poll,
    .open       = ledlock_open,
    .release    = ledlock_release,
    .unlocked_ioctl      = ledlock_ioctl,
};






//=============================================================================
//                              Open & Close
//=============================================================================

int ledlock_open (struct inode* inode, struct file* fp) {
    printk("\tHey there, device opened.\n");
    
    return 0;
}

int ledlock_release (struct inode* inode, struct file* fp) {
    printk("\tDevice released\n");
    
    return 0;
}




//=============================================================================
//                              Read & Write
//=============================================================================

ssize_t ledlock_read(struct file *fp, char __user *buffer, size_t count,
                    loff_t *pos)
{
    unsigned int val;
    
    // if invalid read attempt, fail
    if (count != sizeof(unsigned int)) return -EINVAL;
    
    // read timer value
    mutex_lock(&counter_mutex);
        copy_to_user(buffer, &LEDLOCK_COUNT, count);
        val = LEDLOCK_COUNT;
    mutex_unlock(&counter_mutex);
    
    printk("\tRead timer: %u\n", val);
    return count;
}

ssize_t ledlock_write(struct file *fp, char __user *buffer, size_t count,
                     loff_t *pos)
{
    unsigned int val;
    
    // if invalid write attempt, fail
    if (count != sizeof(unsigned int)) return -EINVAL;
        
    // set new value for counter cap and reset counter, also reset time
    mutex_lock(&counter_mutex);
        LEDLOCK_COUNT = 0;
        copy_from_user(&LEDLOCK_COUNT_CAP, buffer, count);
        val = LEDLOCK_COUNT_CAP;
        LEDLOCK_WRITE_JMARKER = jiffies;
    mutex_unlock(&counter_mutex);
    
    mutex_lock(&state_mutex);
        LEDLOCK_PAUSED = false;
        LEDLOCK_WRITTEN = true;
    mutex_unlock(&state_mutex);
    
    printk("\tNew counter cap: %u\n", val);
    return count;
}



//=============================================================================
//                                  Helpers
//=============================================================================


// writes 8 bits to device
//  does not reorder bits, just writes as-is
void ledlock_display_digit(char val) {
    LEDLOCK_LAST_DIGIT = val;
    outb(val, 0x378);
}

// same as above, but clears display
void ledlock_display_clear(void) {
    outb(0, 0x378);
}

// This function is to be scheduled repeatedly for display.
// It reads timer count from global variable then displays each digit and will
//  sleep as needed between each digit and after completion of the number. It
//  will then schedule itself to be called again if need be. Although the
//  structure of the function should prevent more than one instance running
//  concurrently, the LEDLOCK_DISPLAY_BUSY flag will be used to prevent 
//  simultanious writes to the device. If the driver is paused, the function
//  will display the last digit and wait for the device to be unpaused before
//  continuting where it left off.
void ledlock_display_value(void) {
    char digit_buffer[11], *p1;
    int val;
    long jcount;
    bool paused, busy, display, wrap, schedule, written;
    struct timeval t;
    
    printk("\tAttempting to display...\n");
    
    digit_buffer[10] = 0;   // terminate the cstring
    p1 = digit_buffer;      // look at start of string
    
    // if paused, sleep until unpaused
    mutex_lock(&state_mutex);
        paused  = LEDLOCK_PAUSED;
        busy    = LEDLOCK_DISPLAY_BUSY;
        wrap    = LEDLOCK_WRAP;
        written = LEDLOCK_WRITTEN;
    mutex_unlock(&state_mutex);
    
    printk(wrap? "\t\tWrap\n" : "\t\tNoWrap\n");
    
    if (!written) return;   // if no data whatsoever, give up
    if (busy) return;       // if another instance is displaying, give up
    mutex_lock(&state_mutex);
        LEDLOCK_DISPLAY_BUSY = true;
    mutex_unlock(&state_mutex);
    
    while(paused) {
        ledlock_display_digit(LEDLOCK_LAST_DIGIT);
        msleep(50);
        mutex_lock(&state_mutex);
            paused  = LEDLOCK_PAUSED;
        mutex_unlock(&state_mutex);
        printk("\texternal pause...%X\n", LEDLOCK_LAST_DIGIT);
    }
    
    
    // update the counter, since we may have skipped over a number
    // also, get the number to display
    mutex_lock(&counter_mutex);
        
        val = jiffies_to_msecs(jiffies -
                               (LEDLOCK_WRITE_JMARKER +
                                LEDLOCK_PAUSE_JCOUNT)
                               ) / 1000;
        if (wrap) {
            if (val >= LEDLOCK_COUNT_CAP) {
                LEDLOCK_COUNT = val % LEDLOCK_COUNT_CAP;
            }
            else LEDLOCK_COUNT = val;
        }
        else {    // if not wrapping, get the minimum
            LEDLOCK_COUNT = min(val, LEDLOCK_COUNT_CAP);
            printk("\t\tWrap is off\n");
        }
        val = LEDLOCK_COUNT;
    mutex_unlock(&counter_mutex);
    
    
    // fill buffer
    itoa(digit_buffer, 'd', val);
    printk("\t\tDisplaying: %u\n", val);
    
    while (*p1) {
        mutex_lock(&state_mutex);
            paused = LEDLOCK_PAUSED;
        mutex_unlock(&state_mutex);
        
        // while paused, sleep
        while(paused) {
            ledlock_display_digit(LEDLOCK_LAST_DIGIT);
            msleep(50);
            printk("\tinternal pause...\n");
        }
        
        *p1 -= '0';
        
        // do not write to device if display is disabled
        mutex_lock(&state_mutex);
            display = LEDLOCK_DISPLAY;
        mutex_unlock(&state_mutex);
        
        if (display) {
            if      (*p1 == 0) ledlock_display_digit(L_DIGIT_0);
            else if (*p1 == 1) ledlock_display_digit(L_DIGIT_1);
            else if (*p1 == 2) ledlock_display_digit(L_DIGIT_2);
            else if (*p1 == 3) ledlock_display_digit(L_DIGIT_3);
            else if (*p1 == 4) ledlock_display_digit(L_DIGIT_4);
            else if (*p1 == 5) ledlock_display_digit(L_DIGIT_5);
            else if (*p1 == 6) ledlock_display_digit(L_DIGIT_6);
            else if (*p1 == 7) ledlock_display_digit(L_DIGIT_7);
            else if (*p1 == 8) ledlock_display_digit(L_DIGIT_8);
            else if (*p1 == 9) ledlock_display_digit(L_DIGIT_9);
            else {
                printk("\nERROR: Bad digit buffer\n");
                return;
            }
        }
        // sleep with digit displayed
        msleep(LEDLOCK_TIME_DISPLAY);
        
        // sleep between digits, with blank display
        if (wrap) ledlock_display_clear();
        if (*(p1+1)) msleep(LEDLOCK_TIME_BLANK_DIGIT);
        
        ++p1;
    }
    
    // sleep between values, with blank display
    if (wrap) ledlock_display_clear();
    msleep(LEDLOCK_TIME_BLANK_VALUE);
       
    // this instance is freeing the hardware
    mutex_lock(&state_mutex);
        LEDLOCK_DISPLAY_BUSY = false;
    mutex_unlock(&state_mutex);
    
    printk("\tFinished displaying\n");
}


void empty_helper(struct work_struct *work) {
    bool schedule;
    long jcount, mscount;
    struct timeval t;
    
//    printk("\tInside helper callback that is scheduled\n");
    
    // schedule another value display, using fresh "current" time
    mutex_lock(&state_mutex);
        schedule = LEDLOCK_SCHEDULE;
    mutex_unlock(&state_mutex);
    if (schedule) {
        queue_work(ledlock_wq, &ledlock_work);


        //jcount = jiffies;
        //mscount = ((jiffies_to_msecs(jiffies)+999)/1000) *1000;
        jcount = jiffies_to_msecs(jiffies);
        mscount = ((jcount+999)/1000) *1000 - jcount;
        //printk("\t\tmscount: %ld\n", mscount);
        //printk("\t\tjcount:  %ld - %ld\n", jcount, jiffies_to_msecs(jcount));
        printk("\t\tsleep: %ld\n", mscount);
        
        msleep(mscount);
//        jcount = msecs_to_jiffies((t.tv_sec +1)*1000); // round to next second
//        printk("\t\t%ld : %ld\n", jiffies, jcount);
//        schedule_delayed_work(&ledlock_work, jcount-jiffies);
        
        printk("\tScheduled\n");
        ledlock_display_value();
    }
    else printk("No longer scheduling\n");
};


/* Convert the integer D to a string and save the string in BUF. If
        BASE is equal to 'd', interpret that D is decimal, and if BASE is
        equal to 'x', interpret that D is hexadecimal. */
void itoa (char *buf, int base, int d) {
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;

    /* If %d is specified and D is minus, put `-' in the head. */
    if (base == 'd' && d < 0)
        {
            *p++ = '-';
            buf++;
            ud = -d;
        }
    else if (base == 'x')
        divisor = 16;

    /* Divide UD by DIVISOR until UD == 0. */
    do {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    /* Terminate BUF. */
    *p = 0;

    /* Reverse BUF. */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

//=============================================================================
//                                  IOCTL
//=============================================================================

int ledlock_ioctl(struct file* fp, unsigned int cmd, unsigned int arg) {
    bool paused;
    
    switch(cmd) {
        case IOCTL_LEDLOCK_PON:     // pause timer
            printk("\t\tIOCTL pause\n");
            mutex_lock(&state_mutex);
                paused = LEDLOCK_PAUSED;    // check if it was already paused
                LEDLOCK_PAUSED = true;
            mutex_unlock(&state_mutex);
            
            if (!paused) {  // mark time if not already paused
                mutex_lock(&counter_mutex);
                    LEDLOCK_PAUSE_JMARKER = jiffies;
                mutex_unlock(&counter_mutex);
            }
            ledlock_display_digit(LEDLOCK_LAST_DIGIT);
            break;
            
        case IOCTL_LEDLOCK_POFF:    // unpause timer
            printk("\t\tIOCTL unpause\n");
            mutex_lock(&state_mutex);
                paused = LEDLOCK_PAUSED;
                LEDLOCK_PAUSED = false;
            mutex_unlock(&state_mutex);
            
            if (paused) {  // increment pause-counter if it was paused
                mutex_lock(&counter_mutex);
                    LEDLOCK_PAUSE_JCOUNT = jiffies - LEDLOCK_PAUSE_JMARKER;
                mutex_unlock(&counter_mutex);
            }
            break;
            
        case IOCTL_LEDLOCK_DON:     // turn display on
            printk("\t\tIOCTL display on\n");
            mutex_lock(&state_mutex);
                LEDLOCK_DISPLAY = true;
            mutex_unlock(&state_mutex);
            break;
            
        case IOCTL_LEDLOCK_DOFF:    // turn display off
            printk("\t\tIOCTL display off\n");
            mutex_lock(&state_mutex);
                LEDLOCK_DISPLAY = false;
            mutex_unlock(&state_mutex);
            break;
            
        case IOCTL_LEDLOCK_WON:     // turn wrap on
            printk("\t\tIOCTL wrap on\n");
            mutex_lock(&state_mutex);
                LEDLOCK_WRAP = true;
            mutex_unlock(&state_mutex);
            
//            ledlock_display_value();
            break;
            
        case IOCTL_LEDLOCK_WOFF:    // turn wrap off
            printk("\t\tIOCTL wrap off\n");
            mutex_lock(&state_mutex);
                LEDLOCK_WRAP = false;
            mutex_unlock(&state_mutex);
            break;

        case IOCTL_LEDLOCK_SHOW:    // set display length
            printk("\t\tIOCTL set display length\n");
            mutex_lock(&counter_mutex);
                LEDLOCK_TIME_DISPLAY = arg;
            mutex_unlock(&counter_mutex);
            break;
            
        case IOCTL_LEDLOCK_BLANK_DIGIT:   // set digit blank length
            printk("\t\tIOCTL set blank length\n");
            mutex_lock(&counter_mutex);
                LEDLOCK_TIME_BLANK_DIGIT = arg;
            mutex_unlock(&counter_mutex);
            break;
        case IOCTL_LEDLOCK_BLANK_VALUE:   // set value blank length
            printk("\t\tIOCTL set blank length\n");
            mutex_lock(&counter_mutex);
                LEDLOCK_TIME_BLANK_VALUE = arg;
            mutex_unlock(&counter_mutex);
            break;
    }
    
    return 0;
}



//=============================================================================
//                              Init & Cleanup
//=============================================================================

int ledlock_init(void) {
    int result;
    
    printk("\nInitializing ledlock module...\n");
    if (LEDLOCK_INITIALIZED) {
        printk("ERROR: Already initialized!\n");
        return 1;
    }
    LEDLOCK_INITIALIZED = true;

    // register device
    result = register_chrdev(LEDLOCK_MAJOR, "ledlock", &ledlock_fops);
    if (result < 0) {
        printk("ERROR: Cannot register device\n");
        return result;
    }

    // clear bits
    ledlock_display_clear();

    // initialize mutexes
    mutex_init(&state_mutex);
    mutex_init(&counter_mutex);
    
    // initialize state
    mutex_lock(&state_mutex);
#if defined(WRAP) && defined(NOWRAP)
#error "Only one of WRAP and NOWRAP can be defined at once"
#elif defined(WRAP)
        printk("WRAP defined\n");
        LEDLOCK_WRAP = true;
#elif defined(NOWRAP)
        printk("NOWRAP defined\n");
        LEDLOCK_WRAP = false;
#else
        LEDLOCK_WRAP = true;
        printk("Default Wrap\n");
#endif

        LEDLOCK_PAUSED          = true;
        LEDLOCK_DISPLAY         = true; // pause will keep blank until write
        LEDLOCK_DISPLAY_BUSY    = false;
        LEDLOCK_SCHEDULE        = true;
        LEDLOCK_WRITTEN         = false;
        printk(LEDLOCK_WRAP ? "WRAP: T\n" : "WRAP: F\n"); 
    mutex_unlock(&state_mutex);

    // initialize globals
    mutex_lock(&counter_mutex);
#ifdef DISPLAY
        LEDLOCK_TIME_DISPLAY = DISPLAY;
#else
        LEDLOCK_TIME_DISPLAY = 200;
#endif
#ifdef BLANK_D
        LEDLOCK_TIME_BLANK_DIGIT = BLANK_D;
#else
        LEDLOCK_TIME_BLANK_DIGIT = 50;
#endif
#ifdef BLANK_V
        LEDLOCK_TIME_BLANK_VALUE = BLANK_V;
#else
        LEDLOCK_TIME_BLANK_VALUE = 200;
#endif
#if !defined(WRAP) && !defined(NOWRAP)
        LEDLOCK_WRAP = true;
#endif
        LEDLOCK_COUNT           = 0;
        LEDLOCK_COUNT_CAP       = 0;
        
        LEDLOCK_WRITE_JMARKER   = 0;
        LEDLOCK_PAUSE_JCOUNT    = 0;
        LEDLOCK_PAUSE_JMARKER   = 0;

        printk("Display: %u\n", LEDLOCK_TIME_DISPLAY);
        printk("BlankD: %u\n",  LEDLOCK_TIME_BLANK_DIGIT);
        printk("BlankV: %u\n",  LEDLOCK_TIME_BLANK_VALUE);
    mutex_unlock(&counter_mutex);
    
//    INIT_DELAYED_WORK(&ledlock_work, ledlock_display_value);
//    schedule_delayed_work(&ledlock_work, 100);

    ledlock_wq = create_workqueue("ledlock_workqueue");
//    INIT_WORK(&ledlock_work, ledlock_display_value);
//    queue_work(ledlock_wq, &ledlock_work);
//    ledlock_display_value(NULL);
    
    INIT_WORK(&ledlock_work, empty_helper);
    result = queue_work(ledlock_wq, &ledlock_work);
//    if (result) printk("What!?\n");
    
    //ledlock_display_value();

    printk("Module initialized!\n");
    return 0;
}


void ledlock_cleanup(void) {
    printk("Removing ledlock module...\n");

    // stop further scheduling
    mutex_lock(&state_mutex);
        LEDLOCK_SCHEDULE = false;
    mutex_unlock(&state_mutex);
    
    // flush and destroy work queue
    flush_workqueue(ledlock_wq);
    destroy_workqueue(ledlock_wq);
    
    // unregister device
    unregister_chrdev(LEDLOCK_MAJOR, "ledlock");
    
    // clear bits
    ledlock_display_clear();
    
    printk("Module removed!\n");
}



module_init(ledlock_init);
module_exit(ledlock_cleanup);
















