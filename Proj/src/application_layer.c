// Application layer protocol implementation


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>

#include "application_layer.h"
#include "link_layer.h"

// Definitions for Control Packets
 
#define CtrlPacketStart 1
#define CtrlPacketEnd 3

#define fName "PPinguin.gif" // New filename...

typedef enum {
    cStart,
    cEnd
} ControlPacketType;

// Definitions for Data Packets




// Sender
unsigned char* createControlPacket(unsigned char* controlPacket, int* currentSize, ControlPacketType cpt, long fileSize, unsigned char* fileName) {
    if (controlPacket == NULL) {
        return NULL;
    }

    if (cpt == cStart) {
        controlPacket[0] = CtrlPacketStart; // Dealing with a Start Control Packet.
    } else {
        controlPacket[0] = CtrlPacketEnd; // Dealing with a End Control Packet.
        return controlPacket; // The ending packet is the same as the starting packet. Only difference is the first value.
    }


    // TLV coded long

    printf("\nPlease remove me - DEBUG: In the current architecture long is %d bytes long.\n", sizeof(long));

    char byteData[4];
    byteData[0] = (fileSize >> 24) & 0xFF; // Most significant byte
    byteData[1] = (fileSize >> 16) & 0xFF;
    byteData[2] = (fileSize >> 8) & 0xFF;
    byteData[3] = fileSize & 0xFF; // Least significant byte

    for (int i = 0; i < 4; i++) {

        // Type
        (*currentSize)++;
        controlPacket = (char*)realloc(controlPacket, (*currentSize) * sizeof(char));
        controlPacket[(*currentSize) - 1] = 0; // Filesize

        
        // Length
        (*currentSize)++;
        controlPacket = (char*)realloc(controlPacket, (*currentSize) * sizeof(char));
        controlPacket[(*currentSize) - 1] = 4;

        // Value
        (*currentSize)++;
        controlPacket = (char*)realloc(controlPacket, (*currentSize) * sizeof(char));
        controlPacket[(*currentSize) - 1] = byteData[i];

    }


    // TLV coded filename

    int fileNameSize = (sizeof(fName) / sizeof(char));

    printf("\nPlease remove me - DEBUG: Filename is %d bytes long.\n", fileNameSize);

    for (int i = 0; i < fileNameSize; i++) {

        // Type
        (*currentSize)++;
        controlPacket = (char*)realloc(controlPacket, (*currentSize) * sizeof(char));
        controlPacket[(*currentSize) - 1] = 1; // Filename

        
        // Length
        (*currentSize)++;
        controlPacket = (char*)realloc(controlPacket, (*currentSize) * sizeof(char));
        controlPacket[(*currentSize) - 1] = fileNameSize;

        // Value
        (*currentSize)++;
        controlPacket = (char*)realloc(controlPacket, (*currentSize) * sizeof(char));
        controlPacket[(*currentSize) - 1] = fName[i];

    }

    return controlPacket;
}


// TODO: Continue implemetation of createDataPacket.
unsigned char* createDataPacket(unsigned char* dataPacket, int* currentSize) {

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
        if (bytesWritten == sizeOfPacket) break; 
    }
    return 0;
}

int txApplication(LinkLayer linkStruct, const char* filename) {
    // Open file.
    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        printf("Unable to open file.\n");
        return -1;
    }

    // Get information about the file.
    struct stat st;

    if (stat(12, &st) == -1) {
        printf("Unable to get information about the file.\n");
        return -1;
    }

    long fileSize = st.st_size;

    // Open the connection.
    if (llopen(linkStruct) != 1) {
        printf("%s: An error occoured inside llopen.\n", __func__);
        return -1;
    }

    // Create the initial control packet.
    unsigned char* controlPacket = (unsigned char*)malloc(sizeof(unsigned char));
    int sizeOfControlPacket = 1;

    controlPacket = createControlPacket(controlPacket, &sizeOfControlPacket, cStart, fileSize, fName);

    if (controlPacket == NULL) {
        printf("%s: An error occoured while trying to create the Control Packet.\n", __func__);
        return -1;
    }

    // Send the start control packet.
    if (llwriteWrapper(controlPacket, sizeOfControlPacket) == -1) {
        printf("%s: An error occoured while trying to send the START Control Packet.\n", __func__);
        return -1;
    }

    // TODO: File manipulation
    // TODO: Need to subdivide the file into smaller parts
    // TODO: Send them with the help of llwriteWrapper
    // TODO: Call createDataPacket.

    // Create the the control packet.
    controlPacket = createControlPacket(controlPacket, &sizeOfControlPacket, cEnd, fileSize, fName);

    if (controlPacket == NULL) {
        printf("%s: An error occoured while trying to create the END Control Packet.\n", __func__);
        return -1;
    }

    // Send the end control packet.
    if (llwriteWrapper(controlPacket, sizeOfControlPacket) == -1) {
        printf("%s: An error occoured while trying to send the END Control Packet.\n", __func__);
        return -1;
    }

    if (llclose(TRUE) == -1) { // Call llclose
        printf("%s: An error occoured in llclose.\n", __func__);
        return -1;
    }

    return 0;
}


// Receiver
int rxApplication(LinkLayer linkStruct) {

}


void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {

    LinkLayerRole appRole = LlRx;

    if (strcmp("tx", role) == 0) {
        appRole = LlTx;
    } else if (strcmp("rx", role) == 0) {
        appRole = LlRx;
    } else {
        printf("Please specify a valid role.\n");
        return -1;
    }

    LinkLayer linkStruct = {
        .serialPort = serialPort,
        .role = appRole,
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout
    };



    if (role == LlTx) {
        if (txApplication(linkStruct, filename) == -1) return -1;
    } else {
        if (rxApplication(linkStruct) == -1) return -1;
    }

    return;
}
