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

unsigned char* fName = (unsigned char*)"PPinguin.gif"; // New filename...

/* typedef enum {
    cStart,
    cData,
    cEnd
} ControlPacketType; */
#define CSTART 1
#define CDATA 2
#define CEND 3

// Definitions for Data Packets

#define partitionSize 500

unsigned char sequenceNumber = 0;  // Between 0 and 99



// Sender
/*
    

*/
unsigned char* createControlPacket(unsigned char* controlPacket, int* currentSize, int cpt, long fileSize, unsigned char* fileName) {
    if (controlPacket == NULL) {
        return NULL;
    }

    if (cpt == CSTART) {
        controlPacket[0] = CtrlPacketStart; // Dealing with a Start Control Packet.
    } else {
        controlPacket[0] = CtrlPacketEnd; // Dealing with a End Control Packet.
        return controlPacket; // The ending packet is the same as the starting packet. Only difference is the first value.
    }


    // TLV coded long

    printf("\nPlease remove me - DEBUG: In the current architecture long is %ld bytes long.\n", sizeof(long));

    /* #if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__aarch64__)    

        char byteData[8];
        byteData[0] = (fileSize >> 56) & 0xFF; // Most significant byte
        byteData[1] = (fileSize >> 48) & 0xFF;
        byteData[2] = (fileSize >> 40) & 0xFF;
        byteData[3] = (fileSize >> 32) & 0xFF;
        byteData[4] = (fileSize >> 24) & 0xFF;
        byteData[5] = (fileSize >> 16) & 0xFF;
        byteData[6] = (fileSize >> 8) & 0xFF;
        byteData[7] = fileSize & 0xFF; // Least significant byte

        for (int i = 0; i < 8; i++) {

            // Type
            (*currentSize)++;
            controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
            controlPacket[(*currentSize) - 1] = 0; // Filesize

            
            // Length
            (*currentSize)++;
            controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
            controlPacket[(*currentSize) - 1] = 8;

            // Value
            (*currentSize)++;
            controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
            controlPacket[(*currentSize) - 1] = byteData[i];

        }

    #elif defined(__i386__) || defined(_M_IX86) || defined(__ppc__) || defined(__arm__) */
        char byteData[4];
        byteData[0] = (fileSize >> 24) & 0xFF; // Most significant byte
        byteData[1] = (fileSize >> 16) & 0xFF;
        byteData[2] = (fileSize >> 8) & 0xFF;
        byteData[3] = fileSize & 0xFF; // Least significant byte

        for (int i = 0; i < 4; i++) {
            // Type
            (*currentSize)++;
            controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
            controlPacket[(*currentSize) - 1] = 0; // Filesize
            printf("HEY IM TRYING TO SCHLEEP AAAAAAAAAAA %d\n", ((*currentSize) - 1));
            
            // Length
            (*currentSize)++;
            controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
            controlPacket[(*currentSize) - 1] = 4;

            // Value
            (*currentSize)++;
            controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
            controlPacket[(*currentSize) - 1] = byteData[i];
        }

   /*  #else
        printf("%s: Unknown architecture\n", __func__);
        return -1;
    #endif */


    // TLV coded filename

    int fileNameSize = (sizeof(fName) / sizeof(unsigned char));

    printf("\nPlease remove me - DEBUG: Filename is %d bytes long.\n", fileNameSize);

    for (int i = 0; i < fileNameSize; i++) {

        // Type
        (*currentSize)++;
        controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
        controlPacket[(*currentSize) - 1] = 1; // Filename

        
        // Length
        (*currentSize)++;
        controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
        controlPacket[(*currentSize) - 1] = fileNameSize;

        // Value
        (*currentSize)++;
        controlPacket = (unsigned char*)realloc(controlPacket, (*currentSize) * sizeof(unsigned char));
        controlPacket[(*currentSize) - 1] = fName[i];

    }

    return controlPacket;
}


