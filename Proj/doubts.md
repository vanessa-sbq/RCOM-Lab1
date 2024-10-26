# Doubts

- If a call to write only writes partial data should we retransmit the whole frame or should we just transmit the rest that wasn't actually transmitted.
- In the case where Tx has a fire and cannot send anything anymore, how will Rx know and stop ?



# Rx

- Como saber quando parar de ler o D1 ... Dn? -> Ler tudo até à flag e verificar BCC2 no fim