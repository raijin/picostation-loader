/*
 * (C) 2024 Rhys Baker, spicyjpeg
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
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DTR_DELAY   60
#define DSR_TIMEOUT 120

// The controller bus is shared with memory cards.
// An addressing mechanism is used to ensure packets are processed
// by only one device at a time.
// The first byte of each request is the "address" of the peripheral
// that should respond.
typedef enum {
    ADDR_CONTROLLER  = 0x01,
    ADDR_MEMORY_CARD = 0x81
} DeviceAddress;

// The address is followed by a command byte.
// All controllers support CMD_POLL, but some controllers
// also support an additional "configuration mode" which
// grants access to extra commands.
typedef enum {
    CMD_INIT_PRESSURE   = '@', // Initialize DualShock pressure sensors (config)
	CMD_POLL            = 'B', // Read controller state
	CMD_CONFIG_MODE     = 'C', // Enter or exit configuration mode
	CMD_SET_ANALOG      = 'D', // Set analog mode/LED state (config)
	CMD_GET_ANALOG      = 'E', // Get analog mode/LED state (config)
	CMD_GET_MOTOR_INFO  = 'F', // Get information about a motor (config)
	CMD_GET_MOTOR_LIST  = 'G', // Get list of all motors (config)
	CMD_GET_MOTOR_STATE = 'H', // Get current state of vibration motors (config)
	CMD_GET_MODE        = 'L', // Get list of all supported modes (config)
	CMD_REQUEST_CONFIG  = 'M', // Configure poll request format (config)
	CMD_RESPONSE_CONFIG = 'O', // Configure poll response format (config)
	CMD_CARD_READ       = 'R', // Read 128-byte memory card sector
	CMD_CARD_IDENTIFY   = 'S', // Retrieve memory card size information
	CMD_CARD_WRITE      = 'W'  // Write 128-byte memory card sector
} DeviceCommand;

// Indices for function pointers
enum buttonIndex{
    BUTTON_INDEX_SELECT   =  0,
    BUTTON_INDEX_L3       =  1,
    BUTTON_INDEX_R3       =  2,
    BUTTON_INDEX_START    =  3,
    BUTTON_INDEX_UP       =  4,
    BUTTON_INDEX_RIGHT    =  5,
    BUTTON_INDEX_DOWN     =  6,
    BUTTON_INDEX_LEFT     =  7,
    BUTTON_INDEX_L2       =  8,
    BUTTON_INDEX_R2       =  9,
    BUTTON_INDEX_L1       = 10,
    BUTTON_INDEX_R1       = 11,
    BUTTON_INDEX_TRIANGLE = 12,
    BUTTON_INDEX_CIRCLE   = 13,
    BUTTON_INDEX_X        = 14,
    BUTTON_INDEX_SQUARE   = 15
};

// Define bit masks for each button
#define BUTTON_MASK_SELECT   (1<< 0)
#define BUTTON_MASK_L3       (1<< 1)
#define BUTTON_MASK_R3       (1<< 2)
#define BUTTON_MASK_START    (1<< 3)
#define BUTTON_MASK_UP       (1<< 4)
#define BUTTON_MASK_RIGHT    (1<< 5)
#define BUTTON_MASK_DOWN     (1<< 6)
#define BUTTON_MASK_LEFT     (1<< 7)
#define BUTTON_MASK_L2       (1<< 8)
#define BUTTON_MASK_R2       (1<< 9)
#define BUTTON_MASK_L1       (1<<10)
#define BUTTON_MASK_R1       (1<<11)
#define BUTTON_MASK_TRIANGLE (1<<12)
#define BUTTON_MASK_CIRCLE   (1<<13)
#define BUTTON_MASK_X        (1<<14)
#define BUTTON_MASK_SQUARE   (1<<15)

typedef struct{
    uint16_t type;
    uint16_t buttons;
    uint16_t rx;
    uint16_t ry;
    uint16_t lx;
    uint16_t ly;
}ControllerInfo;

void delayMicroseconds(int time);
void initControllerBus(void);
bool waitForAcknowledge(int timeout);
void selectPort(int port);
uint8_t exchangeByte(uint8_t value);

int exchangePacket(
    DeviceAddress address, const uint8_t *request, uint8_t *response,
    int reqLength, int maxRespLength
);

bool getControllerInfo(int port, ControllerInfo *output);
// Only these really need to be public?
void controller_update(void);
void controller_subscribeOnKeyDown(void (*function)(), uint16_t buttonIndex);
void controller_subscribeOnKeyUp  (void (*function)(), uint16_t buttonIndex);
void controller_subscribeOnKeyHold(void (*function)(), uint16_t buttonIndex);
void controller_unsubscribeAll(void);