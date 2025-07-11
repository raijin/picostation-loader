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
#include <string.h>
#include <stdio.h>
#include "ps1/gpucmd.h"
#include "ps1/registers.h"
#include "ps1/cdrom.h"
#include "psxproject/cdrom.h"
#include "psxproject/filesystem.h"
#include "psxproject/irq.h"
#include "gpu.h"
#include "controller.h"
#include "psxproject/system.h"
#include <stdlib.h>
#include "file_manager.h"
#include "modplayer.h"
#include "counters.h"
#include "ps1/gte.h"


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define LISTING_SIZE 2324
#define MAX_FILES 4096

// In order to pick sprites (characters) out of our spritesheet, we need a table
// listing all of them (in ASCII order in this case) with their UV coordinates
// within the sheet as well as their dimensions. In this example we're going to
// hardcode the table, however in an actual game you may want to store this data
// in the same file as the image and palette data.
typedef struct
{
	uint8_t x, y, width, height;
} SpriteInfo;

static const SpriteInfo fontSprites[] = {
	{.x = 6, .y = 0, .width = 2, .height = 9},	   // !
	{.x = 12, .y = 0, .width = 4, .height = 9},	   // "
	{.x = 18, .y = 0, .width = 6, .height = 9},	   // #
	{.x = 24, .y = 0, .width = 6, .height = 9},	   // $
	{.x = 30, .y = 0, .width = 6, .height = 9},	   // %
	{.x = 36, .y = 0, .width = 6, .height = 9},	   // &
	{.x = 42, .y = 0, .width = 2, .height = 9},	   // '
	{.x = 48, .y = 0, .width = 3, .height = 9},	   // (
	{.x = 54, .y = 0, .width = 3, .height = 9},	   // )
	{.x = 60, .y = 0, .width = 4, .height = 9},	   // *
	{.x = 66, .y = 0, .width = 6, .height = 9},	   // +
	{.x = 72, .y = 0, .width = 3, .height = 9},	   // ,
	{.x = 78, .y = 0, .width = 6, .height = 9},	   // -
	{.x = 84, .y = 0, .width = 2, .height = 9},	   // .
	{.x = 90, .y = 0, .width = 6, .height = 9},	   // /
	{.x = 0, .y = 9, .width = 6, .height = 9},	   // 0
	{.x = 6, .y = 9, .width = 6, .height = 9},	   // 1
	{.x = 12, .y = 9, .width = 6, .height = 9},	   // 2
	{.x = 18, .y = 9, .width = 6, .height = 9},	   // 3
	{.x = 24, .y = 9, .width = 6, .height = 9},	   // 4
	{.x = 30, .y = 9, .width = 6, .height = 9},	   // 5
	{.x = 36, .y = 9, .width = 6, .height = 9},	   // 6
	{.x = 42, .y = 9, .width = 6, .height = 9},	   // 7
	{.x = 48, .y = 9, .width = 6, .height = 9},	   // 8
	{.x = 54, .y = 9, .width = 6, .height = 9},	   // 9
	{.x = 60, .y = 9, .width = 2, .height = 9},	   // :
	{.x = 66, .y = 9, .width = 3, .height = 9},	   // ;
	{.x = 72, .y = 9, .width = 6, .height = 9},	   // <
	{.x = 78, .y = 9, .width = 6, .height = 9},	   // =
	{.x = 84, .y = 9, .width = 6, .height = 9},	   // >
	{.x = 90, .y = 9, .width = 6, .height = 9},	   // ?
	{.x = 0, .y = 18, .width = 6, .height = 9},	   // @
	{.x = 6, .y = 18, .width = 6, .height = 9},	   // A
	{.x = 12, .y = 18, .width = 6, .height = 9},   // B
	{.x = 18, .y = 18, .width = 6, .height = 9},   // C
	{.x = 24, .y = 18, .width = 6, .height = 9},   // D
	{.x = 30, .y = 18, .width = 6, .height = 9},   // E
	{.x = 36, .y = 18, .width = 6, .height = 9},   // F
	{.x = 42, .y = 18, .width = 6, .height = 9},   // G
	{.x = 48, .y = 18, .width = 6, .height = 9},   // H
	{.x = 54, .y = 18, .width = 4, .height = 9},   // I
	{.x = 60, .y = 18, .width = 5, .height = 9},   // J
	{.x = 66, .y = 18, .width = 6, .height = 9},   // K
	{.x = 72, .y = 18, .width = 6, .height = 9},   // L
	{.x = 78, .y = 18, .width = 6, .height = 9},   // M
	{.x = 84, .y = 18, .width = 6, .height = 9},   // N
	{.x = 90, .y = 18, .width = 6, .height = 9},   // O
	{.x = 0, .y = 27, .width = 6, .height = 9},	   // P
	{.x = 6, .y = 27, .width = 6, .height = 9},	   // Q
	{.x = 12, .y = 27, .width = 6, .height = 9},   // R
	{.x = 18, .y = 27, .width = 6, .height = 9},   // S
	{.x = 24, .y = 27, .width = 6, .height = 9},   // T
	{.x = 30, .y = 27, .width = 6, .height = 9},   // U
	{.x = 36, .y = 27, .width = 6, .height = 9},   // V
	{.x = 42, .y = 27, .width = 6, .height = 9},   // W
	{.x = 48, .y = 27, .width = 6, .height = 9},   // X
	{.x = 54, .y = 27, .width = 6, .height = 9},   // Y
	{.x = 60, .y = 27, .width = 6, .height = 9},   // Z
	{.x = 66, .y = 27, .width = 3, .height = 9},   // [
	{.x = 72, .y = 27, .width = 6, .height = 9},   // Backslash
	{.x = 78, .y = 27, .width = 3, .height = 9},   // ]
	{.x = 84, .y = 27, .width = 4, .height = 9},   // ^
	{.x = 90, .y = 27, .width = 6, .height = 9},   // _
	{.x = 0, .y = 36, .width = 3, .height = 9},	   // `
	{.x = 6, .y = 36, .width = 6, .height = 9},	   // a
	{.x = 12, .y = 36, .width = 6, .height = 9},   // b
	{.x = 18, .y = 36, .width = 6, .height = 9},   // c
	{.x = 24, .y = 36, .width = 6, .height = 9},   // d
	{.x = 30, .y = 36, .width = 6, .height = 9},   // e
	{.x = 36, .y = 36, .width = 5, .height = 9},   // f
	{.x = 42, .y = 36, .width = 6, .height = 9},   // g
	{.x = 48, .y = 36, .width = 5, .height = 9},   // h
	{.x = 54, .y = 36, .width = 2, .height = 9},   // i
	{.x = 60, .y = 36, .width = 4, .height = 9},   // j
	{.x = 66, .y = 36, .width = 5, .height = 9},   // k
	{.x = 72, .y = 36, .width = 2, .height = 9},   // l
	{.x = 78, .y = 36, .width = 6, .height = 9},   // m
	{.x = 84, .y = 36, .width = 5, .height = 9},   // n
	{.x = 90, .y = 36, .width = 6, .height = 9},   // o
	{.x = 0, .y = 45, .width = 6, .height = 9},	   // p
	{.x = 6, .y = 45, .width = 6, .height = 9},	   // q
	{.x = 12, .y = 45, .width = 6, .height = 9},   // r
	{.x = 18, .y = 45, .width = 6, .height = 9},   // s
	{.x = 24, .y = 45, .width = 5, .height = 9},   // t
	{.x = 30, .y = 45, .width = 5, .height = 9},   // u
	{.x = 36, .y = 45, .width = 6, .height = 9},   // v
	{.x = 42, .y = 45, .width = 6, .height = 9},   // w
	{.x = 48, .y = 45, .width = 6, .height = 9},   // x
	{.x = 54, .y = 45, .width = 6, .height = 9},   // y
	{.x = 60, .y = 45, .width = 5, .height = 9},   // z
	{.x = 66, .y = 45, .width = 4, .height = 9},   // {
	{.x = 72, .y = 45, .width = 2, .height = 9},   // |
	{.x = 78, .y = 45, .width = 4, .height = 9},   // }
	{.x = 84, .y = 45, .width = 6, .height = 9},   // ~
	{.x = 90, .y = 45, .width = 6, .height = 9},   // Invalid character
	{.x = 0, .y = 54, .width = 6, .height = 9},	   //
	{.x = 6, .y = 54, .width = 6, .height = 9},	   //
	{.x = 12, .y = 54, .width = 4, .height = 9},   //
	{.x = 18, .y = 54, .width = 4, .height = 9},   //
	{.x = 24, .y = 54, .width = 6, .height = 9},   //
	{.x = 30, .y = 54, .width = 6, .height = 9},   //
	{.x = 36, .y = 54, .width = 6, .height = 9},   //
	{.x = 42, .y = 54, .width = 6, .height = 9},   //
	{.x = 0, .y = 63, .width = 7, .height = 9},	   //
	{.x = 12, .y = 63, .width = 7, .height = 9},   //
	{.x = 24, .y = 63, .width = 9, .height = 9},   //
	{.x = 36, .y = 63, .width = 8, .height = 10},  //
	{.x = 48, .y = 63, .width = 11, .height = 10}, //
	{.x = 60, .y = 63, .width = 12, .height = 10}, //
	{.x = 72, .y = 63, .width = 14, .height = 9},  //
	{.x = 0, .y = 73, .width = 10, .height = 10},  //
	{.x = 12, .y = 73, .width = 10, .height = 10}, //
	{.x = 24, .y = 73, .width = 10, .height = 10}, //
	{.x = 36, .y = 73, .width = 10, .height = 9},  //
	{.x = 48, .y = 73, .width = 10, .height = 9},  //
	{.x = 60, .y = 73, .width = 10, .height = 10},  //
	{.x = 72, .y = 73, .width = 10, .height = 10},  //
	{.x = 85, .y = 73, .width = 8, .height = 8}  //
};

