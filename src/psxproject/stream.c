#include "stream.h"

#include "filesystem.h"
#include "spu.h"
#include "system.h"
#include "cdrom.h"


// TODO:
// There are definitely some things that don't need to be public in here.
// Once the state machine is fully included, a lot of these functions can also be made private.
// This whole file can probably be made smaller, or at least much neater.

/* Stream Class */

/*
 * The stream driver lays out a ring buffer of interleaved audio chunks in SPU
 * RAM as follows:
 *
 * +---------------------------------+---------------------------------+-----
 * |              Chunk              |              Chunk              |
 * | +------------+------------+     | +------------+------------+     |
 * | |  Ch0 data  |  Ch1 data  | ... | |  Ch0 data  |  Ch1 data  | ... | ...
 * | +------------+------------+     | +------------+------------+     |
 * +-^------------^------------------+-^------------^------------------+-----
 *   | Ch0 start  | Ch1 start          | Ch0 loop   | Ch1 loop
 *                                     | IRQ address
 *
 * The length of each chunk is given by the interleave size multiplied by the
 * channel count. Each data block must be terminated with the loop end and
 * sustain flags set in order to make the channels "jump" to the next chunk's
 * blocks.
 * - SpicyJpeg
 */
void stream_configureIRQ(Stream *stream){
    uint16_t ctrlReg = SPU_CTRL;

    // Disable the IRQ if an underrun occurs.
    // TODO: handle this in a slightly better way
    if(!stream->_bufferedChunks){
        SPU_CTRL = ctrlReg & ~SPU_CTRL_IRQ_ENABLE;
        return;
    }

    // Exit if the IRQ has been set up before and not yet acknowledged by
    // handleInterrupt().
    if(ctrlReg & SPU_CTRL_IRQ_ENABLE){
        return;
    }

    ChannelMask tempMask = stream->_channelMask;
    uint32_t chunkOffset = stream_getChunkOffset(stream, stream->_head);

    SPU_IRQ_ADDR = chunkOffset / 8;
    SPU_CTRL     = ctrlReg | SPU_CTRL_IRQ_ENABLE;

    for(Channel ch = 0; tempMask; ch++, tempMask >>= 1){
        if(!(tempMask & 1)){
            continue;
        }

        SPU_CH_LOOP_ADDR(ch) = chunkOffset /8;
        chunkOffset         += stream->interleave;
    }
}

void stream_create(Stream *stream){
    stream->_channelMask = 0;
    stream->offset       = 0;
    stream->interleave   = 0;
    stream->numChunks    = 0;
    stream->sampleRate   = 0;
    stream->channels     = 0;

    stream_resetBuffer(stream);
}

bool stream_initFromVAGHeader(Stream *_stream, const VAGHeader *vagHeader, uint32_t _offset, size_t _numChunks){
    if(stream_isPlaying(_stream)){
        return false;
    }
    if(!vagHeader_validateInterleavedMagic(vagHeader)){
        return false;
    }

    stream_resetBuffer(_stream);

    _stream->offset     = _offset;
    _stream->interleave = vagHeader->interleave;
    _stream->numChunks  = _numChunks;
    _stream->sampleRate = vagHeader_getSPUSampleRate(vagHeader);
    _stream->channels   = vagHeader_getNumChannels(vagHeader);
    return true;
}
 
ChannelMask stream_startWithChannelMask(uint16_t left, uint16_t right, ChannelMask mask) {
    if(stream_isPlaying(&stream) || !stream._bufferedChunks){
        return 0;
    }

    mask &= ALL_CHANNELS;

    ChannelMask tempMask = mask;
    uint32_t chunkOffset = stream_getChunkOffset(&stream, stream._head);
    int isRightCh        = 0;

    for(Channel ch = 0; tempMask; ch++, tempMask >>= 1){
        if(!(tempMask & 1)){
            continue;
        }
    
        // Assume each pair of channels is a stero pair. If the channel count is odd,
        // assume the last channel is mono.

        if(isRightCh) {
            SPU_CH_VOL_L(ch) = 0;
            SPU_CH_VOL_R(ch) = right;
        } else if(tempMask != 1){
            SPU_CH_VOL_L(ch) = left;
            SPU_CH_VOL_R(ch) = 0;
        } else {
            SPU_CH_VOL_L(ch) = left;
            SPU_CH_VOL_R(ch) = right;
        }

        SPU_CH_FREQ(ch)  = stream.sampleRate;
        SPU_CH_ADDR(ch)  = chunkOffset / 8;
        SPU_CH_ADSR1(ch) = 0x00ff;
        SPU_CH_ADSR2(ch) = 0x0000;

        chunkOffset += stream.interleave;
        isRightCh   ^= 1;
    }

    stream._channelMask = mask;
    SPU_FLAG_ON1 = mask & 0xffff;
    SPU_FLAG_ON2 = mask >> 16;

    stream_handleInterrupt(&stream);
    return mask;
}

void stream_stop(Stream *stream){
    if(!stream_isPlaying(stream)){
        return;
    }

    SPU_CTRL &= ~SPU_CTRL_IRQ_ENABLE;

    stopChannels(stream->_channelMask);
    stream->_channelMask = 0;
    flushWriteQueue();
    
}

