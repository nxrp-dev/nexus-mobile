/*
 * SDL_sound -- An abstract sound format decoding API.
 * Copyright (C) 2001  Ryan C. Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Ogg Vorbis decoder for SDL_sound.
 *
 * This driver handles .OGG audio files, and depends on libvorbisfile to
 *  do the actual decoding work. libvorbisfile is part of libvorbis, which
 *  is part of the Ogg Vorbis project.
 *
 *   Ogg Vorbis: http://www.xiph.org/ogg/vorbis/
 *   vorbisfile documentation: http://www.xiph.org/ogg/vorbis/doc/vorbisfile/
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon. (icculus@icculus.org)
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef SOUND_SUPPORTS_OGG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "SoundDecoder.h"

#include "SoundDecoder_Internal.h"
#include "SDL_endian_minimal.h"
#include "ALmixer_RWops.h"

//#include <vorbis/codec.h>
#include <ivorbisfile.h>
#define ERR_IO_ERROR "I/O error"


static int OGG_init(void);
static void OGG_quit(void);
static int OGG_open(Sound_Sample *sample, const char *ext);
static void OGG_close(Sound_Sample *sample);
static size_t OGG_read(Sound_Sample *sample);
static int OGG_rewind(Sound_Sample *sample);
static int OGG_seek(Sound_Sample *sample, size_t ms);

static const char *extensions_ogg[] = { "OGG", NULL };
const Sound_DecoderFunctions __Sound_DecoderFunctions_OGG =
{
    {
        extensions_ogg,
        "Ogg Vorbis audio through VorbisFile",
        "Ryan C. Gordon <icculus@icculus.org>",
        "http://www.icculus.org/SDL_sound/"
    },

    OGG_init,       /*   init() method */
    OGG_quit,       /*   quit() method */
    OGG_open,       /*   open() method */
    OGG_close,      /*  close() method */
    OGG_read,       /*   read() method */
    OGG_rewind,     /* rewind() method */
    OGG_seek        /*   seek() method */
};


static int OGG_init(void)
{
    return(1);  /* always succeeds. */
} /* OGG_init */


static void OGG_quit(void)
{
    /* it's a no-op. */
} /* OGG_quit */



    /*
     * These are callbacks from vorbisfile that let them read data from
     *  a RWops...
     */

static size_t RWops_ogg_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    Sound_Sample *sample = (Sound_Sample *) datasource;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    size_t count = ALmixer_RWread(internal->rw, ptr, size, nmemb);
    if (count == ALMIXER_RWOPS_ERROR)
    {
        sample->flags |= SOUND_SAMPLEFLAG_ERROR;
        errno = EIO;
        SoundDecoder_SetError(ERR_IO_ERROR);
        return 0;
    }
    if (count == 0) errno = 0; /* Vorbis distinguishes EOF from error via errno. */
    return count;
} /* RWops_ogg_read */

static int RWops_ogg_seek(void *datasource, ogg_int64_t offset, int whence)
{
    Sound_Sample *sample = (Sound_Sample *) datasource;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    if (offset < LONG_MIN || offset > LONG_MAX)
        return -1;
    return ALmixer_RWseek(internal->rw, (long)offset, whence) < 0 ? -1 : 0;
} /* RWops_ogg_seek */

static int RWops_ogg_close(void *datasource)
{
    /* do nothing; SDL_sound will delete the RWops at a higher level. */
    return(0);  /* this is success in fclose(), so I guess that's okay. */
} /* RWops_ogg_close */

static long RWops_ogg_tell(void *datasource)
{
    Sound_Sample *sample = (Sound_Sample *) datasource;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    return ALmixer_RWtell(internal->rw);
} /* RWops_ogg_tell */

static const ov_callbacks RWops_ogg_callbacks =
{
    RWops_ogg_read,
    RWops_ogg_seek,
    RWops_ogg_close,
    RWops_ogg_tell
};


    /* Return a human readable version of an VorbisFile error code... */
#if (defined DEBUG_CHATTER)
static const char *ogg_error(int errnum)
{
    switch(errnum)
    {
        case OV_EREAD:
            return("i/o error");
        case OV_ENOTVORBIS:
            return("not a vorbis file");
        case OV_EVERSION:
            return("Vorbis version mismatch");
        case OV_EBADHEADER:
            return("invalid Vorbis bitstream header");
        case OV_EFAULT:
            return("internal logic fault in Vorbis library");
    } /* switch */

    return("unknown error");
} /* ogg_error */
#endif

static __inline__ void output_ogg_comments(OggVorbis_File *vf)
{
#if (defined DEBUG_CHATTER)
    int i;
    vorbis_comment *vc = ov_comment(vf, -1);

    if (vc == NULL)
        return;

    SNDDBG(("OGG: vendor == [%s].\n", vc->vendor));
    for (i = 0; i < vc->comments; i++)
    {
        SNDDBG(("OGG: user comment [%s].\n", vc->user_comments[i]));
    } /* for */
#endif
} /* output_ogg_comments */


