#ifndef SOUNDDECODER_INTERNAL_H
#define SOUNDDECODER_INTERNAL_H

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include "al.h" /* OpenAL */
#include "ALmixer_RWops.h"
#include "SoundDecoder.h"

#ifdef __cplusplus
extern "C" {
#endif
	

typedef struct SoundDecoder_DecoderFunctions
{
    const SoundDecoder_DecoderInfo info;
    int (*init)(void);
    void (*quit)(void);
    /* Failed open releases decoder-owned allocations; the dispatcher owns rw. */
    int (*open)(SoundDecoder_Sample *sample, const char *ext);
    void (*close)(SoundDecoder_Sample *sample);
    /* Valid bytes written (including partial data); never an unsigned error.
       Set ERROR/EOF separately. A zero read without EOF cannot complete DecodeAll. */
    size_t (*read)(SoundDecoder_Sample *sample);
    /* Rewind/seek: nonzero only on success; zero preserves failure. */
    int (*rewind)(SoundDecoder_Sample *sample);
    int (*seek)(SoundDecoder_Sample *sample, size_t ms);
} SoundDecoder_DecoderFunctions;

typedef struct SoundDecoder_DecoderFunctions Sound_DecoderFunctions;



typedef struct SoundDecoder_SampleInternal
{
    ALmixer_RWops *rw;
    const SoundDecoder_DecoderFunctions *funcs;
    void *buffer;
    size_t buffer_size;
    void *decoder_private;
    ptrdiff_t total_time;
} SoundDecoder_SampleInternal;

typedef struct SoundDecoder_SampleInternal Sound_SampleInternal;

#define BAIL_MACRO(e, r) { SoundDecoder_SetError(e); return r; }
#define BAIL_IF_MACRO(c, e, r) if (c) { SoundDecoder_SetError(e); return r; }
#define ERR_OUT_OF_MEMORY "Out of memory"
#define ERR_NOT_INITIALIZED "SoundDecoder not initialized"
#define ERR_UNSUPPORTED_FORMAT "Unsupported codec"
#define ERR_NULL_SAMPLE "Sound sample is NULL"
#define ERR_PREVIOUS_SAMPLE_ERROR "Cannot operate on sample due to previous error"
#define ERR_ALREADY_AT_EOF_ERROR "Cannot operate on sample because already at EOF"

#ifdef __ANDROID__

/* This macro crashes when a format string is used. 
 * Provide a real function instead.
#include <android/log.h>
#define SNDDBG(x) __android_log_print(ANDROID_LOG_INFO, "Corona", x);
*/
/* Android doesn't seem to understand the 'restrict' qualifier. */
int SoundDecoder_DebugPrint(const char* format, ...);
#define SNDDBG(x) SoundDecoder_DebugPrint x

#else

#if (defined DEBUG_CHATTER)
#define SNDDBG(x) printf x
#else
#define SNDDBG(x)
#endif

#endif

void SoundDecoder_SetError(const char* err_str, ...);
#define __Sound_SetError SoundDecoder_SetError

/* Convert time without an intermediate float or overflowing multiplication. */
static inline int SoundDecoder_MillisecondsToFrames(size_t ms, uint32_t rate,
                                                    uint64_t limit, uint64_t *frames)
{
    uint64_t seconds = ms / 1000;
    uint64_t fraction = ((uint64_t)(ms % 1000) * rate) / 1000;
    if (rate == 0 || seconds > limit / rate)
        return 0;
    *frames = seconds * rate;
    if (fraction > limit - *frames)
        return 0;
    *frames += fraction;
    return 1;
}

/* Unknown or unrepresentable durations use the public API's -1 sentinel. */
static inline ptrdiff_t SoundDecoder_DurationFrom100ns(int64_t ticks)
{
    if (ticks < 0) return -1;
    uint64_t milliseconds = (uint64_t)ticks / 10000 + ((ticks % 10000) >= 5000);
    return milliseconds <= PTRDIFF_MAX ? (ptrdiff_t)milliseconds : -1;
}

int SoundDecoder_strcasecmp(const char *x, const char *y);
#define __Sound_strcasecmp SoundDecoder_strcasecmp

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif

