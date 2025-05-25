#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern volatile bool waitingForInt1;
extern volatile bool waitingForInt2;
extern volatile bool waitingForInt3;
extern volatile bool waitingForInt4;
extern volatile bool waitingForInt5;

extern bool cdromDataReady;

extern void  *cdromReadDataPtr;
extern size_t cdromReadDataSectorSize;
extern volatile size_t cdromReadDataNumSectors;

extern uint8_t cdromResponse[16];
extern uint8_t cdromRespLength;
extern uint8_t cdromStatus;

extern uint8_t cdromLastReadPurpose;


#define toBCD(i) (((i) / 10 * 16) | ((i) % 10))
#define CDROM_COMMAND_ADDRESS 0x1F801801

#define CDROM_BUSY (CDROM_HSTS & CDROM_HSTS_BUSYSTS)


void initCDROM(void);

void issueCDROMCommand(uint8_t cmd, const uint8_t *arg, size_t argLength);

void waitForINT1();
void waitForINT3();


void startCDROMRead(uint32_t lba, void *ptr, size_t numSectors, size_t sectorSize, bool doubleSpeed, bool wait);

bool readDiscName(char *output);

void cdromINT1(void);
void cdromINT2(void);
void cdromINT3(void);
void cdromINT4(void);
void cdromINT5(void);
size_t file_load(const char *name, void *sectorBuffer);