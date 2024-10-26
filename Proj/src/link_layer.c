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
typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_STATE, CHECK_DATA} state_t;

// Serial Port (File Descriptor)
static int fd;
static int numberOfRetransmitions = 0;
static int timeout = 0;
static LinkLayerRole role;

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
int checkSUFrame(char controlField, int* ringringEnabled){
    state_t state = START;
    while (state != STOP_STATE && (*ringringEnabled)) {
        char byte = 0;
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
                char BCC1 = ADDRESS_SENT_BY_TX ^ controlField;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                state = byte == FLAG ? STOP_STATE : START;
                break;
            case STOP_STATE:
                break;
            default:
                state = START;
                break;
        }
    }
    return 0;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    numberOfRetransmitions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    role = connectionParameters.role;

    if ((fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate)) < 0) {
        return -1;
    }

    if (role == LlTx) { // Transmitter
        while (alarmCount < numberOfRetransmitions) {
            int bytesWritten = 0;

            if (alarmEnabled == FALSE){
                alarm(timeout); // Set alarm to be triggered after timeout
                alarmEnabled = TRUE;

                // Assemble SET frame
                int array_size = 5;
                char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_SET;
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
    } else if (role == LlRx) { // Receiver
        int enterCheckSUFrame = TRUE;
        while (enterCheckSUFrame) {

            int csu = checkSUFrame(CONTROL_UA, &alarmEnabled);
            if (csu == -1) {
                return -1;
            }

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_UA;
            char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_UA, BCC1, FLAG};

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
    char BCC1 = 0x00;
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
            default:
                state = START;
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

    char* frame = (char*)malloc(sizeof(char) * (newFrameSize));

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

    char BCC2 = buf[0];
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

void sendAck(int localControlType) {
    char RR = 0x00;

    if (!localControlType) { // Inverted
        RR = CONTROL_RR1;
    } else {
        RR = CONTROL_RR0;
    }


    char BCC1 = RR ^ ADDRESS_SENT_BY_TX;

    // SEND ACK
    char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, RR, BCC1, FLAG};
    int wb = writeBytes(ua_array, 5);
}


int llread(unsigned char *packet) {
    if (packet == NULL) {
        return -1;
    }

    int state = START;
    char* dataFrame = (char *)malloc(sizeof(char)); // The data from the information frame will be stored here.
    int currentDataFrameIt = 0;

    while (state != STOP_STATE) {
        char byte = 0;
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

                if ((byte == I_FRAME_0 && controlType) || (byte == I_FRAME_1 && !controlType)) {
                    sendAck(!controlType); // We want to send the old acknowledgment.
                    state = STOP_STATE;
                    currentDataFrameIt = 0;
                    free(dataFrame);
                    return 0; // Duplicate was found, we have a total of 0 bytes
                }

                break;
            case C_RCV:
                char BCC1 = ADDRESS_SENT_BY_TX ^ controlField;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                dataFrame[currentDataFrameIt] = byte;
                currentDataFrameIt++;
                dataFrame = (char*)realloc(dataFrame, (currentDataFrameIt + 1) * sizeof(char));

                if (dataFrame == NULL || currentDataFrameIt + 1 > MAX_PAYLOAD_SIZE) return -1;

                if (byte == FLAG) state = CHECK_DATA;
                // gets first byte...
                // gets next byte ...
                // gets n byte ...

                // when byte is equal to flag -> Stop.
        }

    
        if (state == CHECK_DATA) {
            int data_bcc2_flag_size = currentDataFrameIt;

            char* actualData = (char*)malloc(sizeof(char));
            int actualDataIt = 0;
            int sizeOfActualData = 1;

            int expectDestuffing = FALSE;
            for (int i = 0; i < (data_bcc2_flag_size - 2); i++) { // Byte destuffing

                if (expectDestuffing) {

                    if (((dataFrame[i] ^ ESCAPE_XOR) == ESCAPE_OCTET) || ((dataFrame[i] ^ ESCAPE_XOR) == FLAG)) { // Then we are really suppost to do destuffing
                        actualData[actualDataIt] = dataFrame[i] ^ ESCAPE_XOR;
                    } else {
                        // TODO: REMOVE
                        printf("else case\n");

                        // Add previous byte... our expectations were not correct
                        actualData[actualDataIt] = ESCAPE_OCTET;
                        actualDataIt++;
                        sizeOfActualData++;
                        actualData = (char*)realloc(actualData, sizeOfActualData * sizeof(char));
                        if (actualData == NULL) return -1;

                        // Add current byte
                        actualData[actualDataIt] = dataFrame[i];
                    }

                    actualDataIt++;
                    sizeOfActualData++;
                    actualData = (char*)realloc(actualData, sizeOfActualData * sizeof(char));
                    if (actualData == NULL) return -1;
                    expectDestuffing = FALSE;
                    continue;
                }
                
                if (dataFrame[i] == ESCAPE_OCTET) {
                    expectDestuffing = TRUE;
                    continue;
                } else {
                    actualData[actualDataIt] = dataFrame[i];
                    actualDataIt++;
                    sizeOfActualData++;
                    actualData = (char*)realloc(actualData, sizeOfActualData * sizeof(char));
                    if (actualData == NULL) return -1;
                }
            }

            // Check BCC2
            // FIXME: What if BCC2 is equal to FLAG?
            char dataAccm = 0x00;

            for (int i = 0; i < sizeOfActualData; i++) {
                dataAccm ^= actualData[i];  // EXOR all the destuffed data bytes
            }

            if (dataAccm != dataFrame[data_bcc2_flag_size - 2]) { // If dataAccm is not the same as BCC2, something went wrong

                char REJ = 0x00;

                if (!controlType) {
                    REJ = CONTROL_REJ0;
                } else {
                    REJ = CONTROL_REJ1;
                }

                char BCC1 = REJ ^ ADDRESS_SENT_BY_TX;

                // SEND NACK
                char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, REJ, BCC1, FLAG};
                int wb = writeBytes(ua_array, 5);
                state = START;

                // TODO: Clean allocated space
                free(dataFrame);
                dataAccm = 0;
                currentDataFrameIt = 0;
                dataFrame = (char *)malloc(sizeof(char)); // The data from the information frame will be stored here.
                
                free(actualData);
                actualData = (char*)malloc(sizeof(char));
                actualDataIt = 0;
                sizeOfActualData = 1;


            } else {

                for (int i = 0; i < actualDataIt; i++) {
                    packet[i] = (unsigned char)actualData[i]; 
                }

                controlType = !controlType; // Frame was correctly received, tell Tx we want the next one.

                sendAck(controlType); // Use normal control type. Not inverted.

                state = START;
                
                state = STOP_STATE;
            }
        }
    }

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    if (role == LlTx) { // Transmitter
        while (alarmCount < numberOfRetransmitions) {
            int bytesWritten = 0;

            if (alarmEnabled == FALSE){
                alarm(timeout); // Set alarm to be triggered after timeout
                alarmEnabled = TRUE;

                // Assemble DISC frame
                int array_size = 5;
                char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_DISC;
                char set_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_DISC, BCC1, FLAG};

                // Send DISC frame
                while (bytesWritten != 5) { // FIXME: Remove hard-coded value?
                    bytesWritten = writeBytes((set_array + sizeof(char) * bytesWritten), array_size - bytesWritten);
                    if (bytesWritten == -1) {
                        return -1;
                    }
                }
                sleep(1); // Wait until all bytes have been written to the serial port
            }

            if (bytesWritten == 5) {
                int csu = checkSUFrame(CONTROL_DISC, &alarmEnabled);
                if (csu == -1) return -1;
                else return 1;   
            }
        }


        // Is this correct ??????????
        while (1) {
            int csu = checkSUFrame(CONTROL_UA, &alarmEnabled);
            if (csu == -1) {
                return -1;
            } else {
                return 1;
            }
        }

    } else if (role == LlRx) { // Receiver
        int enterCheckSUFrame = TRUE;
        while (enterCheckSUFrame) {

            int csu = checkSUFrame(CONTROL_DISC, &alarmEnabled);
            if (csu == -1) {
                return -1;
            }

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_DISC;
            char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_DISC, BCC1, FLAG};

            int wb = writeBytes(ua_array, 5);

            if (wb == -1) {
                return -1;
            } else if (wb == 5) {
                return 1;
            } else {
                continue;
            }
        }
    }

    return 1;

    int clstat = closeSerialPort();
    return clstat;
}
