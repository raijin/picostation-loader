// Adapted by Rhys Baker
// Based on the C++ code at https://github.com/spicyjpeg/573in1/blob/dev/src/common/spu.hpp

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include "ps1/registers.h"


#define Channel int
#define ChannelMask uint32_t

enum LoopFlag {
    LOOP_END     = 1 << 0,
    LOOP_SUSTAIN = 1 << 1,
    LOOP_START   = 1 << 2
};

static const uint32_t DUMMY_BLOCK_OFFSET = 0x01000;
static const uint32_t DUMMY_BLOCK_END    = 0x01010;
static const uint32_t SPU_RAM_END        = 0x7fff0;

static const int      NUM_CHANNELS = 24;
static const uint16_t MAX_VOLUME   = 0x3fff;

static const ChannelMask ALL_CHANNELS = (1 << NUM_CHANNELS) - 1;

extern uint32_t spuAllocPtr;

/* Utilities */

static inline uint32_t concat4_8(uint8_t a, uint8_t b, uint8_t c, uint8_t d){
    return (a | (b << 8) | (c << 16) | (d << 24));
}

static inline uint32_t concat4_16(uint16_t a, uint16_t b){
    return (a | (b << 16));
}

static inline const uint32_t bswap32(uint32_t num){
    return ((num >> 24) & 0xFF) |
           ((num >> 8)  & 0xFF00) |
           ((num << 8)  & 0xFF0000) |
           ((num << 24) & 0xFF000000);
}

static inline size_t roundup(size_t a, size_t b) {
    return ((a + b - 1) / b) * b;
}

/* Basic SPU API */

void initSPU(void);
Channel getFreeChannel(void);
ChannelMask getFreeChannels(int count);
void stopChannels(ChannelMask mask);

static inline void setMasterVolume(uint16_t master, uint16_t reverb){
    SPU_MASTER_VOL_L = master;
    SPU_MASTER_VOL_R = master;
    SPU_REVERB_VOL_L = reverb;
    SPU_REVERB_VOL_R = reverb;
}

static inline void setChannelVolume(uint8_t channel, uint16_t master){
    SPU_CH_VOL_L(channel) = master;
    SPU_CH_VOL_R(channel) = master;
}

static inline void stopChannel(Channel ch){
    stopChannels(1 << ch);
}

size_t upload(uint32_t offset, const void *data, size_t length, bool wait);
size_t download(uint32_t offset, void *data, size_t length, bool wait);


/* VAGHeader Class */

static const size_t INTERLEAVED_VAG_BODY_OFFSET = 2048;


typedef struct VAGHeader{
    uint32_t magic, version, interleave, length, sampleRate;
    uint16_t _reserved[5], channels;
    char     name[16];
} VAGHeader;

static inline bool vagHeader_validateMagic(const VAGHeader *vagHeader){
    return (vagHeader->magic == concat4_8('V', 'A', 'G', 'p')) && (vagHeader->channels <= 1);
}

static inline bool vagHeader_validateInterleavedMagic(const VAGHeader *vagHeader){
    return (vagHeader->magic == concat4_8('V', 'A', 'G', 'i')) && vagHeader->interleave;
}

static inline uint16_t vagHeader_getSPUSampleRate(const VAGHeader *vagHeader){
    return (uint16_t)((bswap32(vagHeader->sampleRate) << 12) / 44100);
}


static inline size_t vagHeader_getSPULength(const VAGHeader *vagHeader){
    return bswap32(vagHeader->length);
}

static inline int vagHeader_getNumChannels(const VAGHeader *vagHeader){
    return vagHeader->channels ? vagHeader->channels : 2;
}

static inline const void *vagHeader_getData(const VAGHeader *vagHeader){
    return vagHeader + 1;
}


/* Sound Class */

typedef struct Sound {
    uint32_t offset;
    uint16_t sampleRate, length;
} Sound;

void sound_create(Sound *sound);

bool sound_initFromVAGHeader(Sound *sound, const VAGHeader *vagHeader, uint32_t _offset);
Channel sound_playOnChannel(Sound *sound, uint16_t left, uint16_t right, Channel ch);

static inline Channel sound_play(Sound *sound, uint16_t left, uint16_t right){
    return sound_playOnChannel(sound, left, right, getFreeChannel());
}

/// @brief Load a sound from disk.
/// @param name Filename of the VAGp file to load.
/// @param sound Pointer to the sound struct to save the sound data in.
/// @return Error code
int sound_loadSound(const char *name, Sound *sound);

int sound_loadSoundFromBinary(const uint8_t *data, Sound *sound);
