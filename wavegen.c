#include "wavegen.h"
#include <string.h>

static const uint8_t RIFF_MAGIC[4] = "RIFF";
static const uint8_t WAVE_MAGIC[4] = "WAVE";
static const uint8_t SUB1_MAGIC[4] = "fmt ";
static const uint8_t SUB2_MAGIC[4] = "data";

#define SUB1_SIZE 16
#define CHUNK_SIZE 36
#define AUDIO_FORMAT_PCM  1
#define AUDIO_FORMAT_IEEE 3

/* https://stackoverflow.com/questions/4239993/determining-endianness-at-compile-time */
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == _ORDER_BIG_ENDIAN__ || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)

static inline uint16_t short2le(uint16_t n) {
	union {
		uint16_t i;
	    uint8_t a[2];
    } conv;
    conv.i = n;

    return  (uint16_t) (conv.a[0] << 8) |
            (uint16_t) (conv.a[1]);
}

static inline uint16_t long2le(uint32_t n) {
    union {
        uint32_t i;
        uint8_t a[4];
    } conv;
    conv.i = n;

    return  (uint32_t) (conv.a[0] << 24) |
            (uint32_t) (conv.a[1] << 16) |
            (uint32_t) (conv.a[2] << 8) |
            (uint32_t) (conv.a[3]);
}

#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == _ORDER_LITTLE_ENDIAN__ || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)

#define short2le(n) n
#define long2le(n) n

#else
#error "Please define __BIG_ENDIAN__ or __LITTLE_ENDIAN__ at beginning of the header."
#endif

/* Does not check size */
static inline int verify_header(WaveHeader *wh) {
    return (memcmp(wh->ChunkID, RIFF_MAGIC, sizeof(RIFF_MAGIC)) == 0)
        && (memcmp(wh->Format, WAVE_MAGIC, sizeof(WAVE_MAGIC)) == 0)

        && (memcmp(wh->Subchunk1ID, SUB1_MAGIC, sizeof(SUB1_MAGIC)) == 0)
        && (wh->Subchunk1Size == short2le(SUB1_SIZE))
        && (wh->AudioFormat == short2le(WFMT_PCM) || wh->AudioFormat == short2le(WFMT_IEEE))
        
        && (memcmp(wh->Subchunk2ID, SUB2_MAGIC, sizeof(SUB2_MAGIC)) == 0);
}

int wavegen_open_read(WaveFile *wf, const char *fname) {
    WaveHeader theHeader;

    wf->f = fopen(fname, "r");
    if(!wf->f) return 0;

    fread(&theHeader, sizeof(WaveHeader), 1, wf->f);
    if(!verify_header(&theHeader)) return 0;

    wf->channels = short2le(theHeader.NumChannels);
    wf->samplerate = long2le(theHeader.SampleRate);
    wf->samplebytes = short2le(theHeader.BitsPerSample) / 8;
    wf->sampleqty = long2le(theHeader.Subchunk2Size) / (wf->samplebytes * wf->channels);
    wf->fmt = short2le(theHeader.AudioFormat);
    return 1;
}

int wavegen_open_write(WaveFile *wf, const char *fname) {
    wf->f = fopen(fname, "w");
    if(!wf->f) return 0;

    fseek(wf->f, sizeof(WaveHeader), SEEK_SET);
    return 1;
}

int wavegen_close_read(WaveFile *wf) {
    fclose(wf->f);
    wf->f = NULL;
}

int wavegen_close_write(WaveFile *wf) {
    WaveHeader theHeader;

    fseek(wf->f, 0, SEEK_SET);
    memcpy(theHeader.ChunkID, RIFF_MAGIC, sizeof(RIFF_MAGIC));
    memcpy(theHeader.Format, WAVE_MAGIC, sizeof(WAVE_MAGIC));
    
    memcpy(theHeader.Subchunk1ID, SUB1_MAGIC, sizeof(WAVE_MAGIC));
    theHeader.Subchunk1Size     = long2le(SUB1_SIZE);
    theHeader.AudioFormat       = short2le(wf->fmt);
    theHeader.NumChannels       = short2le(wf->channels);
    theHeader.SampleRate        = long2le(wf->samplerate);
    theHeader.ByteRate          = long2le(wf->samplerate * wf->channels * wf->samplebytes);
    theHeader.BlockAlign        = short2le(wf->channels * wf->samplebytes);
    theHeader.BitsPerSample     = short2le(8*wf->samplebytes);

    memcpy(theHeader.Subchunk2ID, SUB2_MAGIC, sizeof(SUB2_MAGIC));
    theHeader.Subchunk2Size     = long2le(wf->samplebytes * wf->sampleqty * wf->channels);
    theHeader.ChunkSize         = long2le(wf->samplebytes * wf->sampleqty * wf->channels + CHUNK_SIZE);
    fwrite(&theHeader, sizeof(WaveHeader), 1, wf->f);

    fclose(wf->f);
    wf->f = NULL;
}

size_t wavegen_read_il(WaveFile *wf, void *ptr, size_t len) {
    size_t qty = fread(ptr, wf->samplebytes, len * wf->channels, wf->f);
    return qty;
}

size_t wavegen_write_il(WaveFile *wf, void *ptr, size_t len) {
    size_t qty = fwrite(ptr, wf->samplebytes, len * wf->channels, wf->f);
    wf->sampleqty += qty;
    return qty;
}

size_t wavegen_read_stack(WaveFile *wf, void *ptr, size_t len) {
    const size_t blksize = len * wf->channels * wf->samplebytes;
    size_t qty = 0;

    for(size_t sample = 0; sample < len; ++sample) {
        for(size_t ch = 0; ch < wf->channels; ++ch) {
            qty += fread(ptr + (blksize * ch) + (sample * wf->samplebytes), wf->samplebytes, 1, wf->f);
        }
    }
}

size_t wavegen_write_stack(WaveFile *wf, void *ptr, size_t len) {
    const size_t blksize = len * wf->samplebytes;
    size_t qty = 0;

    for(size_t sample = 0; sample < len; ++sample) {
        for(size_t ch = 0; ch < wf->channels; ++ch) {
            qty += fwrite(ptr + (blksize * ch) + (sample * wf->samplebytes), wf->samplebytes, 1, wf->f);
        }
    }

    wf->sampleqty += qty;
}

const char *wavegen_fmt2str(WaveAudioFormat f) {
    switch(f) {
    case WFMT_PCM:  return "16-bit LE PCM";
    case WFMT_IEEE: return "IEEE 32-bit Float";
    }

    return NULL;
}

