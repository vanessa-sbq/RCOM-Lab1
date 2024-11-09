# Issues with the protocol that was presented

As mentioned in the report, we identified several errors that revealed issues with the protocol.

## Distorted Penguin

The BCC2 (Block Check Character 2) method is a form of checksum that helps detect errors in data transmission by performing an XOR operation across all the data bytes in a frame.
While BCC2 is useful for detecting certain types of errors, it is not sufficient by itself to guarantee that the transmitted data is error-free.
Several factors can cause errors that would not be detected by simply using BCC2.
This can lead to a situation where the receiver mistakenly believes the data is correct, even though the files differ at the end.

### The original file

![Penguin](https://github.com/vanessa-sbq/RCOM-Lab/blob/8b67086b9cbf403ee0e47b0ebd112a5b8b2f0bc3/Assets/Proj1/penguin.gif?raw=true)

### The file that was received

![Distorted-Penguin](https://github.com/vanessa-sbq/RCOM-Lab/blob/8b67086b9cbf403ee0e47b0ebd112a5b8b2f0bc3/Assets/Proj1/distorted-penguin.gif?raw=true)

### The bytes that were changed

![Diff1](https://github.com/vanessa-sbq/RCOM-Lab/blob/8b67086b9cbf403ee0e47b0ebd112a5b8b2f0bc3/Assets/Proj1/diff1.png?raw=true)

![Diff2](https://github.com/vanessa-sbq/RCOM-Lab/blob/8b67086b9cbf403ee0e47b0ebd112a5b8b2f0bc3/Assets/Proj1/diff2.png?raw=true)

![Diff3](https://github.com/vanessa-sbq/RCOM-Lab/blob/8b67086b9cbf403ee0e47b0ebd112a5b8b2f0bc3/Assets/Proj1/diff3.png?raw=true)

Received file has the following bytes altered: 0xe0, 0xc0, 0x30, 0x00, 0xf7, 0xd3, 0x13, 0x2d, 0x86, 0x41

Original file has the following bytes in the place where they were altered: 0xe8, 0x80, 0x38, 0x40, 0xd7, 0xdb, 0x17, 0x29, 0xa6, 0x49

Calculating the XOR of the changed bytes we can see that in both sides the value is 0xCD.

## Concorde with distorted colors

The need to byte-stuff the BCC2 arises from a special scenario in data transmission:
if the BCC2 checksum byte happens to be the same as a FLAG byte (or any other reserved special byte, like ESCAPE_OCTET),
it could lead to a situation where the receiver mistakenly interprets the BCC2 byte as a frame delimiter rather than as part of the data.
This issue occurs because the FLAG byte is used to mark the start and end of a frame, and if the BCC2 coincidentally matches the value of a FLAG byte,
the frame might appear incomplete or malformed to the receiver.

### The original file

![Concorde](https://github.com/vanessa-sbq/RCOM-Lab/blob/0f193779be40ee984dfb7a18bf598e83337528e2/Assets/Proj1/concorde.jpg?raw=true)

### The file that was received

![ConcordeFunnyColors](https://github.com/vanessa-sbq/RCOM-Lab/blob/0f193779be40ee984dfb7a18bf598e83337528e2/Assets/Proj1/concorde-error-bcc2.jpg?raw=true)

### What we changed to fix this issue

To fix this issue we did Byte Stuffing of the BCC2 byte.

```c
// BCC2 byte stuffing
if (BCC2 == FLAG || BCC2 == ESCAPE_OCTET) {
    frame = (unsigned char*)realloc(frame, newFrameSize + sizeof(unsigned char) * 2);
    newFrameSize++;
    frame[newFrameSize - 3] = ESCAPE_OCTET;
    frame[newFrameSize - 2] = BCC2 ^ ESCAPE_XOR;
    numBytesStuffed++;
} else {
    frame[newFrameSize - 2] = BCC2;
}
```
### After doing this we got an error free concorde.

![Diff4](https://github.com/vanessa-sbq/RCOM-Lab/blob/0f193779be40ee984dfb7a18bf598e83337528e2/Assets/Proj1/concorde-diff.png?raw=true)
