#ifndef WAVEGEN_H
#define WAVEGEN_H

#include <stdio.h>
#include <stdint.h>

/* Define endianness here if error occurs */
/*#define __BIG_ENDIAN__*/
/*#define __LITTLE_ENDIAN*/

typedef enum {
    WFMT_PCM=1,
    WFMT_IEEE=3
} WaveAudioFormat;

#pragma pack(push, 1)
typedef struct {
    uint8_t     ChunkID[4];
    uint32_t    ChunkSize;
    uint8_t     Format[4];

    uint8_t     Subchunk1ID[4];
    uint32_t    Subchunk1Size;
    uint16_t    AudioFormat;
    uint16_t    NumChannels;
    uint32_t    SampleRate;
    uint32_t    ByteRate;
    uint16_t    BlockAlign;
    uint16_t    BitsPerSample;

    uint8_t     Subchunk2ID[4];
    uint32_t    Subchunk2Size;
} WaveHeader;
#pragma pack(pop)

typedef struct {
    FILE            *f;
    uint16_t        channels;
    uint32_t        samplerate;
    uint32_t        sampleqty;
    uint16_t        samplebytes;
    WaveAudioFormat fmt;
} WaveFile;

int wavegen_open_read(WaveFile *wf, const char *fname);
int wavegen_open_write(WaveFile *wf, const char *fname);
int wavegen_close_read(WaveFile *wf);
int wavegen_close_write(WaveFile *wf);

size_t wavegen_read_il(WaveFile *wf, void *ptr, size_t len);
size_t wavegen_write_il(WaveFile *wf, void *ptr, size_t len);
size_t wavegen_read_stack(WaveFile *wf, void *ptr, size_t len);
size_t wavegen_write_stack(WaveFile *wf, void *ptr, size_t len);

const char *wavegen_fmt2str(WaveAudioFormat f);
    
#define wavegen_isopen(wf) ((wf)->f)

#endif /* WAVEGEN_H */