static const int8_t Scroll_SineTable[64] = {
     0,  12,  25,  37,  49,  60,  71,  81,
    90,  98, 106, 112, 117, 122, 125, 126,
   127, 126, 125, 122, 117, 112, 106,  98,
    90,  81,  71,  60,  49,  37,  25,  12,
     0, -12, -25, -37, -49, -60, -71, -81,
   -90, -98,-106,-112,-117,-122,-125,-126,
  -127,-126,-125,-122,-117,-112,-106, -98,
   -90, -81, -71, -60, -49, -37, -25, -12
};

typedef enum
{
	MENU_COMMAND_NONE = 0x0,
	MENU_COMMAND_GOTO_ROOT = 0x1,
	MENU_COMMAND_GOTO_PARENT = 0x2,
	MENU_COMMAND_GOTO_DIRECTORY = 0x3,
	MENU_COMMAND_MOUNT_FILE_FAST = 0x4,
	MENU_COMMAND_MOUNT_FILE_SLOW = 0x5,
	MENU_COMMAND_BOOTLOADER = 0x6
} MENU_COMMAND;

typedef enum
{
	COMMAND_GOTO_ROOT = 0x1,
	COMMAND_GOTO_PARENT = 0x2,
	COMMAND_GOTO_DIRECTORY = 0x3,
	COMMAND_GET_NEXT_CONTENTS = 0x4,
	COMMAND_MOUNT_FILE = 0x5,
	COMMAND_IO_COMMAND = 0x6,
	COMMAND_IO_DATA = 0x7,
	COMMAND_BOOTLOADER = 0xA
} COMMAND;

