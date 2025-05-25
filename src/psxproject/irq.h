#pragma once
#include <stdbool.h>

extern volatile bool vblank;

void initIRQ(void);
void interruptHandlerFunction(void *arg);
void handleCDROMIRQ(void);
void waitForVblank(void);
