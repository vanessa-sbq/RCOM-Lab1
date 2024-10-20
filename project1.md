# Generic functions of data link protocols.

## Framing 

It's the process of packaging and synchronisation / delimitation of a frame.

### Packaging

Data that comes from the upper layer is packed into **frames**.

Frames contain a header, body and a trailer.

All of the data that comes from the upper layer goes into the body section.

These **frames** are called Information Frames.

### Frame synchronization (delimitation)

Like the name suggests **start** and **end** of frames are uniquely identified. (This guarantees data synchronization.)

Usually the start and end contain a **flag** (special character), this allows the frame's beggining and end to be identified.

#### Problem

Sometimes the data that is going to be put inside the body may contain characters that are the same as these flags.

For these we do a simple modification to the data so that the characters inside the data do not get mistaken by the start/end flag.

#### Frame size

The size of frames may be implicitly determined (counting the number of bytes in between synchronization flags) or explicitly indicated (header field).



# Specification

There exist 3 types of frames.

All of the frames have the same header. Only the information frame actually contains any data.

Note that this data comes from the layer above and is not processed by the data link protocol...

We will also use 

> Eight-bit byte as a frame
> Byte stuffing that will ensure that the data does not cointain the previous value.
> Stop and Wait (unit window with modulo 2 numbering) -> Error Control


Note that altough the Transmitter is the only machine that sends the file (and Information Frames), the Receviver also transmits frames (Supervision and Unnumbered Frames).


# General Procedures

**All frames that have a wrong header are ignores without any action.**

**Infomation, SET and DISC frames** are protected by a timer. In the case of a time-out a maximum number of retransmitions attempts must be made. (**Retransmition value must be configurable.**)

# Notes

- A frame can be started with one or more flags (This needs to be accounted inside the reception mechanism ... **StateMachine**)

- Frames **I, SET, DISC** are designated Commands while the rest **UA, RR and REJ** are calles Replies.

- Replies may also be sent by the transmitter -> **UA** in the Termination phase

- Commands may also be sent by the receiver -> **DISC** in the Termination phase







## Information frames

