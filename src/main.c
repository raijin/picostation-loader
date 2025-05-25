/*
 * ps1-bare-metal - (C) 2023 spicyjpeg
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * We saw how to load a single texture and display it in the last two examples.
 * Textures, however, are not always simple images displayed in their entirety:
 * sometimes they hold more than one image (e.g. all frames of a character's
 * animation in a 2D game) but are "cropped out" on the fly during rendering to
 * only draw a single frame at a time. These textures are known as spritesheets
 * and the PS1's GPU fully supports them, as it allows for arbitrary UV
 * coordinates to be used.
 *
 * This example is going to show how to implement a simple font system for text
 * rendering, since that's one of the most common use cases for spritesheets. We
 * are going to load a single texture containing all our font's characters, as
 * having hundreds of tiny textures for each character would be extremely
 * inefficient, and then use a lookup table to obtain the UV coordinates, width
 * and height of each character in a string.
 *
 * NOTE: in order to make the code easier to read, I have moved all the
 * GPU-related functions from previous examples to a separate source file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "ps1/cdrom.h"
//#include "includes/rama.c"
#include "includes/cdrom.h"
#include "includes/filesystem.h"
#include "includes/irq.h"
#include "gpu.h"
#include "controller.h"
#include "includes/system.h"
#include <ctype.h>


// In order to pick sprites (characters) out of our spritesheet, we need a table
// listing all of them (in ASCII order in this case) with their UV coordinates
// within the sheet as well as their dimensions. In this example we're going to
// hardcode the table, however in an actual game you may want to store this data
// in the same file as the image and palette data.
typedef struct {
	uint8_t x, y, width, height;
} SpriteInfo;

static const SpriteInfo fontSprites[] = {
	{ .x =  6, .y =  0, .width = 2, .height = 9 }, // !
	{ .x = 12, .y =  0, .width = 4, .height = 9 }, // "
	{ .x = 18, .y =  0, .width = 6, .height = 9 }, // #
	{ .x = 24, .y =  0, .width = 6, .height = 9 }, // $
	{ .x = 30, .y =  0, .width = 6, .height = 9 }, // %
	{ .x = 36, .y =  0, .width = 6, .height = 9 }, // &
	{ .x = 42, .y =  0, .width = 2, .height = 9 }, // '
	{ .x = 48, .y =  0, .width = 3, .height = 9 }, // (
	{ .x = 54, .y =  0, .width = 3, .height = 9 }, // )
	{ .x = 60, .y =  0, .width = 4, .height = 9 }, // *
	{ .x = 66, .y =  0, .width = 6, .height = 9 }, // +
	{ .x = 72, .y =  0, .width = 3, .height = 9 }, // ,
	{ .x = 78, .y =  0, .width = 6, .height = 9 }, // -
	{ .x = 84, .y =  0, .width = 2, .height = 9 }, // .
	{ .x = 90, .y =  0, .width = 6, .height = 9 }, // /
	{ .x =  0, .y =  9, .width = 6, .height = 9 }, // 0
	{ .x =  6, .y =  9, .width = 6, .height = 9 }, // 1
	{ .x = 12, .y =  9, .width = 6, .height = 9 }, // 2
	{ .x = 18, .y =  9, .width = 6, .height = 9 }, // 3
	{ .x = 24, .y =  9, .width = 6, .height = 9 }, // 4
	{ .x = 30, .y =  9, .width = 6, .height = 9 }, // 5
	{ .x = 36, .y =  9, .width = 6, .height = 9 }, // 6
	{ .x = 42, .y =  9, .width = 6, .height = 9 }, // 7
	{ .x = 48, .y =  9, .width = 6, .height = 9 }, // 8
	{ .x = 54, .y =  9, .width = 6, .height = 9 }, // 9
	{ .x = 60, .y =  9, .width = 2, .height = 9 }, // :
	{ .x = 66, .y =  9, .width = 3, .height = 9 }, // ;
	{ .x = 72, .y =  9, .width = 6, .height = 9 }, // <
	{ .x = 78, .y =  9, .width = 6, .height = 9 }, // =
	{ .x = 84, .y =  9, .width = 6, .height = 9 }, // >
	{ .x = 90, .y =  9, .width = 6, .height = 9 }, // ?
	{ .x =  0, .y = 18, .width = 6, .height = 9 }, // @
	{ .x =  6, .y = 18, .width = 6, .height = 9 }, // A
	{ .x = 12, .y = 18, .width = 6, .height = 9 }, // B
	{ .x = 18, .y = 18, .width = 6, .height = 9 }, // C
	{ .x = 24, .y = 18, .width = 6, .height = 9 }, // D
	{ .x = 30, .y = 18, .width = 6, .height = 9 }, // E
	{ .x = 36, .y = 18, .width = 6, .height = 9 }, // F
	{ .x = 42, .y = 18, .width = 6, .height = 9 }, // G
	{ .x = 48, .y = 18, .width = 6, .height = 9 }, // H
	{ .x = 54, .y = 18, .width = 4, .height = 9 }, // I
	{ .x = 60, .y = 18, .width = 5, .height = 9 }, // J
	{ .x = 66, .y = 18, .width = 6, .height = 9 }, // K
	{ .x = 72, .y = 18, .width = 6, .height = 9 }, // L
	{ .x = 78, .y = 18, .width = 6, .height = 9 }, // M
	{ .x = 84, .y = 18, .width = 6, .height = 9 }, // N
	{ .x = 90, .y = 18, .width = 6, .height = 9 }, // O
	{ .x =  0, .y = 27, .width = 6, .height = 9 }, // P
	{ .x =  6, .y = 27, .width = 6, .height = 9 }, // Q
	{ .x = 12, .y = 27, .width = 6, .height = 9 }, // R
	{ .x = 18, .y = 27, .width = 6, .height = 9 }, // S
	{ .x = 24, .y = 27, .width = 6, .height = 9 }, // T
	{ .x = 30, .y = 27, .width = 6, .height = 9 }, // U
	{ .x = 36, .y = 27, .width = 6, .height = 9 }, // V
	{ .x = 42, .y = 27, .width = 6, .height = 9 }, // W
	{ .x = 48, .y = 27, .width = 6, .height = 9 }, // X
	{ .x = 54, .y = 27, .width = 6, .height = 9 }, // Y
	{ .x = 60, .y = 27, .width = 6, .height = 9 }, // Z
	{ .x = 66, .y = 27, .width = 3, .height = 9 }, // [
	{ .x = 72, .y = 27, .width = 6, .height = 9 }, // Backslash
	{ .x = 78, .y = 27, .width = 3, .height = 9 }, // ]
	{ .x = 84, .y = 27, .width = 4, .height = 9 }, // ^
	{ .x = 90, .y = 27, .width = 6, .height = 9 }, // _
	{ .x =  0, .y = 36, .width = 3, .height = 9 }, // `
	{ .x =  6, .y = 36, .width = 6, .height = 9 }, // a
	{ .x = 12, .y = 36, .width = 6, .height = 9 }, // b
	{ .x = 18, .y = 36, .width = 6, .height = 9 }, // c
	{ .x = 24, .y = 36, .width = 6, .height = 9 }, // d
	{ .x = 30, .y = 36, .width = 6, .height = 9 }, // e
	{ .x = 36, .y = 36, .width = 5, .height = 9 }, // f
	{ .x = 42, .y = 36, .width = 6, .height = 9 }, // g
	{ .x = 48, .y = 36, .width = 5, .height = 9 }, // h
	{ .x = 54, .y = 36, .width = 2, .height = 9 }, // i
	{ .x = 60, .y = 36, .width = 4, .height = 9 }, // j
	{ .x = 66, .y = 36, .width = 5, .height = 9 }, // k
	{ .x = 72, .y = 36, .width = 2, .height = 9 }, // l
	{ .x = 78, .y = 36, .width = 6, .height = 9 }, // m
	{ .x = 84, .y = 36, .width = 5, .height = 9 }, // n
	{ .x = 90, .y = 36, .width = 6, .height = 9 }, // o
	{ .x =  0, .y = 45, .width = 6, .height = 9 }, // p
	{ .x =  6, .y = 45, .width = 6, .height = 9 }, // q
	{ .x = 12, .y = 45, .width = 6, .height = 9 }, // r
	{ .x = 18, .y = 45, .width = 6, .height = 9 }, // s
	{ .x = 24, .y = 45, .width = 5, .height = 9 }, // t
	{ .x = 30, .y = 45, .width = 5, .height = 9 }, // u
	{ .x = 36, .y = 45, .width = 6, .height = 9 }, // v
	{ .x = 42, .y = 45, .width = 6, .height = 9 }, // w
	{ .x = 48, .y = 45, .width = 6, .height = 9 }, // x
	{ .x = 54, .y = 45, .width = 6, .height = 9 }, // y
	{ .x = 60, .y = 45, .width = 5, .height = 9 }, // z
	{ .x = 66, .y = 45, .width = 4, .height = 9 }, // {
	{ .x = 72, .y = 45, .width = 2, .height = 9 }, // |
	{ .x = 78, .y = 45, .width = 4, .height = 9 }, // }
	{ .x = 84, .y = 45, .width = 6, .height = 9 }, // ~
	{ .x = 90, .y = 45, .width = 6, .height = 9 },  // Invalid character
	{ .x =  0, .y = 54, .width =  6, .height = 9 }, // 
    { .x =  6, .y = 54, .width =  6, .height = 9 }, // 
    { .x = 12, .y = 54, .width =  4, .height = 9 }, // 
    { .x = 18, .y = 54, .width =  4, .height = 9 }, // 
    { .x = 24, .y = 54, .width =  6, .height = 9 }, // 
    { .x = 30, .y = 54, .width =  6, .height = 9 }, // 
    { .x = 36, .y = 54, .width =  6, .height = 9 }, // 
    { .x = 42, .y = 54, .width =  6, .height = 9 }, // 
    { .x =  0, .y = 63, .width =  7, .height =  9 }, // 
    { .x = 12, .y = 63, .width =  7, .height =  9 }, // 
    { .x = 24, .y = 63, .width =  9, .height =  9 }, // 
    { .x = 36, .y = 63, .width =  8, .height = 10 }, // 
    { .x = 48, .y = 63, .width = 11, .height = 10 }, // 
    { .x = 60, .y = 63, .width = 12, .height = 10 }, // 
    { .x = 72, .y = 63, .width = 14, .height =  9 }, // 
    { .x =  0, .y = 73, .width = 10, .height = 10 }, //
    { .x = 12, .y = 73, .width = 10, .height = 10 }, // 
    { .x = 24, .y = 73, .width = 10, .height = 10 }, // 
    { .x = 36, .y = 73, .width = 10, .height =  9}, // 
    { .x = 48, .y = 73, .width = 10, .height =  9}, // 
    { .x = 60, .y = 73, .width = 10, .height = 10}  //
};

#define FONT_FIRST_TABLE_CHAR '!'
#define FONT_SPACE_WIDTH      4
#define FONT_TAB_WIDTH        32
#define FONT_LINE_HEIGHT      10

static void printString(
	DMAChain *chain, const TextureInfo *font, int x, int y, const char *str
) {
	int currentX = x, currentY = y;

	uint32_t *ptr;

	// Start by sending a texpage command to tell the GPU to use the font's
	// spritesheet. Note that the texpage command before a drawing command can
	// be omitted when reusing the same texture, so sending it here just once is
	// enough.
	ptr    = allocatePacket(chain, 1);
	ptr[0] = gp0_texpage(font->page, false, false);

	// Iterate over every character in the string.
	for (; *str; str++) {
		uint8_t ch = (uint8_t) *str;

		// Check if the character is "special" and shall be handled without
		// drawing any sprite, or if it's invalid and should be rendered as a
		// box with a question mark (character code 127).
		switch (ch) {
			case '\t':
				currentX += FONT_TAB_WIDTH - 1;
				currentX -= currentX % FONT_TAB_WIDTH;
				continue;

			case '\n':
				currentX  = x;
				currentY += FONT_LINE_HEIGHT;
				continue;

			case ' ':
				currentX += FONT_SPACE_WIDTH;
				continue;
		}
		if (ch >= 0x99 && ch <= 0xFF) {
        	ch = '\x7f'; 
    	}

		// If the character was not a tab, newline or space, fetch its
		// respective entry from the sprite coordinate table.
		const SpriteInfo *sprite = &fontSprites[ch - FONT_FIRST_TABLE_CHAR];

		// Draw the character, summing the UV coordinates of the spritesheet in
		// VRAM to those of the sprite itself within the sheet. Enable blending
		// to make sure any semitransparent pixels in the font get rendered
		// correctly.
		ptr    = allocatePacket(chain, 4);
		ptr[0] = gp0_rectangle(true, true, true);
		ptr[1] = gp0_xy(currentX, currentY);
		ptr[2] = gp0_uv(font->u + sprite->x, font->v + sprite->y, font->clut);
		ptr[3] = gp0_xy(sprite->width, sprite->height);

		// Move onto the next character.
		currentX += sprite->width;
	}
}

#define SCREEN_WIDTH     320
#define SCREEN_HEIGHT    240
#define FONT_WIDTH       96
#define FONT_HEIGHT      84
#define FONT_COLOR_DEPTH GP0_COLOR_4BPP

extern const uint8_t fontTexture[], fontPalette[], piTexture[];

#define MAX_LINES 3000   // Maksimum satır sayısı
#define MAX_LENGTH 60

int loadchecker = 0;


/*
size_t list_load(void *sectorBuffer, int LBA,int listingMode){
	if(listingMode == 1){
		uint8_t test[] = {0x50, 0xf1} ;
		issueCDROMCommand(CDROM_CMD_TEST,test,sizeof(test));
	} else {
		uint8_t test[] = {0x50, 0xf3} ;
		issueCDROMCommand(CDROM_CMD_TEST,test,sizeof(test));
	}

    // Buffer'ı sıfırlıyoruz
    memset(sectorBuffer, 0, 2324*5);
    char **currentSectorBuffer = malloc(6 * sizeof(char *));
    for (int i = 0; i < 6; i++) {
        currentSectorBuffer[i] = malloc(2324 * sizeof(char));
    }
	for (int i = 0; i < 6; i++) {
        memset(currentSectorBuffer[i], 0, 2324);
    }
    // Her bir sektör için okuma yapıyoruz
    for (int i = 0; i < 6; i++) {
		printf("cdrom read %i\n",i);
        startCDROMRead(LBA + i, currentSectorBuffer[i], 1, 2324, true, true);
		printf("sector part");
    }
    for (int i = 0; i < 6; i++) {
		printf("sector fill %i\n",i);
        memcpy(sectorBuffer + (i * 2324), currentSectorBuffer[i], 2324);
    }
	for (int i = 0; i < 6; i++) {
        free(currentSectorBuffer[i]); // Her bir parçayı serbest bırak
    }
    free(currentSectorBuffer);
	printf("sector fill done, returning\n");
	return 0;
}
*/
int caseInsensitiveCompare(const char *a, const char *b) {
    while (*a && *b) {
        char charA = tolower((unsigned char)*a);
        char charB = tolower((unsigned char)*b);
        if (charA != charB) {
            return charA - charB;
        }
        a++;
        b++;
    }
    return *a - *b; // Uzunluk farkını kontrol et
}

