// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

// Buffers
#define BUF_SIZE 256

// I frame number
#define I_FRAME_0 0x00
#define I_FRAME_1 0x80

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

#define ESCAPE_OCTET 0x7D
#define ESCAPE_XOR 0x20

// Reader State Machine && Acknowledgement State Machine
typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_STATE} state_t;

// Serial Port (File Descriptor)
static int fd;
static int numberOfRetransmitions = 0;
static int timeout = 0;

// I frame number
static int controlType = FALSE;

// Handler
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}


/// Supervision frames and Unnumbered frames state machine
int checkSUFrame(unsigned char controlField, int* ringringEnabled){
    state_t state = START;
    while (state != STOP_STATE && (*ringringEnabled)) {
        unsigned char byte = 0;
        if (readByte(&byte) == -1) return -1;
        switch (state) {
            case START:
                if (state == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                break;
            case A_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == controlField ? C_RCV : START);
                break;
            case C_RCV:
                unsigned char BCC1 = ADDRESS_SENT_BY_TX ^ controlField;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                state = byte == FLAG ? STOP_STATE : START;
                break;
            case STOP_STATE:
                break;
        }
    }
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    numberOfRetransmitions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    if ((fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate)) < 0) {
        return -1;
    }

    if (connectionParameters.role == LlTx) { // Transmitter
        while (alarmCount < numberOfRetransmitions) {
            int bytesWritten = 0;

            if (alarmEnabled == FALSE){
                alarm(timeout); // Set alarm to be triggered after timeout
                alarmEnabled = TRUE;

                // Assemble SET frame
                int array_size = 5;
                unsigned char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_SET;
                char set_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_SET, BCC1, FLAG};

                // Send SET frame
                while (bytesWritten != 5) { // FIXME: Remove hard-coded value?
                    bytesWritten = writeBytes((set_array + sizeof(char) * bytesWritten), array_size - bytesWritten);
                    if (bytesWritten == -1) {
                        return -1;
                    }
                }
                sleep(1); // Wait until all bytes have been written to the serial port
            }

            if (bytesWritten == 5) {
                int csu = checkSUFrame(CONTROL_UA, &alarmEnabled);
                if (csu == -1) return -1;
                else return 0;   
            }
        }
    } else if (connectionParameters.role == LlRx) { // Receiver
        int enterCheckSUFrame = TRUE;
        while (enterCheckSUFrame) {

            int csu = checkSUFrame(CONTROL_UA, &alarmEnabled);
            if (csu == -1) {
                return -1;
            }

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_UA;
            unsigned char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_UA, BCC1, FLAG};

            int wb = writeBytes(ua_array, 5);

            if (wb == -1) {
                return -1;
            } else if (wb == 5) {
                return 0;
            } else {
                continue;
            }
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////

// If it returns a negative value then an error occoured.
// If it returns 0 then no errors occoured.
int readIFrameResponse() {
    state_t state = START;
    unsigned char BCC1 = 0x00;
    char byte;

    while (state != STOP_STATE && alarmEnabled) {
        int rb = readByte(&byte);

        if (rb == -1) {
            return -1;
        }

        switch (state) {
            case START:
                state = byte == FLAG ? FLAG_RCV : START;
                break;
            case FLAG_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                break;
            case A_RCV:
                switch (byte) {
                    case CONTROL_RR0:
                        state = C_RCV;
                        controlType = FALSE;
                        break;
                    case CONTROL_RR1:
                        state = C_RCV;
                        controlType = TRUE;
                        break;
                    case CONTROL_REJ0:
                        state = C_RCV;
                        break;
                    case CONTROL_REJ1:
                        state = C_RCV;
                        break;
                    case FLAG:
                        state = FLAG_RCV; 
                        break;                    
                    default:
                        state = START;
                        break;
                }

                BCC1 = ADDRESS_SENT_BY_TX ^ byte;
                break;
            case C_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                state = byte == FLAG ? STOP_STATE : START;
                break;
            case STOP_STATE:
                break;
        }


    }

    return 0;
    
}

int llwrite(const unsigned char *buf, int bufSize) {
    if (bufSize < 0 || buf == NULL) {
        return -1;    
    }

    int newFrameSize = bufSize + 6;
    for (int i = 0; i < bufSize; i++) { // Get the new frame size for byte stuffing
        if (buf[i] == FLAG || buf[i] == ESCAPE_OCTET) newFrameSize++;
    }

    unsigned char* frame = (unsigned char*)malloc(sizeof(unsigned char) * (newFrameSize));

    frame[0] = FLAG; 
    frame[1] = ADDRESS_SENT_BY_TX;

    if (!controlType) {
        frame[2] = 0x00;
    } else {
        frame[2] = 0x80;
    }

    
    frame[3] = frame[1] ^ frame[2];

    int j = 0;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE_OCTET){
            frame[4 + j] = ESCAPE_OCTET;
            j++;
            frame[4 + j] = buf[i] ^ ESCAPE_XOR; // DO THE XOR
        } else {
            frame[4+j] = buf[i];
        }
        j++;
    }

    printf("\n DEBUG: Now doing byte stuffing part. -> Before: "); // TODO: Remove
    for (int i = 0; i < bufSize; i++) {
        printf("%x ", buf[i]);
    }

    printf("\n After Byte Stuffing: "); // TODO: Remove
    for (int i = 0; i < newFrameSize; i++) {
        printf("%x ", frame[i]);
    }

    printf("\n");   // TODO: Remove

    unsigned char BCC2 = buf[0];
    for (int j = 1; j < bufSize; j++) {
        BCC2 ^= buf[j]; 
    }

    frame[newFrameSize - 2] = BCC2;
    frame[newFrameSize - 1] = FLAG;

    int retransmitionCounter = 0;
    int wb = 0;

    while (retransmitionCounter <= numberOfRetransmitions) {  // TODO: Change condition

        if (alarmEnabled == FALSE){
            alarm(timeout); // Set alarm
            alarmEnabled = TRUE;
            
            wb = writeBytes(frame, newFrameSize);

            if (wb == -1) {
                return -1;
            } else if (wb == newFrameSize) {
                break;
            } else {
                retransmitionCounter++;
                continue;
            }
            
        }
        sleep(1); // Guarantee that all bytes were written.
        readIFrameResponse();
    }

    free(frame);

    return wb;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    if (packet == NULL) {
        return -1;
    }

    int state = START;
    char* dataFrame = (char *)malloc(sizeof(char)); // The data from the information frame will be stored here.
    int currentDataFrameIt = 0;

    while (state != STOP_STATE) {
        unsigned char byte = 0;
        if (readByte(&byte) == -1) return -1;

        int controlField = controlType ? I_FRAME_1 : I_FRAME_0;

        switch (state) {
            case START:
                if (state == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                break;
            case A_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == controlField ? C_RCV : START);
                break;
            case C_RCV:
                unsigned char BCC1 = ADDRESS_SENT_BY_TX ^ controlField;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                dataFrame[currentDataFrameIt] = byte;
                currentDataFrameIt++;
                realloc(dataFrame, (currentDataFrameIt + 1) * sizeof(char));

                if (byte == FLAG) state = STOP_STATE;
                // gets first byte...
                // gets next byte ...
                // gets n byte ...

                // when byte is equal to flag -> Stop.
        }

    }

    // TODO: Byte destuffing

    // TODO: Check BCC2

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
