Preston Hamlin (pwh11@my.fsu.edu)

This project is based on the "short" device driver for parallel port hardware
    from the "Linux Device Drivers" (LDD3) book by Jonathan Corbet, Alessandro
    Rubini and Greg Kroah-Hartman. This driver uses pins 2-9 of the parallel
    hardware port to connect to a custom device as described below.
The code within these files is largely written by myself, and any code that is
    of a differing nature is stated as such in the files. My code borrows
    significantly from the LDD3 book as well as the "short" kernel module which
    also is used to interact with parallel port devices. Note that I used the
    word interact rather than communicate. There is little to no functionality
    to perform handshakes with advanced hardware as a prelude to further\
    communication. Since not all of the pins are utilized, there is also a
    limit to bandwidth and functionality.

The device to be used is a simple wiring of pins to a led display. Each pin,
    when powered, illuminates a segment of the display. Bytes are written to
    the device, each bit having one pin.
        
My code functions by maintaining state via static global variables. These are
    used to determine whether or not to continue writing to the device or not.
    If the device were to be removed or the module unloaded, it would be rather
    problematic if the module were to continue scheduling parts of itself with
    the kernel.
Each second, the next number in a sequence is displayed until the maximum term
    is reached. When this happens the counter will either halt, displaying the
    final term, or wrap around to 0 and begin again. Since a number composed of
    more than one digit can be sent to the device, the digits must be displayed
    in sequence, but in a way such that they are still legible. There is a set
    minimum length of time which a digit will be displayed, so long numbers may
    take more than a second to display.
Since it can be obscure when one number ends and another begins, a feature to
    impliment might be the flashing of the horizontal segment between numbers
    to signify the border between digit sequences.



                                === Tasks ===
Blank until first write.                        
Blank on module removal.                        
Display # of wall-clock seconds since write().  
Skips some numbers as necessary.                

Write time as unsigned int.                     
    Check write size.                           
        Fail with EINVAL if bad length.         
    Set modulus to that value.                  
    Set counter to 0.                           
Read time as unsigned int.                      
    Check read size.                            
        Fail with EINVAL if bad length.         
    Returns the time on the clock.              
    
Init and Cleanup                                
    Cleanup stops any further scheduling.       

Compile-time flags.                             
    Wrap/stop.                                  
    Digit display time.                         
    Blank display time.                         
    Middle segment flash.                       

IOCTL                                           
    PAUSE                                       
    WRAP                                        
    DISPLAY                                     

Tests                                           
    Read and write.                             
    Change parameters.                          
    Multiple access.                            
    