/*
    Creates a data packet according to the specification
    dataPacket[] - data packet array to be written
    currentSize - size of the data packet to be written
    fd - file descriptor
    returns 1 on ...  // TODO: Is this true?
            0 if no bytes are read (nothing left to read)
           -1 on error
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
    for (int i = 0; i <= partitionSize; i++) {
        int readBytes = read((*fd), &byte, 1);

        if (readBytes == -1) {
            return -1;
        }

        if (readBytes == 0) {
            return 0;
        }

        accumulatorOfBytesRead++;

        (*currentSize)++;
        (*dataPacket) = (unsigned char*)realloc((*dataPacket), (*currentSize) * sizeof(unsigned char));
        (*dataPacket)[(*currentSize) - 1] = byte;
    }

    // K = 256 * L2 + L1
    int L2 = accumulatorOfBytesRead / 256;
    int L1 = accumulatorOfBytesRead - (L2 * 256);

    (*dataPacket)[2] = L2;
    (*dataPacket)[3] = L1;

    sequenceNumber = sequenceNumber == (unsigned char)99 ? 0 : sequenceNumber + 1;
    return 1; // TODO: Should this be 1?
}


// This function only exits when a successful write is done...
// This means that it will only return if we are able to write the full data.
// FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:
// FIXME: This function needs to have a limit......FIXME:
// FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:FIXME:
int llwriteWrapper(unsigned char* packet, int sizeOfPacket) {
    while (1) {
        int bytesWritten = llwrite(packet, sizeOfPacket);
        if (bytesWritten == -1) return -1;
        if (bytesWritten == 0) continue;
        printf("EXPECTED %x BUT BYTES WRITTEN WERE %x\n", sizeOfPacket, bytesWritten);
        if (bytesWritten == sizeOfPacket){
            
            return 0;
        }
    }
    return 0;
}

int txApplication(LinkLayer linkStruct, const char* filename) {
    printf("Tx is actually Tx\n");

    // Open file.
    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        printf("Unable to open file.\n");
        return -1;
    }

    // Get information about the file.
    struct stat st;

    if (stat(filename, &st) == -1) {
        printf("Unable to get information about the file.\n");
        return -1;
    }

    long fileSize = st.st_size;

    // Open the connection.
    if (llopen(linkStruct) != 1) {
        printf("%s: An error occurred inside llopen.\n", __func__);
        return -1;
    }

    printf("TX is connected up successfully\n");


    // Create the initial control packet.
    unsigned char* controlPacket = (unsigned char*)malloc(sizeof(unsigned char));
    int sizeOfControlPacket = 1;

    controlPacket = createControlPacket(controlPacket, &sizeOfControlPacket, CSTART, fileSize, fName);

    if (controlPacket == NULL) {
        printf("%s: An error occurred while trying to create the Control Packet.\n", __func__);
        return -1;
    }

    // Send the start control packet.
    if (llwriteWrapper(controlPacket, sizeOfControlPacket) == -1) {
        printf("%s: An error occurred while trying to send the START Control Packet.\n", __func__);
        return -1;
    }

    int shouldCreateDataPacket = 1;

    // Create data packet
    while (shouldCreateDataPacket) {
    
        unsigned char* dataPacket = (unsigned char*)malloc(sizeof(unsigned char));
        int sizeOfDataPacket = 1;
        shouldCreateDataPacket = createDataPacket(&dataPacket, &sizeOfDataPacket, &fd);

        if (shouldCreateDataPacket == 0) {
            free(dataPacket);
            break;
        }

        if (dataPacket == NULL || shouldCreateDataPacket == -1) {
            printf("%s: An error occurred while trying to create the Data Packet\n", __func__);
            return -1;
        }

        // Send the data packet.
        if (llwriteWrapper(dataPacket, sizeOfDataPacket) == -1) {
            printf("%s: An error occurred while trying to send the START Control Packet.\n", __func__);
            return -1;
        }

        free(dataPacket);
    }
    

    // Create the the END control packet.
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

    if (llclose(TRUE) == -1) { // Call llclose
        printf("%s: An error occurred in llclose.\n", __func__);
        return -1;
    }

    return 0;
}

/*
    Reads and Checks control packets. 
    fileSize - size of the file to be read
    returns 0 on success
           -1 on error
*/
int readControlPacket(long* fileSize, unsigned char* filename[], int type) {
    unsigned char* controlPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
    int bytesRead = llread(controlPacket);
    if (bytesRead == -1){
        printf("%s: Error in llread\n", __func__);
        return -1;
    }
    //for (int i = 0; i < bytesRead; i++) {
    if (controlPacket[0] != type) {
        printf("%s: Error in controlPacketType\n", __func__);
        return -1;
    } // Check if the control packet is correct
    

    printf("\nPlease check the architecture for the size of long\n"); // TODO: Remove me.

    /* #if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__) || defined(__aarch64__)
        char byteData[8]; // This is where the bytes for the long value will be stored.
        char readTLVmax = 8;
        for (char i = 0; i < readTLVmax; i++) {

            // Type
            if (controlPacket[1 + i] != 0) return -1; // We are expecting a filesize...

            
            // Length
            if (((size_t)controlPacket[2 + i]) > sizeof(long) || controlPacket[2 + i] != (unsigned char)readTLVmax) { // Filesize cannot be longer than an long...
                printf("%s: The length value for filesize is invalid.\n", __func__);
                return -1;
            }

            // Value
            byteData[i] = controlPacket[3 + i];

        }
        
        (*fileSize) = 0;

        (*fileSize) |= (unsigned char)byteData[0] << 56;
        (*fileSize) |= (unsigned char)byteData[1] << 48;
        (*fileSize) |= (unsigned char)byteData[2] << 40;
        (*fileSize) |= (unsigned char)byteData[3] << 32;
        (*fileSize) |= (unsigned char)byteData[4] << 24;
        (*fileSize) |= (unsigned char)byteData[5] << 16;
        (*fileSize) |= (unsigned char)byteData[6] << 8;
        (*fileSize) |= (unsigned char)byteData[7];
    #elif defined(__i386__) || defined(_M_IX86) || defined(__ppc__) || defined(__arm__) */
        char byteData[4];
        char readTLVmax = 4;
        for (int i = 0; i < readTLVmax; i++) {

            printf("Value of T(1) is %x\n", controlPacket[1]);
            printf("Value of T(1+i) is %x\n", controlPacket[1+i]);
            // Type
            if (controlPacket[1 + (i * 3)] != 0) {
                printf("%s: Error in Type of TLV.\n", __func__);
                printf("Expected 0 got %d\n", controlPacket[1+i]);
                return -1; // We are expecting a filesize...
            }

            // Length
            if (((size_t)controlPacket[2 + (i * 3)]) > sizeof(long) || controlPacket[2 + (i * 3)] != (unsigned char)readTLVmax) { // Filesize cannot be longer than an long...
                printf("%s: The length value for filesize is invalid.\n", __func__);
                return -1;
            }

            // Value
            byteData[i] = controlPacket[3 + (i * 3)];
        }

        (*fileSize) = 0;

        (*fileSize) |= (unsigned char)byteData[0] << 24;
        (*fileSize) |= (unsigned char)byteData[1] << 16;
        (*fileSize) |= (unsigned char)byteData[2] << 8;
        (*fileSize) |= (unsigned char)byteData[3];
    /* #else
        printf("%s: Unknown architecture\n", __func__);
        return -1;
    #endif */

    free(controlPacket);
    return 0;
}