void swap(char a[], char b[]) {
    char temp[MAX_LENGTH];
    strcpy(temp, a);
    strcpy(a, b);
    strcpy(b, temp);
}

void swapIndex(uint16_t *a, uint16_t *b) {
    uint16_t temp = *a;
    *a = *b;
    *b = temp;
}

// Partition fonksiyonu
int partition(char lines[][MAX_LENGTH], uint16_t indexes[], int low, int high) {
    char pivot[MAX_LENGTH];
    strcpy(pivot, lines[high]); // Pivot elemanı seç
    int i = (low - 1); // Küçük elemanların indeksini tut

    for (int j = low; j < high; j++) {
        // Eğer mevcut eleman pivot'tan küçükse
        if (caseInsensitiveCompare(lines[j], pivot) < 0) {
            i++; // Küçük elemanların indeksini artır
            swap(lines[i], lines[j]); // Elemanları değiştir
            swapIndex(&indexes[i], &indexes[j]); // İndeksleri değiştir
        }
    }
    swap(lines[i + 1], lines[high]); // Pivot'u doğru yerine yerleştir
    swapIndex(&indexes[i + 1], &indexes[high]);  // Pivot'un indeksini değiştir
    return (i + 1); // Pivot'un indeksini döndür
}

// Hızlı sıralama fonksiyonu
void quickSort(char lines[][MAX_LENGTH], uint16_t indexes[], int low, int high) {
    if (low < high) {
        // Partition işlemi
        int pi = partition(lines, indexes, low, high);

        // Sol ve sağ alt dizileri sıralama
        quickSort(lines, indexes, low, pi - 1);
        quickSort(lines, indexes, pi + 1, high);
    }
}


