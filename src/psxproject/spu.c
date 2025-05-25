#include <assert.h>
#include <stdint.h>
#include "ps1/cdrom.h"
#include "filesystem.h"
#include "spu.h"
#include "ps1/registers.h"
#include "system.h"
#include "cdrom.c"

/* Basic API */

static const int _DMA_CHUNK_SIZE = 4;
static const int _DMA_TIMEOUT    = 100000;
static const int _STATUS_TIMEOUT = 10000;

uint32_t spuAllocPtr = 0x1010; // Pointer to the next free space in SPU ram

static bool _waitForStatus(uint16_t mask, uint16_t value){
    for(int timeout = _STATUS_TIMEOUT; timeout > 0; timeout -= 10) {
        if((SPU_STAT & mask) == value){
            return true;
        }

        delayMicroseconds(10);
    }

    return false;
}

void initSPU(void){
    BIU_DEV4_CTRL = 0
        | ( 1 <<  0) // Write delay
        | (14 <<  4) // Read delay
        | BIU_CTRL_RECOVERY
        | BIU_CTRL_WIDTH_16
        | BIU_CTRL_AUTO_INCR
        | ( 9 << 16) // Number of address lines
        | ( 0 << 24) // DMA read/write delay
        | BIU_CTRL_DMA_DELAY;
    
    SPU_CTRL = 0;
    _waitForStatus(0x3f, 0);

    SPU_MASTER_VOL_L = 0;
    SPU_MASTER_VOL_R = 0;
    SPU_REVERB_VOL_L = 0;
    SPU_REVERB_VOL_R = 0;
    SPU_REVERB_ADDR  = SPU_RAM_END / 8;

    SPU_FLAG_FM1     = 0;
    SPU_FLAG_FM2     = 0;
    SPU_FLAG_NOISE1  = 0;
    SPU_FLAG_NOISE2  = 0;
    SPU_FLAG_REVERB1 = 0;
    SPU_FLAG_REVERB2 = 0;

    SPU_CTRL = SPU_CTRL_ENABLE;
    _waitForStatus(0x3f, 0);

    // Place a dummy (silent) looping block at the beginning of SPU RAM.
    SPU_DMA_CTRL = 4;
    SPU_ADDR     = DUMMY_BLOCK_OFFSET / 8;

    SPU_DATA = 0x0500;
    for(int i = 7; i > 0; i--){
        SPU_DATA = 0;
    }

    SPU_CTRL = SPU_CTRL_XFER_WRITE | SPU_CTRL_ENABLE;
    _waitForStatus(SPU_CTRL_XFER_BITMASK | SPU_STAT_BUSY, SPU_CTRL_XFER_WRITE);
    delayMicroseconds(100);

    SPU_CTRL = SPU_CTRL_UNMUTE | SPU_CTRL_ENABLE;
    stopChannel(ALL_CHANNELS);

    // Enable the SPU's DMA channel
    DMA_DPCR |= DMA_DPCR_ENABLE << (DMA_SPU * 4);

    setMasterVolume(MAX_VOLUME, 0);
}

Channel getFreeChannel(void) {
    bool reenableInterrupts = disableInterrupts();

    for(Channel ch = 0; ch < NUM_CHANNELS; ch++){
        if(!SPU_CH_ADSR_VOL(ch)){
            if(reenableInterrupts){
                enableInterrupts();
            }
            return ch;
        }
    }
    if(reenableInterrupts){
        enableInterrupts();
    }
    return -1;
}

ChannelMask getFreeChannels(int count){
    bool reenableInterrupts = disableInterrupts();

    ChannelMask mask = 0;

    for(Channel ch = 0; ch < NUM_CHANNELS; ch++){
        if(SPU_CH_ADSR_VOL(ch)){
            continue;
        }

        mask |= 1 << ch;
        count--;

        if(!count){
            break;
        }
    }

    if(reenableInterrupts){
        enableInterrupts();
    }
    return mask;
}

