// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

// Frame Fields
#define FLAG 0x7E

#define ADDRESS_SENT_BY_TX 0x03 // or replies sent by receiver.
#define ADDRESS_SENT_BY_RX 0x01 // or replies sent by transmitter.

#define CONTROL_SET 0x03
#define CONTROL_UA 0x07

#define CONTROL_RR0 0xAA
#define CONTROL_RR1 0xAB
#define CONTROL_REJ0 0x54
#define CONTROL_REJ1 0x55
#define CONTROL_DISC 0x0B

// Handler
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    
    int fd;

    if ((fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate)) < 0)
    {
        return -1;
    }

    // TODO

    if (connectionParameters.role == LlTx) {
        
        while (alarmCount < 4) {
            if (alarmEnabled == FALSE){
                alarm(3); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;

                unsigned char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_SET;

                // Send SET byte
                int array_size = 5;
                unsigned char set_array[array_size] = {FLAG, ADDRESS_SEND, CONTROL_SET, BCC1, FLAG};

                int bytesWritten = 0

                while (bytesWritten != 5) {
                    printf("Base address:%x \n Starting address:%x \n bytesLeftToWrite = %u", set_array, set_array + (unsigned char)bytesWritten, array_size - bytesWritten);
                    bytesWritten = writeBytes((set_array + (unsigned char)bytesWritten), array_size - bytesWritten);

                    if (bytesWritten == -1) {
                        bytesWritten = 0; // FIXME: Should return ?
                    }
                }

                //TODO: remove ?
                //int set = write(fd, set_array, BUF_SIZE);
                //printf("%d set bytes written\n", set);

                // Wait until all bytes have been written to the serial port
                sleep(1);
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

            /*
            if (STOP == FALSE){
                printf("Em cima do read\n");
                read(fd, buf, BUF_SIZE);
                printf("Em baixo do read\n");

                for (int i = 0; i < 5; i++){
                    printf("var = 0x%02X\n", buf[i]);
                }

                if (buf[0] == FLAG) {
                    STOP = TRUE;
                    printf("Stop == true\n");
                }
                else {
                    printf("Stop == false\n");
                }
            }
            
            for (int i = 0; i < 5; i++){
                printf("var = 0x%02X\n", buf[i]);
            }
            */

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_UA;

            if (buf[0] == FLAG && buf[1] == ADDRESS_SENT_BY_TX && buf[2] == CONTROL_UA && buf[3] == BCC1 && buf[4] == FLAG){
                printf("Success!\n");
                break;
            } else {
                printf("Unsuccessful\n");
            }

            printf("alarm count: %d\n", alarmCount);
        
        }

    } else {
        // Loop for input
        unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char

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
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