void stream_handleInterrupt(Stream *_stream){
    if(!stream_isPlaying(_stream)){
        return;
    }
    // Disabling the IRQ is always required in order to acknowledge it.
    SPU_CTRL &= ~SPU_CTRL_IRQ_ENABLE;

    _stream->_head = (_stream->_head + 1) % _stream->numChunks;
    _stream->_bufferedChunks--;
    stream_configureIRQ(_stream);
}


size_t stream_feed(Stream *_stream, const void *data, size_t length){
    bool reenableInterrupts = disableInterrupts();

    uintptr_t ptr = (uintptr_t)(data);
    size_t chunkLength = stream_getChunkLength(_stream);
    length = min(length, stream_getFreeChunkCount(_stream) * chunkLength);
    
    for(int i = length; i >= (int)(chunkLength); i -= chunkLength){
        upload(
            stream_getChunkOffset(_stream, _stream->_tail),
            (const void *)(ptr),
            chunkLength,
            true
        );

        ptr += chunkLength;
        _stream->_tail = (_stream->_tail + 1) % _stream->numChunks;
        _stream->_bufferedChunks++;

    }
    if(stream_isPlaying(_stream)){
        stream_configureIRQ(_stream);
    }

    flushWriteQueue();
    if(reenableInterrupts){
        enableInterrupts();
    }
    return length;
}

void stream_resetBuffer(Stream *_stream){
    _stream->_head            = 0;
    _stream->_tail            = 0;
    _stream->_bufferedChunks  = 0;
}

/* Stream State Machine*/
// Public Variables
Stream stream;

// Private Variables
uint8_t streamBuffer[16 * 2048]; // 32 mono chunks or 16 stereo chunks
size_t streamLength;
size_t streamOffset;
uint32_t songLba;
int chunkLength;
int streamFreeChunks;
int feedLength;


StreamStateMachineState streamSMState = STREAM_SM_IDLE;

// TODO:
// Is this function necessary?
void stream_init(void){
    stream_create(&stream);
}

size_t stream_loadSong(const char *name){
    char _songVagHeaderSector[2048];
    VAGHeader _songVagHeader;
    
    songLba = getLbaToFile(name);
    if(!songLba){
        // File not found error.
        return 1;
    }

    // Read the VAG header sector
    startCDROMRead(
        songLba,
        _songVagHeaderSector,
        sizeof(_songVagHeaderSector) / 2048,
        2048,
        true,
        true
    );

    
    // The first sector of music data immediately follows the header's sector.
    // Load it into the stream buffer
    // TODO: Consider turning this into some kind of state machine?
    // Maybe rewrite this entire section once coroutines are added.
    startCDROMRead(
        ++songLba,
        streamBuffer,
        sizeof(streamBuffer) / 2048,
        2048,
        true,
        true
    );
    
    // Directly copy all the data from the VAG header sector to the VAG header struct.
    // The struct is laid out exactly how the header is stored, so this works perfectly.
    __builtin_memcpy(&_songVagHeader, _songVagHeaderSector, sizeof(VAGHeader));

    // Initialise the stream and increment the spuAllocPtr.
    stream_initFromVAGHeader(&stream, &_songVagHeader, spuAllocPtr, 32);
    spuAllocPtr += stream_getChunkLength(&stream) * stream.numChunks;
    chunkLength = stream_getChunkLength(&stream);

    // Set up these variables for the stream state machine to use when streaming more data.
    streamLength = vagHeader_getSPULength(&_songVagHeader) * stream.channels;
    streamOffset = 0;

    // Feed the first buffer-worth of data into the ring buffer, ready for playback.
    streamOffset = stream_feed(&stream, streamBuffer, sizeof(streamBuffer));

    // Ready to play the stream!
    return 0;
}


void stream_update(void){
    
    // Idle:
    // The stream is playing audio. The CDROM isn't reading. There is no data to read.
    // If there are enough free chunks, instruct the CDROM to read more and change
    // state to wait for the data to be ready.
    if(streamSMState == STREAM_SM_IDLE){
        streamFreeChunks = stream_getFreeChunkCount(&stream);
        if(streamFreeChunks >= 8){
            feedLength = min(
                (streamLength) - streamOffset,
                min(streamFreeChunks * chunkLength, sizeof(streamBuffer))
            );
            startCDROMRead(songLba + (streamOffset / 2048), streamBuffer, feedLength / 2048, 2048, true, false);
            streamSMState = STREAM_SM_WAIT_FOR_DATA;
        }
    }

    // Wait For Data:
    // Check if the data is ready. If it is, change state to Data Ready
    if(streamSMState == STREAM_SM_WAIT_FOR_DATA){
        if(cdromDataReady){
            streamSMState = STREAM_SM_DATA_READY;
        }
    }
    
    // Data Ready:
    // The CDROM has finished reading data and the Data Ready flag was raised.
    // Feed this data into the stream ring buffer.
    if(streamSMState == STREAM_SM_DATA_READY){
        // Stream length - stream offset = remaining length
        streamOffset += stream_feed(&stream, streamBuffer, feedLength);
        // If we reached the end of the stream, loop back to the start
        if(streamOffset >= streamLength){
            streamOffset -= streamLength;
        }
        streamSMState = STREAM_SM_IDLE;
    }
}