void stopChannels(ChannelMask mask){
    mask &= ALL_CHANNELS;

    SPU_FLAG_OFF1 = mask & 0xffff;
    SPU_FLAG_OFF2 = mask >> 16;

    for(Channel ch = 0; mask; ch++, mask >>= 1){
        if(!(mask & 1)){
            continue;
        }

        SPU_CH_VOL_L(ch) = 0;
        SPU_CH_VOL_R(ch) = 0;
        SPU_CH_FREQ(ch)  = 1 << 12;
        SPU_CH_ADDR(ch)  = DUMMY_BLOCK_OFFSET / 8;
    }

    SPU_FLAG_ON1 = mask & 0xffff;
    SPU_FLAG_ON2 = mask >> 16;
}

size_t upload(uint32_t offset, const void *data, size_t length, bool wait){
    length /= 4;
    
    // (Assert data is aligned uint32_t)

    length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;

    if(!waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT)){
        return 0;
    }

    uint16_t ctrlReg = SPU_CTRL & ~SPU_CTRL_XFER_BITMASK;

    SPU_CTRL = ctrlReg;
    _waitForStatus(SPU_CTRL_XFER_BITMASK, 0);

    SPU_DMA_CTRL = 4;
    SPU_ADDR     = offset / 8;
    SPU_CTRL     = ctrlReg | SPU_CTRL_XFER_DMA_WRITE;
    _waitForStatus(SPU_CTRL_XFER_BITMASK, SPU_CTRL_XFER_DMA_WRITE);

    DMA_MADR(DMA_SPU) = (uint32_t)(data);
    DMA_BCR (DMA_SPU) = concat4_16(_DMA_CHUNK_SIZE, length);
    DMA_CHCR(DMA_SPU) = 0
        | DMA_CHCR_WRITE
        | DMA_CHCR_MODE_SLICE
        | DMA_CHCR_ENABLE;
    
    if(wait){
        waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT);
    }

    return length * _DMA_CHUNK_SIZE * 4;
}

size_t download(uint32_t offset, void * data, size_t length, bool wait){
     length /= 4;
    
    // (Assert data is aligned uint32_t)

    length = (length + _DMA_CHUNK_SIZE - 1) / _DMA_CHUNK_SIZE;

    if(!waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT)){
        return 0;
    }

    uint16_t ctrlReg = SPU_CTRL & ~SPU_CTRL_XFER_BITMASK;

    SPU_CTRL = ctrlReg;
    _waitForStatus(SPU_CTRL_XFER_BITMASK, 0);

    SPU_DMA_CTRL = 4;
    SPU_ADDR     = offset / 8;
    SPU_CTRL     = ctrlReg | SPU_CTRL_XFER_DMA_READ;
    _waitForStatus(SPU_CTRL_XFER_BITMASK, SPU_CTRL_XFER_DMA_READ);

    DMA_MADR(DMA_SPU) = (uint32_t)(data);
    DMA_BCR (DMA_SPU) = concat4_16(_DMA_CHUNK_SIZE, length);
    DMA_CHCR(DMA_SPU) = 0
        | DMA_CHCR_READ
        | DMA_CHCR_MODE_SLICE
        | DMA_CHCR_ENABLE;
    
    if(wait){
        waitForDMATransfer(DMA_SPU, _DMA_TIMEOUT);
    }

    return length * _DMA_CHUNK_SIZE * 4;
}

/* Sound Class */
void sound_create(Sound *sound){
    sound->offset     = 0;
    sound->sampleRate = 0;
    sound->length     = 0;
}
bool sound_initFromVAGHeader(Sound *sound, const VAGHeader *vagHeader, uint32_t _offset){
    if(!vagHeader_validateMagic(vagHeader)){
        return false;
    }
    sound->offset     = _offset;
    sound->sampleRate = vagHeader_getSPUSampleRate(vagHeader);
    sound->length     = vagHeader_getSPULength(vagHeader);
    return true;
}

#include <stdio.h>

