// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define FLAG 0x7E
#define ADDRESS 0x03

#define CONTROL_SET 0x03
#define CONTROL_UA 0x07

#define BCC1_RECEIVE (ADDRESS ^ CONTROL_SET)
#define BCC1_SEND (ADDRESS ^ CONTROL_UA)

typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP} state_t;

//volatile int STOP = FALSE;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    /*
    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    */

    state_t state = START;

    while (state != STOP){

        unsigned char byte = 0;
        read(fd, &byte, 1); // FIXME: can fail

        switch (state) {
            case START:
                state = byte == FLAG ? FLAG_RCV : START;
                break;
            case FLAG_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS ? A_RCV : START);
                break;
            case A_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == CONTROL_SET ? C_RCV : START);
                break;
            case C_RCV:
                unsigned char BCC1 = ADDRESS ^ CONTROL_SET;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                state = byte == FLAG ? STOP : START;
                break;
            case STOP:
                break;
        }
    }

    unsigned char ua_array[BUF_SIZE] = {FLAG, ADDRESS, CONTROL_UA, BCC1_SEND, FLAG};
	write(fd, ua_array, BUF_SIZE);

    /*
    while (STOP == FALSE)
    {
        // Returns after 5 chars have been input
        int bytes = read(fd, buf, BUF_SIZE);
        buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

        printf(":%s:%d\n", buf, bytes);
        if (buf[0] == FLAG)
            STOP = TRUE;
    }

   for (int i = 0; i < 5; i++) {
       printf("var = 0x%02X\n", buf[i]);
   }


    if (buf[0] == FLAG && buf[1] == ADDRESS && buf[2] == CONTROL_SET && buf[3] == BCC1_RECEIVE && buf[4] == FLAG) {
	unsigned char ua_array[BUF_SIZE] = {FLAG, ADDRESS, CONTROL_UA, BCC1_SEND, FLAG};
	write(fd, ua_array, BUF_SIZE);
    }

    */


    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
    printf("Success!\n");
    return 0;
}