typedef enum
{
	IO_COMMAND_NONE = 0x0,
	IO_COMMAND_GAMEID = 0x1,
} IO_COMMAND;

#define FONT_FIRST_TABLE_CHAR '!'
#define FONT_SPACE_WIDTH 4
#define FONT_TAB_WIDTH 32
#define FONT_LINE_HEIGHT 10

static void sendCommand(uint8_t command, uint16_t argument)
{
	uint8_t test[] = {CDROM_TEST_DSP_CMD, (uint8_t)(0xF0 | command), (uint8_t)((argument >> 8) & 0xFF), (uint8_t)(argument & 0xFF)};
	issueCDROMCommand(CDROM_CMD_TEST, test, sizeof(test));
}

static void printString(DMAChain *chain, const TextureInfo *font, int x, int y, const char *str)
{
	int currentX = x, currentY = y;

	uint32_t *ptr;

	// Start by sending a texpage command to tell the GPU to use the font's
	// spritesheet. Note that the texpage command before a drawing command can
	// be omitted when reusing the same texture, so sending it here just once is
	// enough.
	ptr = allocatePacket(chain, 1);
	ptr[0] = gp0_texpage(font->page, false, false);

	// Iterate over every character in the string.
	for (; *str; str++)
	{
		uint8_t ch = (uint8_t)*str;

		// Check if the character is "special" and shall be handled without
		// drawing any sprite, or if it's invalid and should be rendered as a
		// box with a question mark (character code 127).
		switch (ch)
		{
		case '\t':
			currentX += FONT_TAB_WIDTH - 1;
			currentX -= currentX % FONT_TAB_WIDTH;
			continue;

		case '\n':
			currentX = x;
			currentY += FONT_LINE_HEIGHT;
			continue;

		case ' ':
			currentX += FONT_SPACE_WIDTH;
			continue;
		}
		if (ch >= 0x99 && ch <= 0xFF)
		{
			ch = '\x7f';
		}

		// If the character was not a tab, newline or space, fetch its
		// respective entry from the sprite coordinate table.
		const SpriteInfo *sprite = &fontSprites[ch - FONT_FIRST_TABLE_CHAR];

		// int index = ch - 32;
		// SpriteInfo temp;
		// temp.x = (index % 16) * 8; 
		// temp.y = (index / 16) * 8; 
		// temp.width = 6;
		// temp.height = 8;

		// const SpriteInfo *sprite = &temp;

		// Draw the character, summing the UV coordinates of the spritesheet in
		// VRAM to those of the sprite itself within the sheet. Enable blending
		// to make sure any semitransparent pixels in the font get rendered
		// correctly.
		ptr = allocatePacket(chain, 4);
		ptr[0] = gp0_rectangle(true, true, true);
		ptr[1] = gp0_xy(currentX, currentY);
		ptr[2] = gp0_uv(font->u + sprite->x, font->v + sprite->y, font->clut);
		ptr[3] = gp0_xy(sprite->width, sprite->height);

		// Move onto the next character.
		currentX += sprite->width;
	}
}

