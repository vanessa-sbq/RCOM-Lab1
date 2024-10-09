// Write to serial port in non-canonical mode
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
#include <signal.h>


// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1


// Connection establishment
#define FLAG 0x7E

#define ADDRESS_SEND 0x03
#define ADDRESS_ANSWER 0x01

#define CONTROL_SET 0x03
#define CONTROL_UA 0x07

#define BCC1_SEND (ADDRESS_SEND ^ CONTROL_SET)
#define BCC1_RECEIVE (ADDRESS_SEND ^ CONTROL_UA)
//

#define BUF_SIZE 256

volatile int STOP = FALSE;

// Handler
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}


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

    // Open serial port device for reading and writing, and not as controlling tty
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
    newtio.c_cc[VTIME] = 1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

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

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < 4) {
            if (alarmEnabled == FALSE){
                alarm(3); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;

                unsigned char BCC1 = ADDRESS_SEND ^ CONTROL_SET;

                // Send SET byte
                int array_size = 5;
                unsigned char set_array[5] = {FLAG, ADDRESS_SEND, CONTROL_SET, BCC1, FLAG};

                int bytesWritten = 0;

                while (bytesWritten != 5) {
                    bytesWritten = writeBytes((set_array + sizeof(unsigned char) * bytesWritten), array_size - bytesWritten);

                    if (bytesWritten == -1) {
                        return -1;
                    }
                }
                sleep(1); // Wait until all bytes have been written to the serial port
            }

            unsigned char buf[5] = {0};

            int bufi = 0;
            while (bufi != 5) {
                int bytesRead = readByte( &buf[bufi] );

                switch (bytesRead) {
                    case -1:
                        return -1; // FIXME: Should return?
                        break;
                    case 0:
                        break;
                    default:
                        printf("var = 0x%02X\n", buf[bufi]); // FIXME: Debug
                        bufi++;
                        break;              
                }

                
            }

            int BCC1 = ADDRESS_SEND ^ CONTROL_UA;

            if (buf[0] == FLAG && buf[1] == ADDRESS_SEND && buf[2] == CONTROL_UA && buf[3] == BCC1 && buf[4] == FLAG){
                printf("Success!\n");
                break;
            } else {
                printf("Unsuccessful\n");
            }

            printf("alarm count: %d\n", alarmCount);
        
        }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
