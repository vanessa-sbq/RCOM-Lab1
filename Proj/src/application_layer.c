// Application layer protocol implementation
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "application_layer.h"
#include "link_layer.h"

// Definitions for Control Packets
#define CtrlPacketStart 1
#define CtrlPacketEnd 3

unsigned char* fName = (unsigned char*)"Penguin.gif"; 

#define CSTART 1
#define CDATA 2
#define CEND 3

// Definitions for Data Packets
#define partitionSize 996
unsigned char sequenceNumber = 0;  // Between 0 and 99


/**
 * Creates a control packet (start or end)
 * controlPacket - array to which the packet is written to
 * currentSize - current size of the control packet (while it is being written to)
 * cpt - should have values CSTART or CEND (start or end control packet)
 * fileSize - size of the file to be sent
 * fileName - name of the file to be sent
 * returns a controlPacket on success 
 *         NULL on error
*/
unsigned char* createControlPacket(unsigned char* controlPacket, int* currentSize, int cpt, long fileSize, unsigned char* fileName) {
    if (controlPacket == NULL) return NULL;
    
    if (cpt == CSTART) controlPacket[0] = CSTART; // Dealing with a Start Control Packet.
    else {
        controlPacket[0] = CEND; // Dealing with a End Control Packet.
        return controlPacket; // The ending packet is the same as the starting packet. Only difference is the first value.
    }

    // TLV coded long
    printf("\nPlease remove me - DEBUG: In the current architecture long is %ld bytes long.\n", sizeof(long)); // TODO: Remove (DEBUG)

    char byteData[4];
    byteData[0] = (fileSize >> 24) & 0xFF; // Most significant byte
    byteData[1] = (fileSize >> 16) & 0xFF;
    byteData[2] = (fileSize >> 8) & 0xFF;
    byteData[3] = fileSize & 0xFF; // Least significant byte

    // Type 
    (*currentSize)++;
    controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
    controlPacket[(*currentSize) - 1] = 0; // Filesize

    // Length
    (*currentSize)++;
    controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
    controlPacket[(*currentSize) - 1] = 4;

    for (int i = 0; i < 4; i++) {
        // Value
        (*currentSize)++;
        controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
        controlPacket[(*currentSize) - 1] = byteData[i];
    }

    // TLV coded filename
    int fileNameSize = (int)strlen((const char*) fName);

    // Type
    (*currentSize)++;
    controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
    controlPacket[(*currentSize) - 1] = 1; // Filename
    
    // Length
    (*currentSize)++;
    controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
    controlPacket[(*currentSize) - 1] = fileNameSize;

    printf("\nPlease remove me - DEBUG: Filename is %d bytes long.\n", fileNameSize); // TODO: Remove (DEBUG)
    for (int i = 0; i < fileNameSize; i++) {
        
        // Value
        (*currentSize)++;
        controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
        controlPacket[(*currentSize) - 1] = fName[i];
    }
    return controlPacket;
}


/**
 * Creates a data packet according to the specification
 * dataPacket[] - data packet array to be written
 * currentSize - size of the data packet to be written
 * fd - file descriptor
 * returns 1 on ...  // TODO: Is this true?
 *         0 if no bytes are read (nothing left to read)
 *        -1 on error
*/
int createDataPacket(unsigned char* dataPacket[], int* currentSize, int* fd) {
    unsigned int accumulatorOfBytesRead = 0;
    unsigned char byte = 0x00;

    (*dataPacket)[0] = CDATA; // Control Data

    // Sequence Number
    (*currentSize)++;
    (*dataPacket) = (unsigned char*)realloc((*dataPacket), (*currentSize) * sizeof(unsigned char));
    (*dataPacket)[(*currentSize) - 1] = sequenceNumber;

    // L2
    (*currentSize)++;
    (*dataPacket) = (unsigned char*)realloc((*dataPacket), (*currentSize) * sizeof(unsigned char));
    (*dataPacket)[(*currentSize) - 1] = sequenceNumber; // TODO: Deixar assim?

    // L1
    (*currentSize)++;
    (*dataPacket) = (unsigned char*)realloc((*dataPacket), (*currentSize) * sizeof(unsigned char));
    (*dataPacket)[(*currentSize) - 1] = sequenceNumber; // TODO: Deixar assim?
    
    // Need to subdivide the file into smaller parts (Each packet has data with partitionSize bytes)
    for (int i = 0; i < partitionSize; i++) {
        int readBytes = read((*fd), &byte, 1);
        if (readBytes == -1) return -1;
        if (readBytes == 0) break;

        accumulatorOfBytesRead++;
 
        (*currentSize)++;
        (*dataPacket) = (unsigned char*)realloc((*dataPacket), (*currentSize) * sizeof(unsigned char));
        (*dataPacket)[(*currentSize) - 1] = byte;
    }

    // K = 256 * L2 + L1
    int L2 = accumulatorOfBytesRead / 256;
    int L1 = accumulatorOfBytesRead - (L2 * 256);

    if (L2 == 0 && L1 == 0) return 0;

    (*dataPacket)[2] = L2;
    (*dataPacket)[3] = L1;

    sequenceNumber = sequenceNumber == (unsigned char)99 ? 0 : sequenceNumber + 1;
    return 1; // TODO: Should this be 1?
}