/// @brief Play a sound on a given channel.
/// @param sound Pointer to the sound to play.
/// @param left Left channel volume.
/// @param right Right channel volume.
/// @param ch Channel to play on
/// @return Channel number playback started on, if successful.
/// -1 for invalid channel selection
/// -2 for invalid sound offset
Channel sound_playOnChannel(Sound *sound, uint16_t left, uint16_t right, Channel ch) {
    if((ch<0) || (ch >= NUM_CHANNELS)){
        return -1;
    }
    if(!sound->offset){
        printf("Length:     %d\n", sound->length);
        printf("Offset:     %d\n", sound->offset);
        printf("SampleRate: %d\n", sound->sampleRate);
        printf("INVALID SOUND OFFSET: %d\n", sound->offset);
        return -2;
    }

    SPU_CH_VOL_L(ch) = left;
	SPU_CH_VOL_R(ch) = right;
	SPU_CH_FREQ (ch) = sound->sampleRate;
	SPU_CH_ADDR (ch) = sound->offset / 8;
	SPU_CH_ADSR1(ch) = 0x00ff;
	SPU_CH_ADSR2(ch) = 0x0000;

    if(ch < 16){
        SPU_FLAG_ON1 = 1 << ch;
    } else {
        SPU_FLAG_ON2 = 1 << (ch - 16);
    }

    return ch;
}

int sound_loadSound(const char *name, Sound *sound){
    int remainingLength;
    int uploadedData;
    uint32_t _vagLba;
    uint8_t _sectorBuffer[2048];
    
    
    // Find the file on the filesystem
    _vagLba = getLbaToFile(name);
    assert(_vagLba); // File not found

    // Load the data into the sector
    startCDROMRead(_vagLba, &_sectorBuffer, 1, 2048, true, true);
    // Set the header data
    const VAGHeader *_vagHeader = (const VAGHeader*) _sectorBuffer;
    
    printf("Sound: %s\n", name);
    printf("%d\n",   _vagHeader->channels);
    printf("%d\n",   _vagHeader->interleave);
    printf("%d\n",   _vagHeader->length);
    printf("%d\n",   _vagHeader->magic);
    printf("%d\n\n", _vagHeader->version);
    printf("%d\n\n", _vagHeader->sampleRate);

    // Initialise the sound
    sound_create(sound);
    if(!sound_initFromVAGHeader(sound, _vagHeader, spuAllocPtr)){
        // Failed to validate magic header
        return 2;
    }

    remainingLength = sound->length;

    // Upload first sector of audio data.
    // Whether the data goes on further than this, we need to exclude the header data.
    uploadedData = upload(
        spuAllocPtr,
        vagHeader_getData(_vagHeader),
        min(remainingLength, (2048 - sizeof(VAGHeader))),
        true
    );
    spuAllocPtr += uploadedData;
    remainingLength -= uploadedData;

    while(remainingLength){
        // If not all the data is uploaded, load the next sector of data
        startCDROMRead(
            ++_vagLba,
            &_sectorBuffer,
            1,
            2048,
            true,
            true
        );

        uploadedData = upload(
            spuAllocPtr,
            _sectorBuffer,
            min(remainingLength, 2048),
            true
        );
        spuAllocPtr += uploadedData;
        remainingLength -= uploadedData;

    }

    return 0;
}


int sound_loadSoundFromBinary(const uint8_t *data, Sound *sound){
    int remainingLength;
    
    VAGHeader *_vagHeader = (const VAGHeader*) data;
    
    printf("%d\n",   _vagHeader->channels);
    printf("%d\n",   _vagHeader->interleave);
    printf("%d\n",   _vagHeader->length);
    printf("%d\n",   _vagHeader->magic);
    printf("%d\n\n", _vagHeader->version);
    printf("%d\n\n", _vagHeader->sampleRate);

    _vagHeader->sampleRate = (_vagHeader->sampleRate * 2) / 3;

    // Initialise the sound
    sound_create(sound);
    if(!sound_initFromVAGHeader(sound, _vagHeader, spuAllocPtr)){
        // Failed to validate magic header
        return 2;
    }

    upload(
        spuAllocPtr,
        vagHeader_getData(_vagHeader),
        sound->length,
        true
    );
    
    return 0;
}