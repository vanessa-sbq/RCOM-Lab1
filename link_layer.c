// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

// Buffers
#define BUF_SIZE 256

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

// Reader State Machine && Acknowledgement State Machine
typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_STATE} state_t;

// Serial Port (File Descriptor)
static int fd;
static int numberOfRetransmitions = 0;

// Handler
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    numberOfRetransmitions = connectionParameters.nRetransmissions;

    if ((fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate)) < 0) {
        return -1;
    }

    if (connectionParameters.role == LlTx) {
        while (alarmCount < 4) {
            if (alarmEnabled == FALSE){
                alarm(3); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;

                unsigned char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_SET;

                // Send SET byte
                int array_size = 5;
                unsigned char set_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_SET, BCC1, FLAG};

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

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_UA;

            if (buf[0] == FLAG && buf[1] == ADDRESS_SENT_BY_TX && buf[2] == CONTROL_UA && buf[3] == BCC1 && buf[4] == FLAG){
                printf("Success!\n");
                break;
            } else {
                printf("Unsuccessful\n");
            }

            printf("alarm count: %d\n", alarmCount);
        
        }

    } else { // Receiver
        state_t state = START;

        while (state != STOP_STATE) {
            unsigned char byte = 0;
            read(fd, &byte, 1); // FIXME: can fail

            switch (state) {
                case START:
                    state = byte == FLAG ? FLAG_RCV : START;
                    break;
                case FLAG_RCV:
                    state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                    break;
                case A_RCV:
                    state = byte == FLAG ? FLAG_RCV : (byte == CONTROL_SET ? C_RCV : START);
                    break;
                case C_RCV:
                    unsigned char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_SET;
                    state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                    break;
                case BCC_OK:
                    state = byte == FLAG ? STOP_STATE : START;
                    break;
                case STOP_STATE:
                    break;
            }
        }

        int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_UA;
        unsigned char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_UA, BCC1, FLAG};
        write(fd, ua_array, 5);
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
bool controlType = FALSE;

void readAck() {

    state_t state = START;
    unsigned char BCC1 = 0x00;

    while (state != STOP_STATE) {
        unsigned char byte = 0;
        
        int readRet = readByte(&byte);

        if (readRet <= 0) {
            return;
        }

        switch (state) {
            case START:
                state = byte == FLAG ? FLAG_RCV : START;
                break;
            case FLAG_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                break;
            case A_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == CONTROL_SET ? C_RCV : START);
                break;
            case C_RCV:


                // FIXME: C_RCV does not contain the Control byte, it is only here to check if we can jump to BCC_OK...
                switch (C_RCV) {
                    case CONTROL_RR0:
                        break;
                    case CONTROL_RR1:
                        break;
                    case CONTROL_REJ0:
                        break;
                    case CONTROL_REJ1:
                        break;
                    case FLAG:
                        state = FLAG_RCV; 
                        break; 
                    case BCC1:
                    
                        dsdsddeeedfefe()


                        break;
                    default:
                        state = START;
                        break;
                }
                
                
                break;
            case BCC_OK:
                state = byte == FLAG ? STOP_STATE : START;
                break;
            case STOP_STATE:
                break;
        }
    }
    

}

int llwrite(const unsigned char *buf, int bufSize) {

    if (bufSize < 0 || buf == NULL) {
        return -1;    
    }

    int newFrameSize = bufSize + 6;
    unsigned char* frame = (char*)malloc(sizeof(char) * (newFrameSize));

    frame[0] = FLAG; 
    frame[1] = ADDRESS_SENT_BY_TX;

    if (!controlType) {
        frame[2] = 0x00;
        controlType = TRUE;
    } else {
        frame[2] = 0x80;
        controlType = FALSE; 
    }

    
    frame[3] = frame[1] ^ frame[2];

    unsigned char BCC2 = buf[0];

    for (int j = 1; j < bufSize; j++) {
        BCC2 ^= buf[j]; 
    }

    frame[newFrameSize - 2] = BCC2;
    frame[newFrameSize - 1] = FLAG;



    int retransmitionCounter = 0;
    int bufSizeModified = newFrameSize;

    while (retransmitionCounter <= numberOfRetransmitions) {  // TODO: Change condition

        if (alarmEnabled == FALSE){
                alarm(3); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;
                
                int writeRet = write(fd, (frame + bufSizeModified), bufSizeModified);
                
                if (writeRet < 0 || writeRet < bufSizeModified) {
                    retransmitionCounter++;
                    continue;
                }

                /*
                if (writeRet < bufSizeModified) {
                    bufSizeModified = bufSize - bufSizeModified;
                }
                */
                
                if (writeRet == bufSizeModified) {
                    break;
                }

            }

    }

    sleep(1); // Guarantee that all bytes were written.

    

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