// FIXME: This function needs to have a limit
/**
 * This function only exits when a successful write is done.
 * This means that it will only return if we are able to write the full data.
 * packet - packet to be sent
 * sizeOfPacket - size of packet to be sent
 * returns 0 on success
 *        -1 on error
*/
int llwriteWrapper(unsigned char* packet, int sizeOfPacket) {
    while (1) {
        int bytesWritten = llwrite(packet, sizeOfPacket);
        if (bytesWritten == -1) return -1;
        if (bytesWritten == 0) continue;
        printf("EXPECTED %x BUT BYTES WRITTEN WERE %x\n", sizeOfPacket, bytesWritten);
        if (bytesWritten == sizeOfPacket) return 0;
    }
    return 0;
}


/**
 * Main application function for transmitter.
 * linkStruct - struct that contains information about the transmitter
 * filename - name of the file to be sent
 * returns 0 on success
 *        -1 on error
*/
int txApplication(LinkLayer linkStruct, const char* filename) {
    // Open file
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Unable to open file.\n");
        return -1;
    }

    // Get information about the file
    struct stat st;
    if (stat(filename, &st) == -1) {
        printf("Unable to get information about the file.\n");
        return -1;
    }
    long fileSize = st.st_size;

    // Open the connection
    if (llopen(linkStruct) != 1) {
        printf("%s: An error occurred inside llopen.\n", __func__);
        return -1;
    }

    printf("TX is connected up successfully\n");  // TODO: Remove (DEBUG)

    // Create the initial control packet
    unsigned char* controlPacket = (unsigned char*)malloc(sizeof(unsigned char));
    int sizeOfControlPacket = 1;
    controlPacket = createControlPacket(controlPacket, &sizeOfControlPacket, CSTART, fileSize, fName);
    if (controlPacket == NULL) {
        printf("%s: An error occurred while trying to create the Control Packet.\n", __func__);
        return -1;
    }

    // Send the start control packet
    if (llwriteWrapper(controlPacket, sizeOfControlPacket) == -1) {
        printf("%s: An error occurred while trying to send the START Control Packet.\n", __func__);
        return -1;
    }

    // Create data packet
    int shouldCreateDataPacket = TRUE;
    while (shouldCreateDataPacket) {
        unsigned char* dataPacket = (unsigned char*)malloc(sizeof(unsigned char));
        int sizeOfDataPacket = 1;
        shouldCreateDataPacket = createDataPacket(&dataPacket, &sizeOfDataPacket, &fd);

        if (shouldCreateDataPacket == 0) { // Nothing left to send
            free(dataPacket);
            break;
        }

        if (dataPacket == NULL || shouldCreateDataPacket == -1) {
            printf("%s: An error occurred while trying to create the Data Packet\n", __func__);
            return -1;
        }

        // Send the data packet
        if (llwriteWrapper(dataPacket, sizeOfDataPacket) == -1) {
            printf("%s: An error occurred while trying to send the START Control Packet.\n", __func__);
            return -1;
        }

        free(dataPacket);
    }
    
    // Create the the end control packet
    controlPacket = createControlPacket(controlPacket, &sizeOfControlPacket, CEND, fileSize, fName);
    if (controlPacket == NULL) {
        printf("%s: An error occurred while trying to create the END Control Packet.\n", __func__);
        return -1;
    }

    // Send the end control packet.
    if (llwriteWrapper(controlPacket, sizeOfControlPacket) == -1) {
        printf("%s: An error occurred while trying to send the END Control Packet.\n", __func__);
        return -1;
    }

    free(controlPacket);

    // Close connection
    if (llclose(TRUE) == -1) {
        printf("%s: An error occurred in llclose.\n", __func__);
        return -1;
    }

    return 0;
}


