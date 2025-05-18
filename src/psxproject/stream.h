#pragma once
//#include "ps1/cdrom.h"
#include "spu.h"


// TODO:
// Do most of these stream_ functions really need to be public?

/* Stream Class */
typedef struct Stream{
    uint32_t _channelMask;
    uint16_t _head, _tail, _bufferedChunks;

    uint32_t offset;
    uint16_t interleave, numChunks, sampleRate, channels;
} Stream;

static inline size_t stream_getChunkLength(Stream *_stream) {
    return (size_t)(_stream->interleave) * (size_t)(_stream->channels);
}
static inline size_t stream_getFreeChunkCount(Stream *stream){
    __atomic_signal_fence(__ATOMIC_ACQUIRE);

    // The currently playing chunk cannot be overwritten.
    size_t playingChunk = stream->_channelMask ? 1 : 0;
    return stream->numChunks - (stream->_bufferedChunks + playingChunk);

}

static inline uint32_t stream_getChunkOffset(Stream *stream, size_t chunk) {
    return stream->offset + stream_getChunkLength(stream) * chunk;
}
void stream_configureIRQ(Stream *stream);


ChannelMask stream_startWithChannelMask(uint16_t left, uint16_t right, ChannelMask mask);

static inline ChannelMask stream_start(Stream *stream, uint16_t left, uint16_t right){
    return stream_startWithChannelMask(left, right, getFreeChannels(NUM_CHANNELS));
}
static inline bool stream_isPlaying(Stream *stream){
    __atomic_signal_fence(__ATOMIC_ACQUIRE);

    return (stream->_channelMask != 0);
}
static inline bool stream_isUnderrun(Stream *stream){
    __atomic_signal_fence(__ATOMIC_ACQUIRE);

    return !stream->_bufferedChunks;
}


void stream_create(Stream *stream);
bool stream_initFromVAGHeader(Stream *stream, const VAGHeader *vagHeader, uint32_t _offset, size_t _numChunks);

void stream_stop(Stream *stream);
void stream_handleInterrupt(Stream *stream);

size_t stream_feed(Stream *stream, const void *data, size_t length);
void stream_resetBuffer(Stream *stream);

/* Stream State Machine*/

// This needs to be accessible by things like IRQ for now.
// Users can also access it if needed, but there shouldn't be many reasons to.
extern Stream stream;


typedef enum{
	STREAM_SM_IDLE          = 0,
	STREAM_SM_WAIT_FOR_DATA = 1,
    STREAM_SM_DATA_READY    = 2
} StreamStateMachineState;

// TODO:
// Is this function necessary?
void stream_init(void);

/// @brief Load the VAG header and prepare for song streaming
/// @param name File path of the song.
/// @return Zero or Error code.
size_t stream_loadSong(const char *name);

/// @brief UNUSED
void stream_play(void);

/// @brief Update the stream state machine. Will feed more data to the ring buffer if required.
void stream_update(void);
