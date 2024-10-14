#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>



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

void ByteStuffer(unsigned char* buf, int bufSize) {

    int newFrameSize = bufSize + 6;
    for (int i = 0; i < bufSize; i++) { // Get the new frame size for byte stuffing
        if (buf[i] == FLAG || buf[i] == ESCAPE_OCTET) newFrameSize++;
    }

    unsigned char* frame = (unsigned char*)malloc(sizeof(unsigned char) * (newFrameSize));

    frame[0] = FLAG; 
    frame[1] = ADDRESS_SENT_BY_TX;

   
        frame[2] = 0x80;

    
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

    unsigned char BCC2 = buf[0];

    for (int j = 1; j < bufSize; j++) {
        BCC2 ^= buf[j]; 
    }

    frame[newFrameSize - 2] = BCC2;
    frame[newFrameSize - 1] = FLAG;


    printf("\n DEBUG: Now doing byte stuffing part.\n Before: ");
    for (int i = 0; i < bufSize; i++) {
        printf("%x ", buf[i]);
    }


    printf("\n After Byte Stuffing: ");
    for (int i = 0; i < newFrameSize; i++) {
        printf("%x ", frame[i]);
    }

    printf("\n");


}

int main(){

    unsigned char abc[] = {0x7E, 0x7E, 0x10, 0x7D};

    ByteStuffer(abc, 4);
    return 0;
}

