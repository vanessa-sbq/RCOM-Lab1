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

// Previous C Field (for rx)
static int prevCField = 1; 

// C Field to send next (for tx)
static int CFieldToSendNext = 0;

// Stastics
unsigned long totalNumOfFrames = 0;
unsigned long totalNumOfValidFrames = 0;
unsigned long totalNumOfInvalidFrames = 0;
unsigned long totalNumOfDuplicateFrames = 0;
unsigned long totalNumOfRetransmitions = 0;
unsigned long totalNumOfTimeouts = 0;


// Handler
int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}


/**
 * Supervision frames and Unnumbered frames state machine 
 * controlField - control field to be checked (depending on the frame type)
 * ringringEnabled - Flag (because both tx and rx use this function)
 * returns 0 on success
 *        -1 on error
*/
int checkSUFrame(char controlField, int* ringringEnabled){
    state_t state = START;
    while (state != STOP_STATE && (*ringringEnabled)) {
        //printf("DA BLUETOOS DEVICE IS LEADY TO PAIL\n"); // TODO: Remove (DEBUG)
        char byte = 0;
        int rb = 0;
        if ((rb = readByte(&byte)) == -1) return -1;
        if (rb == 0) continue;

        if (byte != 0x00) printf("byte: %.8x\n", byte);
        char BCC1 = 0x00;

        switch (state) {
            case START:
                if (byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                break;
            case A_RCV:
                state = byte == FLAG ? FLAG_RCV : (byte == controlField ? C_RCV : START);
                break;
            case C_RCV:
                BCC1 = ADDRESS_SENT_BY_TX ^ controlField;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                state = byte == FLAG ? STOP_STATE : START;
                break;
            case STOP_STATE:
                printf("SET read successfully\n");
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
/**
 * Function that opens the connection between tx and rx
 * connectionParameters - connection parameters (about tx or rx) 
 * returns 1 on success
 *        -1 on error
*/
int llopen(LinkLayer connectionParameters) {
    signal(SIGALRM, alarmHandler);
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
                // Assemble SET frame
                char BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_SET;
                char set_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_SET, BCC1, FLAG};

                // Send SET frame
                bytesWritten = writeBytes(set_array, 5);

                if (bytesWritten == -1) {
                    printf("%s: Error in writeBytes.\n", __func__);
                    return -1;
                }

                alarm(timeout); // Set alarm to be triggered after timeout
                alarmEnabled = TRUE;
                
                printf("TX just wrote\n"); // TODO: Remove (DEBUG)
            }

            if (bytesWritten == 5) {
                alarm(timeout); // Set alarm to be triggered after timeout
                alarmEnabled = TRUE;
                int csu = checkSUFrame(CONTROL_UA, &alarmEnabled);
                if (csu == -1) return -1;
                else if (alarmEnabled){
                    printf("HERE ALO ALO AQUI AQUI\n"); // TODO: Remove (DEBUG)
                    alarm(0);
                    alarmEnabled = FALSE;
                    alarmCount = 0;
                    return 1;   
                } 
            }
            totalNumOfRetransmitions++;
        }
    } else if (role == LlRx) { // Receiver
        int enterCheckSUFrame = TRUE;
        while (enterCheckSUFrame) {
            int csu = checkSUFrame(CONTROL_SET, &enterCheckSUFrame);

            if (csu == -1) {
                printf("%s: An error occurred inside checkSUFrame.\n", __func__);
                return -1;
            }

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_UA;
            char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_UA, BCC1, FLAG};

            int wb = writeBytes(ua_array, 5);

            if (wb == -1) {
                printf("%s: An error occurred inside writeBytes.\n", __func__);
                return -1;
            } 
            else if (wb == 5) return 1;
            else continue;
            totalNumOfRetransmitions++; // TODO: Should this be here?
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
/**
 * Reads an I frame response (ACK or NACK) to determine which frame to send next
 * returns 0 on valid frame
 *         1 on invalid frame
 *        -1 on error
*/
int readIFrameResponse() {
    printf("NOW READING IFRAMERESPOSNE\n"); // TODO: Remove (DEBUG)

    state_t state = START;
    char BCC1 = 0x00;
    char byte;
    int isInvalid = 0;

    while (state != STOP_STATE && alarmEnabled) {
        int rb = readByte(&byte);
        if (rb == -1) return -1;
        if (rb == 0) continue;
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
                        printf("Received RR0\n");
                        totalNumOfValidFrames++;
                        state = C_RCV;
                        CFieldToSendNext = 0;
                        break;
                    case CONTROL_RR1:
                        printf("Received RR1\n");
                        totalNumOfValidFrames++;
                        state = C_RCV;
                        CFieldToSendNext = 1;
                        break;
                    case CONTROL_REJ0:
                        printf("Received REJ0\n");
                        totalNumOfInvalidFrames++;
                        isInvalid = 1;
                        state = C_RCV;
                        break;
                    case CONTROL_REJ1:
                        printf("Received REJ1\n");
                        totalNumOfInvalidFrames++;
                        isInvalid = 1;
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
                //printf("NOW IN CRCV\n"); // TODO: Remove (DEBUG)
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                //printf("NOW IN BCCOK WITH BYTE BEING %x\n", byte); // TODO: Remove (DEBUG)
                state = byte == FLAG ? STOP_STATE : START;
                break;
            case STOP_STATE:
                //printf("STOP_STATE\n"); // TODO: Remove (DEBUG)
                break;
            default:
                state = START;
                break;
        }
    }
    return isInvalid;
}

/**
 * Function that tx uses to write frames to the serial port 
 * buf - frame to write to serial port (before byte stuffing)
 * bufSize - Size of the frame to write to serial port (before byte stuffing)
 * returns number of data bytes written (without byte stuffing) on success
 *        -1 on error
*/
int llwrite(const unsigned char *buf, int bufSize) {
    if (bufSize < 0 || buf == NULL) return -1;    
    
    int newFrameSize = bufSize + 6;
    int numBytesStuffed = 0;
    for (int i = 0; i < bufSize; i++) { // Get the new frame size for byte stuffing
        if (buf[i] == FLAG || buf[i] == ESCAPE_OCTET) {
            printf("FOUND A FLAG INSIDE DATA\n");
            numBytesStuffed++;
            newFrameSize++;
        }
    }

    char* frame = (char*)malloc(sizeof(char) * (newFrameSize));

    frame[0] = FLAG; 
    frame[1] = ADDRESS_SENT_BY_TX;

    if (CFieldToSendNext) frame[2] = 0x80; // Send frame 1 next
    else frame[2] = 0x00; // Send frame 0 next
    //if (!controlType) frame[2] = 0x00;
    //else frame[2] = 0x80;
    
    frame[3] = frame[1] ^ frame[2];

    // Byte Stuffing
    int j = 0;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESCAPE_OCTET){
            frame[4 + j] = ESCAPE_OCTET;
            j++;
            frame[4 + j] = buf[i] ^ ESCAPE_XOR; // Do the XOR
        } else {
            frame[4 + j] = buf[i];
        }
        j++;
    }

    printf("\n DEBUG: Now doing byte stuffing part. -> Before: "); // TODO: Remove (DEBUG)
    for (int i = 0; i < bufSize; i++) {
        printf("%x ", buf[i]);
    }

    printf("\n After Byte Stuffing: "); // TODO: Remove (DEBUG)
    for (int i = 0; i < newFrameSize; i++) {
        printf("%.2x ", frame[i]);
    }

    printf("\n"); // TODO: Remove (DEBUG)

    char BCC2 = buf[0];
    for (int j = 1; j < bufSize; j++) {
        BCC2 ^= buf[j]; 
    }

    // BCC2 byte stuffing
    if (BCC2 == FLAG || BCC2 == ESCAPE_OCTET) {
        printf("OH NO, BCC2 is a flag, how sad :(\n");
        frame = (char*)realloc(frame, newFrameSize + sizeof(char) * 2);
        newFrameSize++;
        frame[newFrameSize - 3] = ESCAPE_OCTET;
        frame[newFrameSize - 2] = BCC2 ^ ESCAPE_XOR;
        numBytesStuffed++;
    } else {
        frame[newFrameSize - 2] = BCC2;
    }

    frame[newFrameSize - 1] = FLAG;    

    int retransmitionCounter = 0;
    int wb = 0;
    alarmCount = 0;
    alarmEnabled = FALSE;
    while (retransmitionCounter < numberOfRetransmitions) {  // TODO: Change condition
        if (alarmEnabled == FALSE){
            printf("TX IS WRITING\n");
            wb = writeBytes(frame, newFrameSize);
            totalNumOfFrames++;
            alarm(timeout); // Set alarm
            alarmEnabled = TRUE;

            if (wb == -1) {
                return -1;
            } else if (wb == newFrameSize) {
                int response = 0;
                if ((response = readIFrameResponse()) == -1) {
                    printf("%s: An error occured in readIFrameResponse.\n", __func__);
                    return -1;
                }
                if (response == 0) {
                    alarm(0);
                    alarmEnabled = FALSE;
                    alarmCount = 0;
                    break;
                }
            }

            retransmitionCounter++;
            totalNumOfRetransmitions++;
        }
        //sleep(1); // Guarantee that all bytes were written. // TODO: Remove
    }

    free(frame);

    // Return number of writer characters
    return (wb - 6 - numBytesStuffed);
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
/**
 * Helper function that sends an ACK to Tx.
 * The function should get the values that is inside the frame and use it to respond to Tx.
 * 
 * This way if Tx sends frame 0, Rx should tell Tx that it wants frame 1.
 * 
 * returns void
 * 
*/
void sendAck(char receivedCField) {
    printf("Expected: %x, actual: %x \n", prevCField, receivedCField);

    char prevCFieldChar = prevCField ? I_FRAME_0 : I_FRAME_1;
    char RR = 0x00;
    if (receivedCField == prevCFieldChar){
        RR = prevCFieldChar;
    }
    else {
        if (receivedCField == I_FRAME_0) {
            RR = CONTROL_RR1;
            prevCField = 0;
        }
        else {
            RR = CONTROL_RR0;
            prevCField = 1;
        }
    }

    // Conditions
    char BCC1 = RR ^ ADDRESS_SENT_BY_TX;

    // Send ACK
    char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, RR, BCC1, FLAG};
    writeBytes(ua_array, 5);  // TODO: Check errors
}