void list_and_parse(int LBA, int listingMode, char lines[MAX_LINES][MAX_LENGTH], int *lineCount, int *firstboot, uint16_t indexes[MAX_LINES]) {
    *lineCount = 0;
    *firstboot = 0;

    uint8_t test[] = {0x50, (listingMode == 1) ? 0xf1 : 0xf3};
    issueCDROMCommand(CDROM_CMD_TEST, test, sizeof(test));

    char currentLine[MAX_LENGTH];
    int currentPos = 0;
	memset(currentLine, 0, sizeof(currentLine));
    for (int s = 0; s < 6; s++) {
        uint8_t sector[2048];
        memset(sector, 0, sizeof(sector));
        startCDROMRead(LBA+s, sector, 1, 2048, false, true);
		printf("sector get,%i,sectordata:\n%s",LBA+s,sector);
        for (int i = 0; i < 2048; i++) {
            char c = (char)sector[i];

            if (c == '\0') continue;

            if (c == '\n') {
                if (currentPos > 0 && *lineCount < MAX_LINES) {
                    currentLine[currentPos] = '\0'; // Null karakter ekle
                    strncpy(lines[*lineCount], currentLine, MAX_LENGTH);
                    lines[*lineCount][MAX_LENGTH - 1] = '\0'; // Null karakter ekle
                    indexes[*lineCount] = *lineCount; // Indexi güncelle
                    (*lineCount)++;
                    currentPos = 0; // currentPos'u sıfırla
                }
            } else if (c != '\r') {
                if (currentPos < MAX_LENGTH - 1) {
                    currentLine[currentPos++] = c; // currentLine dizisine karakter ekle
                }
            }
        }

        if (*lineCount >= MAX_LINES) break; // MAX_LINES sınırını kontrol et
		delayMicroseconds(150);
    }

    // Dosya sonunda açık kalan satırı da ekle
    if (currentPos > 0 && *lineCount < MAX_LINES) {
        currentLine[currentPos] = '\0'; // Null karakter ekle
        strncpy(lines[*lineCount], currentLine, MAX_LENGTH);
        lines[*lineCount][MAX_LENGTH - 1] = '\0'; // Null karakter ekle
        indexes[*lineCount] = *lineCount; // Indexi güncelle
        (*lineCount)++;
    }

    quickSort(lines, indexes, 0, *lineCount - 1);
}