static int OGG_open(Sound_Sample *sample, const char *ext)
{
    int rc;
    ogg_int64_t total_time;
    OggVorbis_File *vf;
    vorbis_info *info;
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;

    vf = (OggVorbis_File *) malloc(sizeof (OggVorbis_File));
    BAIL_IF_MACRO(vf == NULL, ERR_OUT_OF_MEMORY, 0);

    rc = ov_open_callbacks(sample, vf, NULL, 0, RWops_ogg_callbacks);
    if (rc != 0)
    {
#if (defined DEBUG_CHATTER)		
        SNDDBG(("OGG: can't grok data. reason: [%s].\n", ogg_error(rc)));
#endif
        free(vf);
        BAIL_MACRO("OGG: Not valid Ogg Vorbis data.", 0);
    } /* if */

    if (sample->flags & SOUND_SAMPLEFLAG_ERROR)
    {
        ov_clear(vf);
        free(vf);
        BAIL_MACRO(ERR_IO_ERROR, 0);
    }
    info = ov_info(vf, -1);
    if (info == NULL || info->rate <= 0 || (uint64_t)info->rate > UINT32_MAX ||
        info->channels <= 0 || info->channels > UINT8_MAX)
    {
        ov_clear(vf);
        free(vf);
        BAIL_MACRO("OGG: failed to retrieve bitstream info", 0);
    } /* if */

    output_ogg_comments(vf);
	
    SNDDBG(("OGG: bitstream version == (%d).\n", info->version));
    SNDDBG(("OGG: bitstream channels == (%d).\n", info->channels));
    SNDDBG(("OGG: bitstream sampling rate == (%ld).\n", info->rate));
    SNDDBG(("OGG: seekable == {%s}.\n", ov_seekable(vf) ? "TRUE" : "FALSE"));
    SNDDBG(("OGG: number of logical bitstreams == (%ld).\n", ov_streams(vf)));
    SNDDBG(("OGG: serial number == (%ld).\n", ov_serialnumber(vf, -1)));
    SNDDBG(("OGG: total seconds of sample == (%f).\n", ov_time_total(vf, -1)));

    internal->decoder_private = vf;
    sample->flags = ov_seekable(vf) ? SOUND_SAMPLEFLAG_CANSEEK : SOUND_SAMPLEFLAG_NONE;
    sample->actual.rate = (uint32_t) info->rate;
    sample->actual.channels = (uint8_t) info->channels;
    total_time = ov_time_total(vf, -1);
    if (total_time < 0 || (uint64_t)total_time > PTRDIFF_MAX)
      internal->total_time = -1;
    else
      internal->total_time = (ptrdiff_t)total_time;


    /* Tremor always produces native-endian signed 16-bit PCM. */
    sample->actual.format = AUDIO_S16SYS;
    return(1);
} /* OGG_open */


static void OGG_close(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    OggVorbis_File *vf = (OggVorbis_File *) internal->decoder_private;
    ov_clear(vf);
    free(vf);
} /* OGG_close */

/* Note: According to the Vorbis documentation:
 * "ov_read() will  decode at most one vorbis packet per invocation, 
 * so the value returned will generally be less than length."
 * Due to this, for buffer sizes like 16384, SDL_Sound was always getting
 * an underfilled buffer and always setting the EAGAIN flag.
 * Since the SDL_Sound API implies that the entire buffer
 * should be filled unless EOF, additional code has been added
 * to this function to call ov_read() until the buffer is filled.
 * However, there may still be some corner cases where the buffer
 * cannot be entirely filled. So be aware.
 */
static size_t OGG_read(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    OggVorbis_File *vf = (OggVorbis_File *) internal->decoder_private;
    size_t total = 0;
    size_t frame_size = sample->actual.channels * ((sample->actual.format & 0xFF) / 8);
    int bitstream;
    if (frame_size == 0 || internal->buffer_size < frame_size)
    {
        sample->flags |= SOUND_SAMPLEFLAG_ERROR;
        BAIL_MACRO("OGG: buffer smaller than a PCM frame", 0);
    }
    while (internal->buffer_size - total >= frame_size)
    {
        size_t remaining = internal->buffer_size - total;
        int request = (int)(remaining > INT_MAX ? INT_MAX : remaining);
        long count;
        request -= request % frame_size;
        count = ov_read(vf, (char *)internal->buffer + total, request, &bitstream);
        if (count < 0 || count > request)
        {
            sample->flags |= SOUND_SAMPLEFLAG_ERROR;
            SoundDecoder_SetError(ERR_IO_ERROR);
            break;
        }
        total += (size_t)count;
        if (sample->flags & SOUND_SAMPLEFLAG_ERROR) break;
        if (count == 0)
        {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        }
    }
    if (total < internal->buffer_size &&
        !(sample->flags & (SOUND_SAMPLEFLAG_EOF | SOUND_SAMPLEFLAG_ERROR)))
        sample->flags |= SOUND_SAMPLEFLAG_EAGAIN;
    return total;
} /* OGG_read */


static int OGG_rewind(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    OggVorbis_File *vf = (OggVorbis_File *) internal->decoder_private;

    BAIL_IF_MACRO(ov_raw_seek(vf, 0) < 0, ERR_IO_ERROR, 0);
    return(1);
} /* OGG_rewind */


static int OGG_seek(Sound_Sample *sample, size_t ms)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
    OggVorbis_File *vf = (OggVorbis_File *) internal->decoder_private;
	/* Unlike Vorbis, Tremor uses integer milliseconds instead of double seconds. */
    BAIL_IF_MACRO((uint64_t)ms > INT64_MAX, "OGG: seek out of range", 0);
    BAIL_IF_MACRO(ov_time_seek(vf, (ogg_int64_t)ms) < 0, ERR_IO_ERROR, 0);
    return(1);
} /* OGG_seek */

#endif /* SOUND_SUPPORTS_OGG */


/* end of ogg.c ... */