static int delay = 0;
static int string_offset = 0;
static int sine_offset = 0;
static int x_ofs = 0;

static inline uint32_t gp0_vertex_uv(int x, int y, int u, int v, uint16_t clut)
{
    return ((v & 0xFF) << 24) | ((u & 0xFF) << 16) | ((y & 0xFFFF) << 8) | (x & 0xFF);
    // Note: clut is usually passed separately, or stored in first command
}

static void printScroll(DMAChain *chain, const TextureInfo *font, int x, int y, const char *str)
{
	int currentX = x, currentY = y;
	int scrollIndex = 0;

	uint32_t *ptr;

	// Start by sending a texpage command to tell the GPU to use the font's
	// spritesheet. Note that the texpage command before a drawing command can
	// be omitted when reusing the same texture, so sending it here just once is
	// enough.
	ptr = allocatePacket(chain, 1);
	ptr[0] = gp0_texpage(font->page, false, false);

	int stringlen = strlen(str);
	int offset = 0;

	uint8_t ch2 = (uint8_t)str[string_offset];
	int scrollWidth = ch2 == 32 ? FONT_SPACE_WIDTH : fontSprites[ch2 - FONT_FIRST_TABLE_CHAR].width;

	// delay2 += 1;
	// if (delay2 == 2)
	// {
		x_ofs+=1;
		if (x_ofs >= scrollWidth)
		{
			string_offset = (string_offset + 1) % stringlen;
			x_ofs = 0;
		}
	// 	delay2 = 0;
		
	// }

	// Iterate over every character in the string.
	while (true)
	{


		int sine_lookup = (scrollIndex + sine_offset) % 64;
		currentY = y + ((Scroll_SineTable[sine_lookup] * 20) / 128);
		scrollIndex = (scrollIndex + 1) % 64;

		delay += 1;
		if (delay == 50)
		{
			sine_offset = (sine_offset + 1) % 64;
			//string_offset+=1;
			delay = 0;
		}

		
		uint8_t ch = (uint8_t)str[(offset + string_offset) % stringlen];
		offset++;

		if (ch == 32)
		{
			currentX += FONT_SPACE_WIDTH;
			continue;
		}

			const SpriteInfo *sprite = &fontSprites[ch - FONT_FIRST_TABLE_CHAR];
		// If the character was not a tab, newline or space, fetch its
		// respective entry from the sprite coordinate table.


		// int index = ch - 32;
		// SpriteInfo temp;
		// temp.x = (index % 16) * 8; 
		// temp.y = (index / 16) * 8; 
		// temp.width = 6;
		// temp.height = 8;

		// const SpriteInfo *sprite = &temp;

		// Draw the character, summing the UV coordinates of the spritesheet in
		// VRAM to those of the sprite itself within the sheet. Enable blending
		// to make sure any semitransparent pixels in the font get rendered
		// correctly.
		ptr = allocatePacket(chain, 4);
		ptr[0] = gp0_rectangle(true, true, true);
		ptr[1] = gp0_xy(currentX - x_ofs, currentY);
		ptr[2] = gp0_uv(font->u + sprite->x, font->v + sprite->y, font->clut);
		ptr[3] = gp0_xy(sprite->width, sprite->height);

		// Move onto the next character.
		currentX += sprite->width;

		if (currentX - x_ofs > 320)
		{
			break;;
		}
	}
}

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_WIDTH 96
#define FONT_HEIGHT 84
#define FONT_COLOR_DEPTH GP0_COLOR_4BPP

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 20
#define TEXTURE_COLOR_DEPTH GP0_COLOR_4BPP

