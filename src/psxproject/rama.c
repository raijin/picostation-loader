#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define CDREG0 0xBF801800
#define pCDREG0 *(unsigned char *)CDREG0

#define CDREG1 0xBF801801
#define pCDREG1 *(unsigned char *)CDREG1

#define CDREG2 0xBF801802
#define pCDREG2 *(unsigned char *)CDREG2

#define CDREG3 0xBF801803
#define pCDREG3 *(unsigned char *)CDREG3

#define CDREG0_DATA_IN_RESPONSEFIFO 0x20
#define CDREG0_DATA_IN_DATAFIFO 0x40
#define CDREG0_DATA_BUSY 0x80


void CDClearInts() {
    pCDREG0 = 1;
    pCDREG3 = 0x1F;
    // put it back
    pCDREG0 = 0;
}

void StartCommand() {

    while( ( pCDREG0 & CDREG0_DATA_IN_DATAFIFO ) != 0 )
        ;
    while( ( pCDREG0 & CDREG0_DATA_BUSY ) != 0 )
        ;

    // Select Reg3,Index 1 : 0x1F resets all IRQ bits
    //CDClearInts(); // new clearInts now does pCDREG0 = 0 as last step
}

void WriteParam( uint8_t inParam ) {
    // pCDREG0 = 0;                //not required in a loop, but good practice?
    pCDREG2 = inParam;
}

void WriteCommand( uint8_t inCommand ) {
    pCDREG0 = 0;
    pCDREG1 = inCommand;
}

uint8_t lastInt = 0; // global variable
uint8_t lastResponse = 0; // global variable

uint8_t CDWaitIntWithTimeout( unsigned int timeout ) {
    unsigned int timer = 0;
    pCDREG0 = 1;
    while( ( pCDREG3 & 0x07 ) == 0 ) {
        if( timeout && ( timer++ > timeout ) ) {
            if (timeout > 1999) printf(" toAck(%d)\n", timeout);
            return 0;
        }
    }
    //NewPrintf(" Ack(%d)\n", timer);
    uint8_t returnInt = ( pCDREG3 & 0x07 );
    return returnInt;
}

uint8_t ReadResponse() {
    pCDREG0 = 0x01;
    uint8_t returnValue = pCDREG1 & 0xFF; // better mask
    return returnValue;
}

uint8_t AckWithTimeout(unsigned int timeout) {
    lastInt = CDWaitIntWithTimeout(timeout);
    lastResponse = ReadResponse();
    CDClearInts();
    return lastInt;
}