See retransmition here -> [Information Frame Retransmition](#information-frame-retransmition)

Information frames will include 2 independent protections. One for the header and one for the data field.

Like the Supervision frames, information frames also contain a header that has the same shape but some values do differ (**BCC1**, **BCC2** altough BCC2 is only after the data.)

F A C BCC1 DATA BCC2 F (Information Frame)

### Control Field of the Information Frame

The control field now only has 2 possible values:

> 0x00 -> Means that the current frame is number 0.


> 0x80 -> ... frame number 1.

### BCC2

BCC2 allows for the detection of errors inside the data field. It is calculated using XOR of all data...

D1 XOR D2 XOR D3 ... XOR DN

**Procedures for Information frames**

> Data field of the Information frames are protected using it's own BCC (even parity on each bit of the data octets and the BCC).




> When receiving an Information Frame and **only** the header has errors the following can be done:

>> If it's a new frame, discard the data field and make a retransmition request with **REJ**. (Timer can be anticipated ?)

>> If it's a duplicate frame, discard the data field, do not pass info to Application **but confirm the reception with RR.**




> When receiving an Information Frame and it has no errors there are two possibilities:

>> If it's a new frame, the data field is accepted, passed into the Application and the frame needs to be confirmed with RR.

>> If it's a duplicate, the data is not passed into the Application but the frame will still be confirmed with RR.




## Supervision frames / Unnumbered frames

They contain 5 8-bit bytes.

F -> Flag (Start of a frame)
A -> Address Field
C -> Control fild 
BCC1 -> Protection Field.
F -> Flag (End of a frame)

### Flag

Flag's value is 0x7E.

### Address Field

Can have 2 values:

> 0x03 -> Indicates that the command was sent by the Transmitter (Tx) or that this is a reply sent by the Receiver (Rx).

> 0x01 -> Indicates that the command was sent by the Receiver (Rx) or that the Transmitter (Tx) has sent a reply.

### Control Field

> SET -> Sent by the transmitter to initiate a connection.

> UA -> Confirmation sent by the receiver that tells the transmitter that the supervision frame was valid.

> RR0 -> Indication sent by the Receiver that says that he's ready to receive the frame number 0.

> RR1 -> Indication sent by the Receiver that says that he's ready to receive the frame number 1.

> REJ0 -> Indication sent by the Receiver that says that frame number 0 had an error and that it was rejected.

> REJ1 -> Indication sent by the Receiver that says that frame number 1 had an error and that it was rejected.

> DISC -> Frame that indicates the termination of a connection.

### BCC1 field

The BCC1 field does an XOR between the Control Field and the Address Field. It is used to detect if the header has any errors.






# Application Layer

In the application Layer the file that is going to be transmited will be fragmented.

Each fragment will be encapsulated in data packets and these will be going down to the data link layer **one by one**.

Note that the Application Layer may send Control Packets...

Also note that both Control and Data Packets are still represented by the **Information Frame**.




# Byte stuffing mechanism

PPP mechanism is adopted. Uses **0x7D** as the **escape octet**.

## How does it work?

If the octet **0x7E** (Flag) occours inside the frame then the sequence will be replaced by the sequence **0x7D 0x5E**.

If the escape octet is also found iside the frame then this sequence will get replaced by **0x7D 0x5D**.

**This means that one octet gets transformed into two octets.**

**This also means that the value that has found inside the frame has XOR'ed with 0x20.**

BCC generation and verification is done before byte stuffing and after byte destuffing.


# The phases of the data link protocol

There are 3 phases (shown in picture below): Establishment, Data Transfer and Termination.

[]()INSERT IMAGE



# Retransmitions, Timer and Frame Protection

Like the specification says, the error control mechanism that is going to be implemented is Stop-and-Wait.

## Timer

The timer only gets enabled after an Information Frame, SET Frame (Establishment Phase) or DISC Frame (Termination Phase).

The timer gets disabled after a valid acknoledgment.

If a timeout occours retransmission is forced.

## Information Frame Retransmition

After a time-out occours due to the loss of frame I or the loss of it's acknowledgement a maximum of preconfigured retransmission attemts are made.

A negative acknoledgment (REJ) will also trigger information frame retransmission. [It may not count as a retransmission ?]

## Frame Protection

Done with BCCs.



# Protocol-Application interface

Application only has llread() which actually returns any data from the Data Link Protocol.

llopen(), llwrite() and llclose() are functions that do not return any data that will contribute to the generation of the file.


## llopen()

Arguments:

> Has a structure has the argument with parameters of the connection.

Returns:

> Data link identifier.
> Negative value if any error occours.

Steps:

I: Rx application layer invokes llopen()

II: Tx application layer invokes llopen() that will run in the link layer and will exchange supervision frames.

## llwrite() && llread()

Arguments for llwrite:

> Has a buffer with the characters that are to be trasmitted and the buffer's size (How many characters are inside the buffer).

Returns for llwrite:

> Number of characters that were written.
> Negative value if any error occours.



Arguments for llread():

> The function receives a pointer where the array of characters is going to be written.

Returns for llread():

> Array length (number of characters that were read)
> Negative value if any error occours.


Steps:

The Tx application layer forms a packet (data or control) and invokes llwrite()

The Rx application layer invoked llread()

llread() and llwrite() exchange Information and Supervision frames.

When the frames are correctly received, both functions return the control back to the application layer.


## llclose()

Arguments:

> Receives an integer value that will tell if we should display Communication statistics.

Returns:

> Positive value in case of success.
> Negative value in case of error.

Steps:

The application layer of Tx and Rx invoke the llclose() function that will run at the link layer, exchanging appropritate Supervision frames.



# Test application

The application should support two types of packets sent by the Transmitter:

> Control package that signals the start and end of a file transfer.

> Data packets that contain fragments of the file to be transmitted.

## Control Packet

The END of transmission control packet should repeat the information that was contained in the START packet.

The control packet must have a field with the file size and optionally a field with the file name (possibly other fields).

C T1L1V1 T2L2V2 ...

### Control Field [C]

Value 1 represents START Packet.

Value 2 represents END Packet.

Each parameter (size, file name ot other) is coded as TLV (Type, Lenght, Value).

T (one octet) {

0 - file size
1 - file name
other values - can be defined

} indicates the parameter


L (one octet) {
    represents length ?
} indicates the V field size in octets


V (one octet) {
    ?
} number of octets indicated in L



## Data Packets

Data Packets must contain a field (two octets) that indicate the size of the respective data field (D1...DN) to allow for aditional checks regarding data integrity.

**This size depends on the maximum size established for the Information field of Information Frames.**

C S L2 L1 P1 ... Pk

C (Control Field) {
    2 - data
}

S (Sequence number) {
    0-99
}

L2 L1  (Number of octets (K) in data field){
    K = 256 * L2 + L1
}

P1..Pk (Packet data field) {
    K octets
}


# Layer Independency

Layered Architectures such as this one are based in the fact that layers are independent of each other.

## At the data link layer

No processing shall be done that may affect the header of packets passed by the application layer.

This data is "innaccessible" to data link protocol.

No distinction is made between control and data packets (The data link layer is "blind", it doesn't need to treat control and data packets differently).

## At the application layer

The application layer does not know about the details of the data link layer. It only knows that it can use it's services to send and receive data.

It has no knowledge about the structure of the frames and the mechanisms inside it. All mechanisms that are present indide the data link layer (frame delineation/synchronisation, byte stuffing, byte destuffing, protection mechanisms of the frames, numbering of frames, retransmition ...) are exclusively performed in the data link layer.


# Serial Port Reception Types

## Canonic

read() returns only full lines (ended by ASCII LF EOF, EOL).

Used for terminals.


## Non-canonic

read() returns up to a maximum number of characters.

Enables the configuration of a maximum time between each character read.

Suitable for reading groups of characters.


## Asynchronous

read() returns immediatly.

Uses a signal handler.