/*
    // TODO
*/
int readDataPacket(int* fd, long* fileSize, unsigned char* fileName[]) {

    int continueReadingBytes = 1;
    long totalAmountRead = 0;

    while (continueReadingBytes){
        unsigned char* dataPacket = (unsigned char*)malloc(MAX_PAYLOAD_SIZE * sizeof(unsigned char));
        int readBytes = 0;

        printf("WALKING THROUGH THE ROOM\n");

        readBytes = llread(dataPacket);

        printf("HEY! HI! HELLO!\n");

        if (readBytes == 0) continue;

        if (readBytes == -1) {
            printf("%s: An error occurred in llread.\n", __func__);
            return -1;
        }

       /*  if (readBytes == 0) { // FIXME: FIX THIS SHIT
            continueReadingBytes = 0;
            break;
        } */

        if (dataPacket[0] == CEND){

            if (readControlPacket(fileSize, fileName, CEND) != 0) {
                printf("%s: Error in readControlPacket.\n", __func__);
                return -1;
            }

            return 0;
        }

        totalAmountRead += readBytes - 4;

        // TODO: Use S
        int l1 = dataPacket[3];
        int l2 = dataPacket[2];
        int k = 256 * l2 + l1;
        //for (int i = 0; i < k; i++){
        // FIXME: Should we use FILE* with fprintf()?
        int bytesWritten = 0;
        bytesWritten = write((*fd), dataPacket + sizeof(unsigned char) * 4 , k); // FIXME: CHECK FIXME: CHECK FIXME: CHECK FIXME: CHECK FIXME: CHECK 
        //bytesWritten = write((*fd), dataPacket + 4, k); // FIXME: CHECK FIXME: CHECK FIXME: CHECK FIXME: CHECK FIXME: CHECK 

        if (bytesWritten == -1) {
            printf("%s: An error occurred while writing to the file.\n", __func__);
            return -1;
        }
        //}
    
    }

    // Pass fd
    // Loop with llread()
    // Take output from llread() and write to file with a for loop

    return totalAmountRead;
}



// Receiver
int rxApplication(LinkLayer linkStruct) {

    // Open the connection.
    if (llopen(linkStruct) != 1) {
        printf("%s: An error occurred inside llopen.\n", __func__);
        return -1;
    }

    printf("RX is connected up successfully\n");

    // Read the start control packet.
    long fileSize;
    unsigned char* fileName = (unsigned char*)malloc(sizeof(unsigned char) * 10); //FIXME: DEFINE A MAXIMUM SIZE FOR FILENAME. ALSO... DOES THIS WORK ? ** || *[]
    if (readControlPacket(&fileSize, &fileName, CSTART) != 0) {
        printf("%s: Error in readControlPacket.\n", __func__);
        return -1;
    }
    
    
    // Create file.
    int fd = open("XiaomiPinguin.gif", O_CREAT | O_RDWR); // FIXME: Use filename........

    if (fd < 0) {
        printf("Unable to open file.\n");
        return -1;
    }

    // Read Data Packets
    if (readDataPacket(&fd, &fileSize, &fileName) < 0) {
        printf("%s: Error while reading data packet.\n", __func__);
        return -1;
    }

    return 0;
}


void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {

    LinkLayerRole appRole = LlRx;

    if (strcmp("tx", role) == 0) {
        appRole = LlTx;
    } else if (strcmp("rx", role) == 0) {
        appRole = LlRx;
    } else {
        printf("Please specify a valid role.\n");
        return;
    }

    // Constructing the struct
    LinkLayer linkStruct = {
        .role = appRole,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };
    strcpy(linkStruct.serialPort, serialPort);

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
