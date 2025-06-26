#include <stdbool.h>
#include <stdint.h>


 // As the controller bus is shared with memory cards, an addressing mechanism is
 // used to ensure packets are processed by a single device at a time. The first
 // byte of each request packet is thus the "address" of the peripheral that
 // shall respond to it.
 typedef enum {
    ADDR_CONTROLLER  = 0x01,
    ADDR_MEMORY_CARD = 0x81
} DeviceAddress;

// The address is followed by a command byte and any required parameters. The
// only command used in this example (and supported by all controllers) is
// CMD_POLL, however some controllers additionally support a "configuration
// mode" which grants access to an extended command set.
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
    CMD_CARD_WRITE      = 'W', // Write 128-byte memory card sector
    CMD_GAME_ID_PING    = ' ',
    CMD_GAME_ID_SEND    = '!'
} DeviceCommand;

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
#define BYTE_DELAY 30


//void delayMicroseconds(int time);
void sendPacketNoAcknowledge(DeviceAddress address, const uint8_t *request, int reqLength);
void sendGameID(const char *str);
void initControllerBus(void);
bool waitForAcknowledge(int timeout);
void selectPort(int port);
uint8_t exchangeByte(uint8_t value);
int exchangePacket(
    DeviceAddress address, const uint8_t *request, uint8_t *response,
    int reqLength, int maxRespLength
);
uint16_t getButtonPress(int port);