extern const uint8_t fontTexture[], fontPalette[], logoTexture[], logoPalette[];
extern const uint8_t timewarped_hit[];
static uint16_t s_nextCounter = 0;


#define c_maxFilePathLength 255
#define c_maxFilePathLengthWithTerminator c_maxFilePathLength + 1
#define c_maxFileEntriesPerSector 8

int loadchecker = 0;

void wait_ms(uint32_t ms)
{
	uint32_t frequency = (GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL ? 50 : 60;
	uint32_t frames = (ms * frequency) / 1000;
	for (uint32_t i = 0; i < frames; ++i)
	{
		waitForVblank();
	}
}

bool doLookup(uint16_t *itemCount, char *sectorBuffer)
{
	uint16_t offset = 0;
	while (offset < LISTING_SIZE && *itemCount < MAX_FILES)
	{
		uint16_t length = ((uint8_t *)sectorBuffer)[offset];
		if (length == 0)
		{
			return sectorBuffer[offset + 1] == 1 || (sectorBuffer[offset + 2] == 0 && sectorBuffer[offset + 3] == 0);
		}
		file_manager_init_file_data(*itemCount, sectorBuffer[offset + 1], &sectorBuffer[offset + 2], length);
		offset += length + 2;
		*itemCount = *itemCount + 1;
	}

	return false;
}

uint32_t list_load(void *sectorBuffer, uint8_t command, uint16_t argument)
{
	uint16_t fileEntryCount = 0;

	bool hasNext = true;
	while (hasNext)
	{
		sendCommand(command, argument);
		startCDROMRead(
			100,
			sectorBuffer,
			1,
			2340,
			true,
			true);

		hasNext = doLookup(&fileEntryCount, ((char *)sectorBuffer) + 12);
		command = COMMAND_GET_NEXT_CONTENTS;
		argument = fileEntryCount;
	}

	file_manager_sort(fileEntryCount);
	file_manager_clean_list(&fileEntryCount);
	return fileEntryCount;
}

static void checkMusic() {
    if (((int16_t)(s_nextCounter - COUNTERS[1].value)) <= 0) {
        MOD_Poll();
        s_nextCounter += MOD_hblanks;
    }
}

int main(int argc, const char **argv)
{
	COUNTERS[1].mode = 0x0100;

	initIRQ();
	initSerialIO(115200);
	initControllerBus();
	// initFilesystem();
	initCDROM();

	file_manager_init();

	uint8_t currentCommand = MENU_COMMAND_GOTO_ROOT;

	printf("Hello from menu loader!\n");

	if ((GPU_GP1 & GP1_STAT_FB_MODE_BITMASK) == GP1_STAT_FB_MODE_PAL)
	{
		puts("Using PAL mode");
		setupGPU(GP1_MODE_PAL, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
	else
	{
		puts("Using NTSC mode");
		setupGPU(GP1_MODE_NTSC, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	DMA_DPCR |= DMA_DPCR_ENABLE << (DMA_GPU * 4);

	GPU_GP1 = gp1_dmaRequestMode(GP1_DREQ_GP0_WRITE);
	GPU_GP1 = gp1_dispBlank(false);

	TextureInfo font;
	TextureInfo logo;

	uploadIndexedTexture(
		&font, fontTexture, fontPalette, SCREEN_WIDTH * 2, 0, SCREEN_WIDTH * 2,
		FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT, FONT_COLOR_DEPTH
	);

	uploadIndexedTexture(
		&logo, logoTexture, logoPalette, SCREEN_WIDTH * 2, FONT_WIDTH,
		SCREEN_WIDTH * 2, TEXTURE_HEIGHT + (FONT_WIDTH*2), TEXTURE_WIDTH, TEXTURE_HEIGHT,
		TEXTURE_COLOR_DEPTH
	);

	DMAChain dmaChains[2];
	bool usingSecondFrame = false;

	// dummy list
	// char text[2048] = "Game\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\nGame\n";

	char sectorBuffer[2324];

	// file_load("SYSTEM.CNF;1", txtBuffer2);

	// printf("format s %s\n", txtBuffer2);
	///

	uint32_t fileEntryCount = 0;

	// printf("disk buffer %s\n", txtBuffer);
	uint16_t selectedindex = 0;

	// uint16_t sectorBuffer[1024];

	int creditsmenu = 0;

	uint16_t previousButtons = getButtonPress(0);

	for (;;)
	{
		int bufferX = usingSecondFrame ? SCREEN_WIDTH : 0;
		int bufferY = 0;

		DMAChain *chain = &dmaChains[usingSecondFrame];
		usingSecondFrame = !usingSecondFrame;

		uint32_t *ptr;

		GPU_GP1 = gp1_fbOffset(bufferX, bufferY);

		chain->nextPacket = chain->data;

		ptr = allocatePacket(chain, 4);
		ptr[0] = gp0_texpage(0, true, false);
		ptr[1] = gp0_fbOffset1(bufferX, bufferY);
		ptr[2] = gp0_fbOffset2(bufferX + SCREEN_WIDTH - 1, bufferY + SCREEN_HEIGHT - 2);
		ptr[3] = gp0_fbOrigin(bufferX, bufferY);

		ptr    = allocatePacket(chain, 3);
		ptr[0] = gp0_rgb(64, 64, 64) | gp0_vramFill();
		ptr[1] = gp0_xy(bufferX, bufferY);
		ptr[2] = gp0_xy(SCREEN_WIDTH, SCREEN_HEIGHT);

		//draw logo
		//if (firstboot == 0 && loadingmenu == 0){
			ptr    = allocatePacket(chain, 5);
			ptr[0] = gp0_texpage(logo.page, false, false);
			ptr[1] = gp0_rectangle(true, true, true);
			ptr[2] = gp0_xy(96, 10);
			ptr[3] = gp0_uv(logo.u, logo.v, logo.clut);
			ptr[4] = gp0_xy(logo.width, logo.height);
		//}


		char controllerbuffer[256];
		// get the controller button press

		snprintf(controllerbuffer, sizeof(controllerbuffer), "%i", getButtonPress(0));
		// printString(chain, &font, 56,100, controllerbuffer);
		uint16_t buttons = getButtonPress(0);
		uint16_t pressedButtons = ~previousButtons & buttons;

		const uint16_t pageSize = 16;

		if (pressedButtons & BUTTON_MASK_SELECT)
		{
			if (creditsmenu == 0)
			{
				creditsmenu = 1;
				MOD_Load((struct MODFileFormat*)timewarped_hit);
				s_nextCounter = COUNTERS[1].value + MOD_hblanks;
			}
			else
			{
				creditsmenu = 0;
				MOD_Silence();
			}
		}

		if (creditsmenu == 0)
		{
			if (pressedButtons & BUTTON_MASK_UP)
			{
				selectedindex = selectedindex > 0 ? selectedindex - 1 : selectedindex;
			}
			if (pressedButtons & BUTTON_MASK_DOWN)
			{
				selectedindex = selectedindex < (int)(fileEntryCount - 1) ? selectedindex + 1 : selectedindex;
			}
			if (pressedButtons & BUTTON_MASK_LEFT)
			{
				selectedindex = selectedindex >= pageSize ? selectedindex - pageSize : 0;
			}
			if (pressedButtons & BUTTON_MASK_RIGHT)
			{
				selectedindex = selectedindex < (int)(fileEntryCount - (pageSize + 1)) ? selectedindex + pageSize : fileEntryCount - 1;
			}

			if (pressedButtons & BUTTON_MASK_START)
			{
				fileData *file = file_manager_get_file_data(selectedindex);
				if (file->flag == 0)
				{
					currentCommand = MENU_COMMAND_MOUNT_FILE_SLOW;
				}
			}

			if (pressedButtons & BUTTON_MASK_X)
			{
				fileData *file = file_manager_get_file_data(selectedindex);
				if (file->flag == 0)
				{
					currentCommand = MENU_COMMAND_MOUNT_FILE_FAST;
				}
				else
				{
					currentCommand = MENU_COMMAND_GOTO_DIRECTORY;
				}
			}

			if (pressedButtons & BUTTON_MASK_SQUARE)
			{
				currentCommand = MENU_COMMAND_GOTO_PARENT;
			}

			if (pressedButtons & BUTTON_MASK_TRIANGLE)
			{
				currentCommand = MENU_COMMAND_BOOTLOADER;
			}

			if (currentCommand != MENU_COMMAND_NONE)
			{
				printString(chain, &font, 40, 40, "Please Wait Loading...");
			}
			else
			{
				char fbuffer[32];
				snprintf(fbuffer, sizeof(fbuffer), "%i of %i", selectedindex + 1, fileEntryCount);
				printString(chain, &font, 16, 16, fbuffer);

				int32_t start = 0;
				if ((int32_t)fileEntryCount >= pageSize)
				{
					start = MIN(MAX(selectedindex - (pageSize / 2), 0), (int32_t)fileEntryCount - pageSize);
				}

				int32_t itemCount = MIN(start + pageSize, (int32_t)fileEntryCount) - start;
				if (itemCount > 0)
				{
					for (int32_t i = 0; i < itemCount; i++)
					{
						uint32_t index = start + i;

						if (index == selectedindex)
						{
							ptr = allocatePacket(chain, 3);
							ptr[0] = gp0_rgb(48, 48, 48) | gp0_rectangle(false, false, false);
							ptr[1] = gp0_xy(0, 33 + (i * 11));
							ptr[2] = gp0_xy(320, 12);
						}

						fileData *file = file_manager_get_file_data(index);

						char buffer[300];
						snprintf(buffer, sizeof(buffer), "%-4d %s %s\n", index + 1, file->flag == 0 ? "\x8f" : "\x92", file->filename);
						printString(chain, &font, 16, 34 + (i * 11), buffer);
					}
				}
				else
				{
					printString(chain, &font, 40, 40, "Empty Folder");
				}

				printString(chain, &font, 12, 212, "\x91 Select / Fast Boot, \x96 Regular Boot, \x90 Parent Folder");
			}
		}
		else
		{
			printString(
				chain, &font, 40, 40,
				"PicosStation Plus Menu Alpha Release");

			// printString(
			// 	chain, &font, 40, 120,
			// 	"https://github.com/team-Resurgent/picostation-menu");

			printScroll(chain, &font, 0, 120,
				"Well here it is the PicosStation/Plus Credits.... Huge thanks go to Rama, Skitchin, SpicyJpeg, Danhans42, NicholasNoble and ChatGPT... Shout outs go out to every one on the PSX Dev & Xbox-Scene Discord... Until next time EqUiNoX and Raijun......................................................................................................"
			);

			checkMusic();
		}

		//	char printBuffer[1024];

		// 	sprintf(printBuffer, "%i", modelLba);
		//	printf("LBA: %i \n", modelLba);

		//	printString(chain, &font, 56,100, printBuffer);

		previousButtons = buttons;
		*(chain->nextPacket) = gp0_endTag(0);
		waitForGP0Ready();
		waitForVblank();
		sendLinkedList(chain->data);

		if (currentCommand != MENU_COMMAND_NONE)
		{
			if (currentCommand == MENU_COMMAND_GOTO_ROOT)
			{
				fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_ROOT, 0);
			}
			else if (currentCommand == MENU_COMMAND_GOTO_PARENT)
			{
				fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_PARENT, 0);
				selectedindex = 0;
			}
			else if (currentCommand == MENU_COMMAND_BOOTLOADER)
			{
				// sendCommand(COMMAND_BOOTLOADER, 0xBEEF);
			}
			else if (currentCommand == MENU_COMMAND_GOTO_DIRECTORY)
			{
				uint16_t index = file_manager_get_file_index(selectedindex);
				fileEntryCount = list_load(sectorBuffer, COMMAND_GOTO_DIRECTORY, index);
				selectedindex = 0;
			}
			else if ((currentCommand == MENU_COMMAND_MOUNT_FILE_FAST) || (currentCommand == MENU_COMMAND_MOUNT_FILE_SLOW))
			{
				printf("DEBUG: selectedindex :%d\n", selectedindex);

				uint16_t index = file_manager_get_file_index(selectedindex);
				sendCommand(COMMAND_MOUNT_FILE, index);

				initFilesystem();

				char gameId[2048];
				strcpy(gameId, "cdrom:\\PS.EXE;1");

				char configBuffer[2048];
				if (file_load("SYSTEM.CNF;1", configBuffer) == 0)
				{
					printf("SYSTEM.CNF contents = '\n%s'\n", configBuffer);

					int i = 0;
					int j = 0;
					char tempBuffer[500];
					memset(tempBuffer, 0, 500);
					while (configBuffer[i] != '\0' && configBuffer[i] != '\n' && i < 499) {
						if (configBuffer[i] != ' ' && configBuffer[i] != '\t') { 
							tempBuffer[j] = configBuffer[i]; 
							j++;
						}
						i++; 
					}

					char* gameId = tempBuffer;
					if (strncmp(tempBuffer, "BOOT=", 5) == 0) {
						gameId = tempBuffer + 5;
					}

					printf("Game id: %s\n", gameId);

					printf("Sending game id to memcard\n");
					sendGameID(gameId);

					printf("Sending game id to picostation\n");
					sendCommand(COMMAND_IO_COMMAND, IO_COMMAND_GAMEID);
					uint32_t len = strlen(gameId);
					size_t paddedLen = len + 1; 
					for (uint32_t i = 0; i < paddedLen; i += 2)
					{
						delayMicroseconds(10000);
						uint16_t pair = 0;
						if (i < len)
						{
							pair |= (uint8_t)gameId[i] << 8;
						}
						if (i + 1 < len)
						{
							pair |= (uint8_t)gameId[i + 1];
						}
						sendCommand(COMMAND_IO_DATA, pair);
					}
				}
				
				issueCDROMCommand(CDROM_CMD_RESET ,NULL,0);
				delayMicroseconds(40000);
				
				if (currentCommand == MENU_COMMAND_MOUNT_FILE_FAST) {
					softFastReboot();
				} else {
					softReset();
				}
			}

			currentCommand = MENU_COMMAND_NONE;
		}
	}

	return 0;
}