/**
 * Function that rx uses to read frames from the serial port
 * packet - buffer to read the frame data into
 * returns number of data bytes read on success
 *        -1 on error
*/
int llread(unsigned char *packet) {
    if (packet == NULL) return -1;

    int state = START;
    char* dataFrame = (char *)malloc(sizeof(char)); // The data from the information frame will be stored here.
    int currentDataFrameIt = 0;
    char receivedCField = 0x00;

    while (state != STOP_STATE) {
        char byte = 0;
        int rb = 0;
        if ((rb = readByte(&byte)) == -1) return -1;
        if (rb == 0) continue;

        char BCC1 = 0x00; 
    
        switch (state) {
            case START:
                //printf("HERE START\n"); // TODO: Remove (DEBUG)
                if (byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                //printf("HERE IN FLAG_RCV\n"); // TODO: Remove (DEBUG)
                state = byte == FLAG ? FLAG_RCV : (byte == ADDRESS_SENT_BY_TX ? A_RCV : START);
                break;
            case A_RCV:
                //printf("HERE IN ARCV\n"); // TODO: Remove (DEBUG)
                receivedCField = byte;
                printf("Received C Field: %x\n", receivedCField); // TODO: Remove (DEBUG)
                if (byte == FLAG) state = FLAG_RCV;
                else if ((byte == I_FRAME_0) || (byte == I_FRAME_1)) state = C_RCV;
                else state = START;
                break;
            case C_RCV:
                //printf("HERE IN CRCV\n"); // TODO: Remove (DEBUG)
                BCC1 = ADDRESS_SENT_BY_TX ^ receivedCField;
                state = byte == FLAG ? FLAG_RCV : (byte == BCC1 ? BCC_OK : START);
                break;
            case BCC_OK:
                //printf("HERE IN BCCOK\n"); // TODO: Remove (DEBUG)
                dataFrame[currentDataFrameIt] = byte;
                currentDataFrameIt++;
                dataFrame = (char*)realloc(dataFrame, (currentDataFrameIt + 1) * sizeof(char));

                if (dataFrame == NULL) return -1;

                // When byte is equal to flag -> Stop
                if (byte == FLAG){
                    printf("FOUND FLAG FOUND FLAGGGGGGG\n"); // TODO: Remove (DEBUG)
                    state = CHECK_DATA;
                } 

                
        }

        if (state == CHECK_DATA) {
            int data_bcc2_flag_size = currentDataFrameIt;
            char* actualData = (char*)malloc(sizeof(char));
            int actualDataIt = 0;
            int sizeOfActualData = 1;
            int expectDestuffing = FALSE;
            for (int i = 0; i < (data_bcc2_flag_size - 1); i++) { // Byte destuffing // TODO: Change -2 to -1, after BCC2 stuffing

                if (actualDataIt != 0) {
                    sizeOfActualData++;
                    actualData = (char*)realloc(actualData, sizeOfActualData * sizeof(char));
                }

                if (dataFrame[i] != ESCAPE_OCTET) {
                    actualData[actualDataIt] = dataFrame[i];
                    actualDataIt++;
                } else {
                    // We know that the current byte is not to be added
                    // And we take the next byte, xor it and add it to the actualData
                    if (i + 1 > (data_bcc2_flag_size - 1)) {
                        printf("%s: An error occurred inside the byte destuffing.\n", __func__);
                        return -1;
                    }
                    i++;
                    actualData[actualDataIt] = (dataFrame[i] ^ ESCAPE_XOR);
                    actualDataIt++;
                }

               // 1 - We have a byte that is the escape octet
               // 2 - We have a byte that is not the escape octet
            }

            // Check BCC2
            char dataAccm = 0x00;

            for (int i = 0; i < sizeOfActualData-1; i++) {
                dataAccm ^= actualData[i];  // EXOR all the destuffed data bytes
            }

            // Case - XOR is invalid or the data is too big (Reject)
            printf("sizeOFActualdata: %d\n", sizeOfActualData);
            if (dataAccm != actualData[actualDataIt-1] || (sizeOfActualData-1) > MAX_PAYLOAD_SIZE) { // If dataAccm is not the same as BCC2, something went wrong
                printf("XOR INVALID, EXPECTED %.2x BUT GOT %.2x.\n", actualData[actualDataIt-1], dataAccm); // TODO: Remove (DEBUG)

                char REJ = 0x00;
                if (prevCField == 0) REJ = CONTROL_REJ0;
                else REJ = CONTROL_REJ1;

                char BCC1 = REJ ^ ADDRESS_SENT_BY_TX; 

                // SEND NACK
                char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, REJ, BCC1, FLAG};
                writeBytes(ua_array, 5); // TODO: Check errors
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
                totalNumOfFrames++;
                totalNumOfInvalidFrames++;
            } else { 
                // Case - Frame is a duplicate (Accept and discard)
                if (prevCField == receivedCField){
                    printf("Frame is duplicate\n");
                    sendAck(receivedCField); 
                    free(dataFrame);
                    free(actualData);
                    totalNumOfFrames++;
                    totalNumOfDuplicateFrames++;
                    return 0;
                }

                // Case - Frame accepted (Accept)
                for (int i = 0; i < sizeOfActualData; i++) {
                    //printf("Bytes are %.2x\n",(unsigned char)actualData[i] ); // TODO: Remove (DEBUG)
                    //if (i > 3) printf("%.2x ",(unsigned char)actualData[i] ); // TODO: Remove (DEBUG)
                    packet[i] = (unsigned char)actualData[i]; 
                }
                sendAck(receivedCField); 
                printf("Frame accepted\n");
                totalNumOfFrames++;
                totalNumOfValidFrames++;
                return sizeOfActualData;
            }
            // 3 - Reads same frame again (prevCField + discard frame)
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) { // FIXME: What to do with showStatistics??
    printf("Statistics:\n");
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
                //sleep(1); // Wait until all bytes have been written to the serial port // TODO: Remove
            }

            if (bytesWritten == 5) { //FIXME: DISABLE ALARM
                int csu = checkSUFrame(CONTROL_DISC, &alarmEnabled);
                if (csu == -1) return -1;
                else break;   
            }
            totalNumOfRetransmitions++;
        }
        printf("Number of dropped packets (TX): %ld\n", ((int)totalNumOfFrames) - ((int)(totalNumOfValidFrames)) - ((int)(totalNumOfInvalidFrames)));
    } else if (role == LlRx) { // Receiver
        printf("RX entered llclose()\n");
        int enterCheckSUFrame = TRUE;
        while (enterCheckSUFrame) {

            int csu = checkSUFrame(CONTROL_DISC, &enterCheckSUFrame);
            if (csu == -1) {
                return -1;
            }

            int BCC1 = ADDRESS_SENT_BY_TX ^ CONTROL_DISC;
            char ua_array[5] = {FLAG, ADDRESS_SENT_BY_TX, CONTROL_DISC, BCC1, FLAG};

            int wb = writeBytes(ua_array, 5);

            if (wb == -1) return -1;
            else if (wb == 5) {
                printf("Number of dropped packets (RX): %ld\n", ((int)totalNumOfFrames) - ((int)(totalNumOfValidFrames)) - ((int)(totalNumOfInvalidFrames)) - ((int)(totalNumOfDuplicateFrames)));
                printf("Number of frames received that were duplicate: %ld\n", totalNumOfDuplicateFrames);
                break;
            }
            else continue;
        }
    }    
   
    printf("Number of frames that were sent/received and are valid: %ld\n", totalNumOfValidFrames);
    printf("Number of frames that were sent/recevived and are invalid: %ld\n", totalNumOfInvalidFrames);
    printf("Total number of frames that were sent/received: %ld\n", totalNumOfFrames);

    if (closeSerialPort() == -1){
        printf("%s: Error while closing serial port\n", __func__);
        return -1;
    }
    return 1;
}
