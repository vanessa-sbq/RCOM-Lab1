# Doubts

- If a call to write only writes partial data should we retransmit the whole frame or should we just transmit the rest that wasn't actually transmitted.
- Should we return -1 on error when readByte() gives an error?
- In the case where Tx has a fire and cannot send anything anymore, how will Rx know and stop ?
- Data size in D1 and Dn (should be in application layer or link layer?)
- Como saber quando parar de ler o D1 ... Dn?