/**
 * Reads and Checks control packets. 
 * fileSize - size of the file to be read
 * returns 0 on success
 *        -1 on error
*/
int readControlPacket(unsigned char* controlPacket, long* fileSize, unsigned char* filename, int type) {
    //unsigned char* controlPacket1 = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char)); // TODO: Remove 

    if (type != CEND) {
        int bytesRead = llread(controlPacket);
        if (bytesRead == -1){
            printf("%s: Error in llread\n", __func__);
            return -1;
        }
    }

    printf("controlPacket[0]: %d\n", (controlPacket[0]));
    printf("controlPacket[1]: %d\n", (controlPacket[1]));
    printf("controlPacket[2]: %d\n", (controlPacket[2]));

    //if (bytesRead == 0) printf("llread returned zero\n"); TODO: Remove me

    // Check if the control packet is correct
    if ((controlPacket[0]) != type) {
        printf("%s: Error in controlPacketType\n", __func__);
        printf("Expected %x but got %d\n", type, controlPacket[0]);
        return -1;
    } 
    
    //printf("\nPlease check the architecture for the size of long\n"); // TODO: Remove (DEBUG)

    // Check Filesize
    // Type
    if ((controlPacket[1]) != 0) {
            printf("%s: Error in Type of TLV.\n", __func__);
            printf("Expected 0 got %d\n", controlPacket[1]);
            return -1; // We are expecting a filesize...
    }

    // Length
    char readTLVmax = 4; // TODO: Make it work for other architectures too
    char byteData[4];
    if (controlPacket[2] != (unsigned char)readTLVmax) { // Filesize cannot be longer than an long...
        printf("%s: The length value for filesize is invalid.\n", __func__);
        return -1;
    }
    
    int offset = 3;

    // Value
    for (int i = 0; i < readTLVmax; i++) {
        byteData[i] = controlPacket[3 + i];
        offset++;
    }

    (*fileSize) = 0;
    (*fileSize) |= (unsigned char)byteData[0] << 24;
    (*fileSize) |= (unsigned char)byteData[1] << 16;
    (*fileSize) |= (unsigned char)byteData[2] << 8;
    (*fileSize) |= (unsigned char)byteData[3];
    
    // Check Filename
    // Type
    if (controlPacket[offset] != 1) {
            printf("%s: Error in Type of TLV.\n", __func__);
            printf("Expected 1 got %d\n", controlPacket[1]);
            return -1; // We are expecting a filesize...
    }

    // Length
    offset++;
    int filenameSize = controlPacket[offset];

    filename = (unsigned char*)realloc(filename, filenameSize * sizeof(unsigned char));

    if (filenameSize < 1) {
        printf("%s: Error in controlPacket, filenameSize is less than one\n", __func__);
        return -1;
    }

    // Value
    offset++;
    for (int i = 0; i < filenameSize; i++) { 
        filename[i] = controlPacket[offset + i]; 
    }
    filename[filenameSize] = '\0'; // FIXME: Do we need to add '\0'
    
    return 0;
}