/*
void parseLines(char *dataBuffer, char lines[MAX_LINES][MAX_LENGTH], int *lineCount, int *firstboot,uint16_t indexes[MAX_LINES]) {
    if (!dataBuffer) {
		*firstboot = 2;
        return;
    }

    *lineCount = 0; 
    char *start = dataBuffer;
    char *end = dataBuffer;

    while (*start != '\0' && *lineCount < MAX_LINES + 1) {
        while (*end != '\n' && *end != '\0') {
            end++;
        }

        int length = end - start;
        if (length >= MAX_LENGTH) {
            length = MAX_LENGTH - 1;
        }

        for (int i = 0; i < length; i++) {
            lines[*lineCount][i] = start[i];
        }
        lines[*lineCount][length] = '\0';

        (*lineCount)++;

        if (*end == '\n') {
            end++;
        }
        start = end;
		*firstboot = 0;
		
		for (int i = 0; i < *lineCount; i++) {
        	indexes[i] = i;
    	}
    }
	quickSort(lines, indexes,0, *lineCount - 1);

}
//*/



char games[MAX_LINES][MAX_LENGTH];
char dirs[MAX_LINES][MAX_LENGTH];
int gameLineCount = 0;
int dirLineCount = 0;
uint8_t test[] = {0x50, 0xfa, 0xf0,0xf1} ;
int main(int argc, const char **argv) {

	initIRQ();
	initSerialIO(115200);
	initControllerBus();
	initFilesystem(); 
	initCDROM();

	if ((GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL) {
		puts("Using PAL mode");
		setupGPU(GP1_MODE_PAL, SCREEN_WIDTH, SCREEN_HEIGHT);
	} else {
		puts("Using NTSC mode");
		setupGPU(GP1_MODE_NTSC, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	DMA_DPCR |= DMA_DPCR_ENABLE << (DMA_GPU * 4);

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);
	GPU_GP1 = gp1_dispBlank(false);

	TextureInfo font;

	uploadIndexedTexture(
		&font, fontTexture, fontPalette, SCREEN_WIDTH * 2, 0, SCREEN_WIDTH * 2,
		FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT, FONT_COLOR_DEPTH
	);


	DMAChain dmaChains[2];
	bool     usingSecondFrame = false;


	//dummy list
	//char txtBuffer[2048] = "123456789012345678901234567890123456789012345678901234567890123456789\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\n";

	//file_load("SYSTEM.CNF;1", txtBuffer2);
	//printf("format s %s\n", txtBuffer2);
	//printf("disk buffer %s\n", txtBuffer);
    //uint16_t sectorBuffer[1024];

    uint16_t selectedindex = 0;
    int startnumber = 0;
	int creditsmenu = 0;
	int loadingmenu = 0;
	int framedelayer = 0;
	int framedelayer2 = 0;
	int firstboot = 1;
	int dirDepth = 0;
	uint16_t dirFix = 0;
	char games[MAX_LINES][MAX_LENGTH];
	char dirs[MAX_LINES][MAX_LENGTH];
	uint16_t *indexes = malloc(sizeof(uint16_t) * MAX_LINES);
	uint16_t *indexes2 = malloc(sizeof(uint16_t) * MAX_LINES);
	uint16_t previousButtons = getButtonPress(0);
	for (;;) {
		int bufferX = usingSecondFrame ? SCREEN_WIDTH : 0;
		int bufferY = 0;

		DMAChain *chain  = &dmaChains[usingSecondFrame];
		usingSecondFrame = !usingSecondFrame;

		uint32_t *ptr;

		GPU_GP1 = gp1_fbOffset(bufferX, bufferY);

		chain->nextPacket = chain->data;

		ptr    = allocatePacket(chain, 4);
		ptr[0] = gp0_texpage(0, true, false);
		ptr[1] = gp0_fbOffset1(bufferX, bufferY);
		ptr[2] = gp0_fbOffset2(
			bufferX + SCREEN_WIDTH - 1, bufferY + SCREEN_HEIGHT - 2
		);
		ptr[3] = gp0_fbOrigin(bufferX, bufferY);

		ptr    = allocatePacket(chain, 3);
		ptr[0] = gp0_rgb(64, 64, 64) | gp0_vramFill();
		ptr[1] = gp0_xy(bufferX, bufferY);
		ptr[2] = gp0_xy(SCREEN_WIDTH, SCREEN_HEIGHT);
		if (firstboot == 0 && loadingmenu == 0 && creditsmenu == 0){
			ptr    = allocatePacket(chain, 3);
			ptr[0] = gp0_rgb(48, 48, 48) | gp0_rectangle(false, false, false);
			ptr[1] = gp0_xy(0, 18 + (1+selectedindex-startnumber)*10);
			ptr[2] = gp0_xy(320, 12);
		}

		char controllerbuffer[256];
		
		//get the controller button press

		snprintf(controllerbuffer, sizeof(controllerbuffer), "%i", getButtonPress(0));
		//printString(chain, &font, 56,100, controllerbuffer);
		uint16_t buttons = getButtonPress(0);
		uint16_t pressedButtons = ~previousButtons & buttons;
		if (dirDepth > 0){
			dirFix = 1;
		} else  {
			dirFix = 0;
		}
		int goup = 0;
		if (firstboot == 1){
			printf("entered firstboot\n");
			printString(
				chain, &font, 40, 80,
				"LOADING GAME LIST FROM SD CARD...");
				// We gotta render few layers to be able to show this text
				if(framedelayer2 < 2){
					framedelayer2++;
				} else {
					memset(games, 0, sizeof(games));
					memset(dirs, 0, sizeof(dirs));
					list_and_parse(100, 1, games, &gameLineCount, &firstboot,indexes);
					list_and_parse(120, 2, dirs, &dirLineCount, &firstboot,indexes2);
					framedelayer2 = 0;

				}
		} else if (firstboot == 2){
			printString(
				chain, &font, 40, 80,
				"THERE ARE NO GAMES ON THE SD CARD");
		} else if (creditsmenu == 1){
			printString(
				chain, &font, 40, 40,
				"Picostation Game Loader Alpha Release"
			);
			printString(
				chain, &font, 40, 80,
				"Huge thanks to Rama, Skitchin, SpicyJpeg,\nDanhans42, NicholasNoble and ChatGPT"
			);

			printString(
				chain, &font, 40, 120,
				"https://github.com/raijin/picostation-loader"
			);
			printString(
				chain, &font, 40, 160,
				"https://psx.dev"
			);

			if(pressedButtons & BUTTON_MASK_CIRCLE)    {
				creditsmenu = 0;
			}
		} else if(loadingmenu == 1) {
				printString(
				chain, &font, 40, 80,
				"LOADING...");
				if(framedelayer < 2){
					framedelayer++;
				} else {
					if((selectedindex == 0) & (dirDepth > 0)){
						uint8_t test[] = {CDROM_TEST_DSP_CMD, 0xF4} ;
						issueCDROMCommand(CDROM_CMD_TEST ,test,sizeof(test));
						loadingmenu = 0;
						dirDepth--;
						if (dirDepth == 0){
							dirFix = 0;
						}
						firstboot = 1;
						framedelayer = 0;
						framedelayer2 = 0;
						goup = 1;
						printf("go back! dir depth:%i - dirfix:%i\n",dirDepth,dirFix);
					}
					else if(selectedindex < dirLineCount + dirFix){
						printf("directory change\n");
						uint16_t sendData = indexes2[selectedindex-dirFix] + 1;
						uint8_t high = (sendData >> 8) & 0xFF; // üst 8 bit
						uint8_t low  = sendData & 0xFF; 
						printf("High: %x, low: %x\n", high,low);
						uint8_t test[] = {CDROM_TEST_DSP_CMD, 0xF0, high, low} ;
						issueCDROMCommand(CDROM_CMD_TEST ,test,sizeof(test));
						loadingmenu = 0;
						framedelayer = 0;
						framedelayer = 2;
						if(goup == 0){
							dirDepth = dirDepth + 1;			
						} else {
							goup = 0;
						}
						firstboot = 1;
						selectedindex = 0;
					} else {
						uint16_t sendData = indexes[(selectedindex-(dirLineCount+dirFix))] + 1;
						printf("game change: %i sendindex:%i selectedindex:%i dirlinecount:%i dirfix: %i\n",sendData,(selectedindex-(dirLineCount+dirFix)), selectedindex,dirLineCount,dirFix);
						uint8_t high = (sendData >> 8) & 0xFF; // üst 8 bit
						uint8_t low  = sendData & 0xFF; 
						printf("High: %x, low: %x", high,low);
						uint8_t test[] = {CDROM_TEST_DSP_CMD, 0xF2, high, low} ;
						issueCDROMCommand(CDROM_CMD_TEST ,test,sizeof(test));
						softFastReboot();
					}
						
				}
				
		} else {
			if(pressedButtons & BUTTON_MASK_UP)   {
				if (selectedindex > 0){
					selectedindex = selectedindex - 1;
					if(startnumber - selectedindex == 1){
						startnumber = startnumber - 20;
					}
				}
				printf("DEBUG:UP  :%d, startnumber:%d\n", selectedindex, startnumber);
			}
			if(pressedButtons & BUTTON_MASK_DOWN)    {
				
				if (selectedindex < (dirFix+gameLineCount+dirLineCount-1)){
					selectedindex = selectedindex + 1;
					if(selectedindex - startnumber > 19){
						startnumber = startnumber + 20;
					}
				}
				printf("DEBUG:DOWN  :%d, startnumber:%d\n", selectedindex, startnumber);
			}

			if(pressedButtons & BUTTON_MASK_RIGHT)    {
				if((dirFix+gameLineCount+dirLineCount)>20){
					if (selectedindex <= (gameLineCount+dirLineCount) - 20){
						selectedindex = selectedindex +20;
						startnumber = startnumber+20;
					} else if (selectedindex - ((dirFix+gameLineCount+dirLineCount)- 20) > 0 && (startnumber + selectedindex) < (dirFix+gameLineCount+dirLineCount)){
						startnumber = startnumber+20;
						selectedindex = gameLineCount+dirLineCount;

					}
				}
				printf("DEBUG:RIGHT  :%d, startnumber:%d\n", selectedindex, startnumber);
			}


			if(pressedButtons & BUTTON_MASK_LEFT)    {
				
				if (selectedindex > 19){
					selectedindex = selectedindex - 20;
					if (startnumber-20 <= 1){
						startnumber = 0;
					} else {
						startnumber = startnumber - 20;
					}
				}
				printf("DEBUG:LEFT  :%d, startnumber:%d\n", selectedindex, startnumber);
			}

			if(pressedButtons & BUTTON_MASK_START)    {
				printf("DEBUG: selectedindex :%d\n", selectedindex);
				loadingmenu = 1;
			}

			if(pressedButtons & BUTTON_MASK_X)    {
				printf("DEBUG:X selectedindex  :%d\n", selectedindex);
				loadingmenu = 1;
				//		 Rama's code 
				//		StartCommand();
				//		WriteParam( 0x50 );
				//		WriteParam( 0xd1 );
				//		WriteParam( 0xab );
				//		WriteParam( 0xfe );
				//		WriteCommand( 0x19 );
				//		AckWithTimeout(500000);
			}

			if((pressedButtons & BUTTON_MASK_L1) && (pressedButtons & BUTTON_MASK_R1))    {
				uint8_t test[] = {CDROM_TEST_DSP_CMD, 0xfa, 0xBE, 0xEF} ;
				issueCDROMCommand(CDROM_CMD_TEST,test,sizeof(test));
			}

			if(pressedButtons & BUTTON_MASK_SELECT)    {
				creditsmenu = 1;
			}

			if(pressedButtons & BUTTON_MASK_TRIANGLE)    {
				firstboot=1;
			}

			printString(
				chain, &font, 16, 10,
				"Picostation Game Loader"
			);
			for (int i = startnumber; i < startnumber + 20; i++) {
			
				char buffer[62];
				if(dirFix == 1 && i == 0){
					snprintf(buffer, sizeof(buffer), "\x93 Go Back");
					printString(chain, &font, 5, 30+(i-startnumber)*10, buffer);	
				}
				else if(i < dirLineCount+dirFix){
					snprintf(buffer, sizeof(buffer), "\x92 %s", dirs[i-dirFix],indexes2[i-dirFix]);
					printString(chain, &font, 5, 30+(i-startnumber)*10, buffer);	
				} else {
					snprintf(buffer, sizeof(buffer), "\x8f %s",games[i-(dirFix+dirLineCount)]);
					printString(chain, &font, 5, 30+(i-startnumber)*10, buffer);	
				}
				/*
				if(i == selectedindex){

					printString(
						chain, &font, 5, 30+(i-startnumber)*10,
						">"
					);

				}
				*/
				if(i == (gameLineCount+dirLineCount+dirFix-1)){
					break;
				}
			}
			//char fbuffer[60];
			//snprintf(fbuffer, sizeof(fbuffer), "selind: %i,stnum: %i,games: %i,dirs: %i, dirfix:%i, dd:%i", selectedindex,startnumber,gameLineCount,dirLineCount,dirFix,dirDepth);
			//snprintf(fbuffer, sizeof(fbuffer),"selected index:%i, possible index:%i",(selectedindex-(dirLineCount+dirFix)),(indexes[(selectedindex-(dirLineCount+dirFix))] + 1));
			//printString(chain, &font, 16, 20, fbuffer);

		}
		previousButtons = buttons;
		*(chain->nextPacket) = gp0_endTag(0);
		waitForGP0Ready();
		waitForVblank();
		sendLinkedList(chain->data);
	}

	return 0;
}
