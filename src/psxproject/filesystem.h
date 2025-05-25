#pragma once

#include <stdint.h>

// Macros

// Read a 32-bit number in the format int32_LSB-MSB from the array, starting at startIndex
// Returns the uint32_t that is parsed.
#define int32_LM(array, startIndex) (((uint32_t)array[startIndex]) | ((uint32_t)array[startIndex+1] << 8) | ((uint32_t)array[startIndex+2] << 16) | ((uint32_t)array[startIndex+3] << 24))

// Global variables
extern uint8_t gRootDirData[2048];

// Structs

typedef struct{
   uint32_t lba;
   uint8_t length;
   char name[255];
} DirectoryEntry;

// Functions

void initFilesystem(void);
uint32_t getRootDirLba(uint8_t *pvdSector, uint32_t *LBA);
int parseDirRecord(uint8_t *dataSector, uint8_t *recordLength, DirectoryEntry *directoryEntry);
void getRootDirData(void *rootDirData);
uint32_t getLbaToFile(const char *filename);