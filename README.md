# Issues with the protocol that was presented

As mentioned in the report, we identified several errors that revealed issues with the protocol.

## Distorted Penguin

The BCC2 (Block Check Character 2) method is a form of checksum that helps detect errors in data transmission by performing an XOR operation across all the data bytes in a frame.
While BCC2 is useful for detecting certain types of errors, it is not sufficient by itself to guarantee that the transmitted data is error-free.
Several factors can cause errors that would not be detected by simply using BCC2.
This can lead to a situation where the receiver mistakenly believes the data is correct, even though the files differ at the end.

### The original file

### The file that was received

### The bytes that were changed

## Concord with distorted colors

The need to byte-stuff the BCC2 arises from a special scenario in data transmission:
if the BCC2 checksum byte happens to be the same as a FLAG byte (or any other reserved special byte, like ESCAPE_OCTET),
it could lead to a situation where the receiver mistakenly interprets the BCC2 byte as a frame delimiter rather than as part of the data.
This issue occurs because the FLAG byte is used to mark the start and end of a frame, and if the BCC2 coincidentally matches the value of a FLAG byte,
the frame might appear incomplete or malformed to the receiver.

### The original file

### The file that was received

### What we changed