/**
 * Reads, checks a data packet and writes contents to a new file.
 * fd - file descriptor of the new file
 * fileSize - size of the new file
 * fileName - name of the received file
 * returns number of bytes read on success
 *        -1 on error
*/
int readDataPacket(int* fd, long* fileSize, unsigned char* fileName) {
    int continueReadingBytes = 1;
    long totalAmountRead = 0;
    while (continueReadingBytes){
        unsigned char* dataPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
        int readBytes = 0;

        //printf("WALKING THROUGH THE ROOM\n");  // TODO: Remove (DEBUG)
        readBytes = llread(dataPacket);
        //printf("HEY! HI! HELLO!\n"); // TODO: Remove (DEBUG)

        if (readBytes == 0) continue; // Nothing left to read
        if (readBytes == -1) {
            printf("%s: An error occurred in llread.\n", __func__);
            return -1;
        }

        printf("Packet type: %d\n", dataPacket[0]);
        if (dataPacket[0] == CEND){
            printf("Calling readControlPacket.\n"); // TODO: Remove (DEBUG)
            if (readControlPacket(dataPacket, fileSize, fileName, CEND) != 0) {
                printf("%s: Error in readControlPacket.\n", __func__);
                return -1;
            }
            return 0;
        }

        totalAmountRead += readBytes - 4;
 
        // TODO: Use S (serial number)

        int l1 = dataPacket[3];
        int l2 = dataPacket[2];
        int k = 256 * l2 + l1;

        if (k == 0 && dataPacket[0] == CDATA) return 0;

        printf("k: %d\n", k); // TODO: Remove (DEBUG)
        int bytesWritten = 0;
        //bytesWritten = write((*fd), dataPacket + sizeof(unsigned char) * 4 , k); // FIXME: Is this correct?
        bytesWritten = write((*fd), dataPacket + 4, k); // FIXME: Should it be like this?

        if (bytesWritten == -1) {
            printf("%s: An error occurred while writing to the file.\n", __func__);
            return -1;
        }

        free(dataPacket);
    }
    return totalAmountRead;
}


/**
 * Main application function for receiver.
 * linkStruct - struct that contains information about the receiver
 * returns 0 on success
 *        -1 on error
*/
int rxApplication(LinkLayer linkStruct) {
    // Open the connection
    if (llopen(linkStruct) != 1) {
        printf("%s: An error occurred inside llopen.\n", __func__);
        return -1;
    }

    //printf("RX is connected up successfully\n"); // TODO: Remove (DEBUG)

    // Read the start control packet
    unsigned char* controlPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
    long fileSize;
    unsigned char* fileName = (unsigned char*)malloc(sizeof(unsigned char)); //FIXME: Define a maximum size for filename. Also... Does this work? ** || *[]
    if (readControlPacket(controlPacket, &fileSize, fileName, CSTART) != 0) { 
        printf("%s: Error in readControlPacket.\n", __func__);
        return -1;
    }
    
    // Create file
    int fd = open(fileName, O_WRONLY | O_APPEND | O_CREAT, 777); // FIXME: Use filename
    if (fd < 0) {
        printf("Unable to open file.\n");
        return -1;
    }

    // Read data packets
    if (readDataPacket(&fd, &fileSize, fileName) < 0) {
        printf("%s: Error while reading data packet.\n", __func__);
        return -1;
    }

    free(controlPacket);
    //printf("After freeing controlPacket\n");


    // Close the connection
    if (llclose(1) != 1){ // TODO: Change the argument
        printf("%s: An error ocurred inside llclose.\n", __func__);
        return -1;
    }

    return 0;
}


/**
 * Main application layer function, that calls different functions depending on role.
 * serialPort - serial port path
 * role - tx or rx
 * baudRate - speed of the transmission
 * nTries - number of retransmissions in case of failure
 * timeout - timer value for timeouts
 * filename - name of the file to be sent
*/
void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayerRole appRole = LlRx;
    if (strcmp("tx", role) == 0) appRole = LlTx;
    else if (strcmp("rx", role) == 0) appRole = LlRx;
    else {
        printf("Please specify a valid role.\n");
        return;
    }

    // Creating the struct
    LinkLayer linkStruct = {
        .role = appRole,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };
    strcpy(linkStruct.serialPort, serialPort);

    // Call function depending on role
    if (appRole == LlTx) {
        if (txApplication((linkStruct), filename) == -1) {
            printf("%s, Error in txApplication.\n", __func__);
        }
    } else {
        if (rxApplication((linkStruct)) == -1){
            printf("%s, Error in rxApplication.\n", __func__);
        }
    }
}
