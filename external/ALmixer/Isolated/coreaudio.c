#ifdef ALMIXER_COMPILE_WITHOUT_SDL

/*
 * SDL_sound Core Audio backend
 * Copyright (C) 2010 Eric Wing <ewing . public @ playcontrol.net>
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef __APPLE__

#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h> /* printf */
#include <al.h>
#include <arpa/inet.h> /* htonl */
#include <AudioToolbox/AudioToolbox.h>

#include "SoundDecoder.h"

#include "SoundDecoder_Internal.h"

typedef struct CoreAudioFileContainer
{
	AudioFileID* audioFileID;
	ExtAudioFileRef extAudioFileRef;
	AudioStreamBasicDescription* outputFormat;
} CoreAudioFileContainer;

static int CoreAudio_init(void);
static void CoreAudio_quit(void);
static int CoreAudio_open(Sound_Sample *sample, const char *ext);
static void CoreAudio_close(Sound_Sample *sample);
static size_t CoreAudio_read(Sound_Sample *sample);
static int CoreAudio_rewind(Sound_Sample *sample);
static int CoreAudio_seek(Sound_Sample *sample, size_t ms);

static const char *extensions_coreaudio[] = 
{
	"aif",
	"aiff",
	"aifc",
	"wav",
	"wave",
	"mp3",
	"mp4",
	"m4a",
	"aac",
	"adts",
	"caf",
	"Sd2f",
	"Sd2",
	"au",
	"snd",
	"next",
	"mp2",
	"mp1",
	"ac3",
	"3gpp",
	"3gp",
	"3gp2",
	"3g2",
	"amrf",
	"amr",
	"ima4",
	"ima",
	NULL 
};
const Sound_DecoderFunctions __Sound_DecoderFunctions_CoreAudio =
{
    {
        extensions_coreaudio,
        "Decode audio through Core Audio through",
        "Eric Wing <ewing . public @ playcontrol.net>",
        "http://playcontrol.net"
    },
	
    CoreAudio_init,       /*   init() method */
    CoreAudio_quit,       /*   quit() method */
    CoreAudio_open,       /*   open() method */
    CoreAudio_close,      /*  close() method */
    CoreAudio_read,       /*   read() method */
    CoreAudio_rewind,     /* rewind() method */
    CoreAudio_seek        /*   seek() method */
};


static int CoreAudio_init(void)
{
    return(1);  /* always succeeds. */
} /* CoreAudio_init */


static void CoreAudio_quit(void)
{
    /* it's a no-op. */
} /* CoreAudio_quit */

/*
   http://developer.apple.com/library/ios/#documentation/MusicAudio/Reference/AudioFileConvertRef/Reference/reference.html
   kAudioFileAIFFType = 'AIFF',
   kAudioFileAIFCType            = 'AIFC',
   kAudioFileWAVEType            = 'WAVE',
   kAudioFileSoundDesigner2Type  = 'Sd2f',
   kAudioFileNextType            = 'NeXT',
   kAudioFileMP3Type             = 'MPG3',
   kAudioFileMP2Type             = 'MPG2',
   kAudioFileMP1Type             = 'MPG1',
   kAudioFileAC3Type             = 'ac-3',
   kAudioFileAAC_ADTSType        = 'adts',
   kAudioFileMPEG4Type           = 'mp4f',
   kAudioFileM4AType             = 'm4af',
   kAudioFileCAFType             = 'caff',
   kAudioFile3GPType             = '3gpp',
   kAudioFile3GP2Type            = '3gp2',
   kAudioFileAMRType             = 'amrf'
*/
static AudioFileTypeID CoreAudio_GetAudioTypeForExtension(const char* file_extension)
{
	if (file_extension == NULL) return 0;
	if( (__Sound_strcasecmp(file_extension, "aif") == 0)
		|| (__Sound_strcasecmp(file_extension, "aiff") == 0)
		|| (__Sound_strcasecmp(file_extension, "aifc") == 0)
	)
	{
		return kAudioFileAIFCType;
	}
	else if( (__Sound_strcasecmp(file_extension, "wav") == 0)
		|| (__Sound_strcasecmp(file_extension, "wave") == 0)
	)
	{
		return kAudioFileWAVEType;
	}
	else if( (__Sound_strcasecmp(file_extension, "mp3") == 0)
	)
	{
		return kAudioFileMP3Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "mp4") == 0)
	)
	{
		return kAudioFileMPEG4Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "m4a") == 0)
	)
	{
		return kAudioFileM4AType;
	}
	else if( (__Sound_strcasecmp(file_extension, "aac") == 0)
	)
	{
		return kAudioFileAAC_ADTSType;
	}
	else if( (__Sound_strcasecmp(file_extension, "adts") == 0)
	)
	{
		return kAudioFileAAC_ADTSType;
	}
	else if( (__Sound_strcasecmp(file_extension, "caf") == 0)
		|| (__Sound_strcasecmp(file_extension, "caff") == 0)
	)
	{
		return kAudioFileCAFType;
	}
	else if( (__Sound_strcasecmp(file_extension, "Sd2f") == 0)
		|| (__Sound_strcasecmp(file_extension, "sd2") == 0)
	)
	{
		return kAudioFileSoundDesigner2Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "au") == 0)
		|| (__Sound_strcasecmp(file_extension, "snd") == 0)
		|| (__Sound_strcasecmp(file_extension, "next") == 0)
	)
	{
		return kAudioFileNextType;
	}
	else if( (__Sound_strcasecmp(file_extension, "mp2") == 0)
	)
	{
		return kAudioFileMP2Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "mp1") == 0)
	)
	{
		return kAudioFileMP1Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "ac3") == 0)
	)
	{
		return kAudioFileAC3Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "3gpp") == 0)
			|| (__Sound_strcasecmp(file_extension, "3gp") == 0)
	)
	{
		return kAudioFile3GPType;
	}
	else if( (__Sound_strcasecmp(file_extension, "3gp2") == 0)
			|| (__Sound_strcasecmp(file_extension, "3g2") == 0)
	)
	{
		return kAudioFile3GP2Type;
	}
	else if( (__Sound_strcasecmp(file_extension, "amrf") == 0)
		|| (__Sound_strcasecmp(file_extension, "amr") == 0)
	)
	{
		return kAudioFileAMRType;
	}
	else if( (__Sound_strcasecmp(file_extension, "ima4") == 0)
		|| (__Sound_strcasecmp(file_extension, "ima") == 0)
	)
	{
		/* not sure about this one */
		return kAudioFileCAFType;
	}
	else
	{
		return 0;
	}

}

static const char* CoreAudio_FourCCToString(int32_t error_code)
{
	static char return_string[16];
	uint32_t big_endian_code = htonl(error_code);
	char* big_endian_str = (char*)&big_endian_code;
	// see if it appears to be a 4-char-code
	if(isprint(big_endian_str[0])
	   && isprint(big_endian_str[1])
	   && isprint(big_endian_str[2])
	   && isprint (big_endian_str[3]))
	{
		return_string[0] = '\'';
		return_string[1] = big_endian_str[0];
		return_string[2] = big_endian_str[1];
		return_string[3] = big_endian_str[2];
		return_string[4] = big_endian_str[3];
		return_string[5] = '\'';
		return_string[6] = '\0';
	}
	else if(error_code > -200000 && error_code < 200000)
	{
		// no, format it as an integer
		snprintf(return_string, 16, "%d", error_code);
	}
	else
	{
		// no, format it as an integer but in hex
		snprintf(return_string, 16, "0x%x", error_code);
	}
	return return_string;
}



SInt64 CoreAudio_SizeCallback(void* inClientData)
{
	ALmixer_RWops* rw_ops = (ALmixer_RWops*)inClientData;
	long current_position = ALmixer_RWtell(rw_ops);
	if (current_position < 0) return -1;
	long end_position = ALmixer_RWseek(rw_ops, 0, SEEK_END);
	if (ALmixer_RWseek(rw_ops, current_position, SEEK_SET) != current_position) return -1;
//	fprintf(stderr, "CoreAudio_SizeCallback:%d\n", end_position);

	return end_position;
}

OSStatus CoreAudio_ReadCallback(
	void* inClientData,
	SInt64 inPosition,
	UInt32 requestCount,
	void* data_buffer,
	UInt32* actualCount
)
{
	ALmixer_RWops* rw_ops = (ALmixer_RWops*)inClientData;
	*actualCount = 0;
	if (inPosition < 0 || inPosition > LONG_MAX ||
	    ALmixer_RWseek(rw_ops, (long)inPosition, SEEK_SET) != inPosition)
		return kAudioFileUnspecifiedError;
	size_t bytes_actually_read = ALmixer_RWread(rw_ops, data_buffer, 1, requestCount);
	if (bytes_actually_read == ALMIXER_RWOPS_ERROR || bytes_actually_read > requestCount)
		return kAudioFileUnspecifiedError;
//	fprintf(stderr, "CoreAudio_ReadCallback:%d, %d\n", requestCount, bytes_actually_read);

	*actualCount = (UInt32) bytes_actually_read;
	return noErr;
}


static int CoreAudio_open(Sound_Sample *sample, const char *ext)
{
	CoreAudioFileContainer* core_audio_file_container;
	AudioFileID* audio_file_id;
	OSStatus error_result;
	Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
	AudioStreamBasicDescription	actual_format;
	AudioStreamBasicDescription	output_format;
	Float64 estimated_duration;
	UInt32 format_size;
	

	core_audio_file_container = (CoreAudioFileContainer*)malloc(sizeof(CoreAudioFileContainer));
	BAIL_IF_MACRO(core_audio_file_container == NULL, ERR_OUT_OF_MEMORY, 0);


	audio_file_id = (AudioFileID*)malloc(sizeof(AudioFileID));
	if (audio_file_id == NULL)
	{
		free(core_audio_file_container);
		BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
	}

	error_result = AudioFileOpenWithCallbacks(
		internal->rw,
		CoreAudio_ReadCallback,
		NULL,
		CoreAudio_SizeCallback,
		NULL,
		CoreAudio_GetAudioTypeForExtension(ext),
		audio_file_id
	);
	if (error_result != noErr)
	{
		free(audio_file_id);
		free(core_audio_file_container);
		SNDDBG(("Core Audio: can't grok data. reason: [%s].\n", CoreAudio_FourCCToString(error_result)));
		BAIL_MACRO("Core Audio: Not valid audio data.", 0);
	} /* if */
	
    format_size = sizeof(actual_format);
    error_result = AudioFileGetProperty(
		*audio_file_id,
		kAudioFilePropertyDataFormat,
		&format_size,
		&actual_format
	);
    if (error_result != noErr)
	{
		AudioFileClose(*audio_file_id);
		free(audio_file_id);
		free(core_audio_file_container);
		SNDDBG(("Core Audio: AudioFileGetProperty failed. reason: [%s]", CoreAudio_FourCCToString(error_result)));
		BAIL_MACRO("Core Audio: Not valid audio data.", 0);
	} /* if */

    format_size = sizeof(estimated_duration);
    error_result = AudioFileGetProperty(
		*audio_file_id,
		kAudioFilePropertyEstimatedDuration,
		&format_size,
		&estimated_duration
	);
    if (error_result != noErr)
	{
		AudioFileClose(*audio_file_id);
		free(audio_file_id);
		free(core_audio_file_container);
		SNDDBG(("Core Audio: AudioFileGetProperty failed. reason: [%s].\n", CoreAudio_FourCCToString(error_result)));
		BAIL_MACRO("Core Audio: Not valid audio data.", 0);
	} /* if */


	core_audio_file_container->audioFileID = audio_file_id;
	

	
	/* If the audio file contains more audio channels than OpenAL supports, then reduce the number of channels
	 * to be decoded by CoreAudio to something that is supported. CoreAudio will downmix it for us in this case.
	 * Note: OpenAL always supports mono (1 channel) and stereo (2 channel) sound.
	 *       This is only an issue for audio files encoded for surround sound, such as 5.1 audio which is 6 channels.
	 */
	for (; actual_format.mChannelsPerFrame > 2; actual_format.mChannelsPerFrame--)
	{
		const char* alFormatName;
		switch (actual_format.mChannelsPerFrame)
		{
			case 8:
				alFormatName = (actual_format.mBitsPerChannel >= 16) ? "AL_FORMAT_71CHN16" : "AL_FORMAT_71CHN8";
				break;
			case 7:
				alFormatName = (actual_format.mBitsPerChannel >= 16) ? "AL_FORMAT_61CHN16" : "AL_FORMAT_61CHN8";
				break;
			case 6:
				alFormatName = (actual_format.mBitsPerChannel >= 16) ? "AL_FORMAT_51CHN16" : "AL_FORMAT_51CHN8";
				break;
			case 4:
				alFormatName = (actual_format.mBitsPerChannel >= 16) ? "AL_FORMAT_QUAD16" : "AL_FORMAT_QUAD8";
				break;
			default:
				alFormatName = NULL;
				break;
		}
		if (alFormatName)
		{
			ALenum alFormatType = alGetEnumValue(alFormatName);
			if ((alFormatType != 0) && (alFormatType != (ALenum)-1))
			{
				break;
			}
		}
	}

    /* Fix for Apple Silicon macOS 26 (Tahoe) regression where mono audio playback fails.
     * Apple's built-in OpenAL implementation on macOS 26+ has a bug where mono audio buffers
     */
#if TARGET_OS_OSX && (defined(__arm64__) || defined(__aarch64__))
    if (actual_format.mChannelsPerFrame == 1){
        actual_format.mChannelsPerFrame = 2;
    }
#endif
    
	if (!(actual_format.mSampleRate > 0) || actual_format.mSampleRate > UINT32_MAX ||
	    actual_format.mChannelsPerFrame == 0 || actual_format.mChannelsPerFrame > UINT8_MAX ||
	    (sample->desired.format != 0 && (sample->desired.format & 0xFF) != 8 &&
	     (sample->desired.format & 0xFF) != 16))
	{
		AudioFileClose(*audio_file_id);
		free(audio_file_id);
		free(core_audio_file_container);
		BAIL_MACRO("Core Audio: Unsupported PCM format", 0);
	}
	sample->flags = SOUND_SAMPLEFLAG_CANSEEK;
	sample->actual.rate = (UInt32) actual_format.mSampleRate;
	sample->actual.channels = (UInt8)actual_format.mChannelsPerFrame;
	internal->total_time = (estimated_duration >= 0 &&
	    estimated_duration * 1000.0 + 0.5 < (double)PTRDIFF_MAX) ?
	    (ptrdiff_t)(estimated_duration * 1000.0 + 0.5) : -1;

#if 0
	/* FIXME: Both Core Audio and SDL 1.3 support float and 32-bit formats */
	if(actual_format.mFormatFlags & kAudioFormatFlagIsBigEndian)
	{
		if(16 == actual_format.mBitsPerChannel)
		{
			if(kAudioFormatFlagIsSignedInteger & actual_format.mFormatFlags)
			{
				sample->actual.format = AUDIO_S16MSB;
			}
			else
			{
				sample->actual.format = AUDIO_U16MSB;				
			}
		}
		else if(8 == actual_format.mBitsPerChannel)
		{
			if(kAudioFormatFlagIsSignedInteger & actual_format.mFormatFlags)
			{
				sample->actual.format = AUDIO_S8;
			}
			else
			{
				sample->actual.format = AUDIO_U8;				
			}
		}
		else // might be 0 for undefined? 
		{
			// This case seems to come up a lot for me. Maybe for file types like .m4a?
			sample->actual.format = AUDIO_S16SYS;
			SNDDBG(("Core Audio: Unsupported actual_format.mBitsPerChannel: [%d].\n", actual_format.mBitsPerChannel));
			
		}
	}
	else // little endian
	{
		if(16 == actual_format.mBitsPerChannel)
		{
			if(kAudioFormatFlagIsSignedInteger & actual_format.mFormatFlags)
			{
				sample->actual.format = AUDIO_S16LSB;
			}
			else
			{
				sample->actual.format = AUDIO_U16LSB;				
			}
		}
		else if(8 == actual_format.mBitsPerChannel)
		{
			if(kAudioFormatFlagIsSignedInteger & actual_format.mFormatFlags)
			{
				sample->actual.format = AUDIO_S8;
			}
			else
			{
				sample->actual.format = AUDIO_U8;				
			}
		}
		else // might be 0 for undefined? 
		{
			sample->actual.format = AUDIO_S16SYS;
			
			SNDDBG(("Core Audio: Unsupported actual_format.mBitsPerChannel: [%d].\n", actual_format.mBitsPerChannel));
		}

	}
#else
	
	
	
    /*
     * I want to use Core Audio to do conversion and decoding for performance reasons.
	 * This is particularly important on mobile devices like iOS.
	 * Taking from the Ogg Vorbis decode, I pretend the "actual" format is the same 
	 * as the desired format. 
     */
    sample->actual.format = (sample->desired.format == 0) ?
	AUDIO_S16SYS : sample->desired.format;
#endif	


	SNDDBG(("CoreAudio: channels == (%d).\n", sample->actual.channels));
	SNDDBG(("CoreAudio: sampling rate == (%d).\n",sample->actual.rate));
	SNDDBG(("CoreAudio: total seconds of sample == (%d).\n", internal->total_time));
	SNDDBG(("CoreAudio: sample->actual.format == (%d).\n", sample->actual.format));


	
	error_result = ExtAudioFileWrapAudioFileID(*audio_file_id,
		false, // set to false for read-only
		&core_audio_file_container->extAudioFileRef
	);
	if(error_result != noErr)
	{
		AudioFileClose(*audio_file_id);
		free(audio_file_id);
		free(core_audio_file_container);
		SNDDBG(("Core Audio: can't wrap data. reason: [%s].\n", CoreAudio_FourCCToString(error_result)));
		BAIL_MACRO("Core Audio: Failed to wrap data.", 0);
	} /* if */


	/* The output format must be linear PCM because that's the only type OpenAL knows how to deal with.
	 * Set the client format to 16 bit signed integer (native-endian) data because that is the most
	 * optimal format on iPhone/iPod Touch hardware.
	 * Maintain the channel count and sample rate of the original source format.
	 */
	output_format.mSampleRate = actual_format.mSampleRate; // preserve the original sample rate
	output_format.mChannelsPerFrame = actual_format.mChannelsPerFrame; // preserve the number of channels
	output_format.mFormatID = kAudioFormatLinearPCM; // We want linear PCM data
	output_format.mFramesPerPacket = 1; // We know for linear PCM, the definition is 1 frame per packet

	if(sample->desired.format == 0)
	{
		// do AUDIO_S16SYS
		output_format.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked; // I seem to read failures problems without kAudioFormatFlagIsPacked. From a mailing list post, this seems to be a Core Audio bug.
		output_format.mBitsPerChannel = 16; // We know we want 16-bit
	}
	else
	{
		output_format.mFormatFlags = 0; // clear flags
		output_format.mFormatFlags |= kAudioFormatFlagIsPacked; // I seem to read failures problems without kAudioFormatFlagIsPacked. From a mailing list post, this seems to be a Core Audio bug.
		// Mask against bitsize
		if((sample->desired.format & 0xFF) == 16)
		{
			output_format.mBitsPerChannel = 16; /* 16-bit */
		}
		else
		{
			output_format.mBitsPerChannel = 8; /* 8-bit */
		}

		// Mask for signed/unsigned
		if((1<<15) & sample->desired.format)
		{
			output_format.mFormatFlags = output_format.mFormatFlags | kAudioFormatFlagIsSignedInteger;

		}
		else
		{
			// no flag set for unsigned
		}
		// Mask for big/little endian
		if((1<<12) & sample->desired.format)
		{
			output_format.mFormatFlags = output_format.mFormatFlags | kAudioFormatFlagIsBigEndian;
		}
		else
		{
			// no flag set for little endian 
		}
	}

	output_format.mBytesPerPacket = output_format.mBitsPerChannel/8 * output_format.mChannelsPerFrame; // e.g. 16-bits/8 * channels => so 2-bytes per channel per frame
	output_format.mBytesPerFrame = output_format.mBitsPerChannel/8 * output_format.mChannelsPerFrame; // For PCM, since 1 frame is 1 packet, it is the same as mBytesPerPacket

	
/*
	output_format.mSampleRate = actual_format.mSampleRate; // preserve the original sample rate
	output_format.mChannelsPerFrame = actual_format.mChannelsPerFrame; // preserve the number of channels
	output_format.mFormatID = kAudioFormatLinearPCM; // We want linear PCM data
//	output_format.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger;
	output_format.mFormatFlags = kAudioFormatFlagsNativeEndian |  kAudioFormatFlagIsSignedInteger;
	output_format.mFramesPerPacket = 1; // We know for linear PCM, the definition is 1 frame per packet
	output_format.mBitsPerChannel = 16; // We know we want 16-bit
	output_format.mBytesPerPacket = 2 * output_format.mChannelsPerFrame; // We know we are using 16-bit, so 2-bytes per channel per frame
	output_format.mBytesPerFrame = 2 * output_format.mChannelsPerFrame; // For PCM, since 1 frame is 1 packet, it is the same as mBytesPerPacket
*/
	SNDDBG(("output_format: mSampleRate: %lf\n", output_format.mSampleRate)); 
	SNDDBG(("output_format: mChannelsPerFrame: %d\n", output_format.mChannelsPerFrame)); 
	SNDDBG(("output_format: mFormatID: %d\n", output_format.mFormatID)); 
	SNDDBG(("output_format: mFormatFlags: %d\n", output_format.mFormatFlags)); 
	SNDDBG(("output_format: mFramesPerPacket: %d\n", output_format.mFramesPerPacket)); 
	SNDDBG(("output_format: mBitsPerChannel: %d\n", output_format.mBitsPerChannel)); 
	SNDDBG(("output_format: mBytesPerPacket: %d\n", output_format.mBytesPerPacket)); 
	SNDDBG(("output_format: mBytesPerFrame: %d\n", output_format.mBytesPerFrame)); 
	
	
	/* Set the desired client (output) data format */
	error_result = ExtAudioFileSetProperty(core_audio_file_container->extAudioFileRef, kExtAudioFileProperty_ClientDataFormat, sizeof(output_format), &output_format);
	if(noErr != error_result)
	{
		ExtAudioFileDispose(core_audio_file_container->extAudioFileRef);
		AudioFileClose(*audio_file_id);
		free(audio_file_id);
		free(core_audio_file_container);
		SNDDBG(("Core Audio: ExtAudioFileSetProperty(kExtAudioFileProperty_ClientDataFormat) failed, reason: [%s].\n", CoreAudio_FourCCToString(error_result)));
		BAIL_MACRO("Core Audio: Not valid audio data.", 0);
	}	


	core_audio_file_container->outputFormat = (AudioStreamBasicDescription*)malloc(sizeof(AudioStreamBasicDescription));
	if (core_audio_file_container->outputFormat == NULL)
	{
		ExtAudioFileDispose(core_audio_file_container->extAudioFileRef);
		AudioFileClose(*audio_file_id);
		free(audio_file_id);
		free(core_audio_file_container);
		BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
	}


	
	/* Copy the output format to the audio_description that was passed in so the 
	 * info will be returned to the user.
	 */
	memcpy(core_audio_file_container->outputFormat, &output_format, sizeof(AudioStreamBasicDescription));

	

	internal->decoder_private = core_audio_file_container;
	return(1);
} /* CoreAudio_open */


static void CoreAudio_close(Sound_Sample *sample)
{
	Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
	CoreAudioFileContainer* core_audio_file_container = (CoreAudioFileContainer *) internal->decoder_private;
	
	free(core_audio_file_container->outputFormat);
	ExtAudioFileDispose(core_audio_file_container->extAudioFileRef);
	AudioFileClose(*core_audio_file_container->audioFileID);
	free(core_audio_file_container->audioFileID);
	free(core_audio_file_container);
		
} /* CoreAudio_close */


static size_t CoreAudio_read(Sound_Sample *sample)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *)sample->opaque;
    CoreAudioFileContainer *container = (CoreAudioFileContainer *)internal->decoder_private;
    UInt32 frame_size = container->outputFormat->mBytesPerFrame;
    size_t total = 0;
    if (frame_size == 0 || internal->buffer_size < frame_size)
    {
        sample->flags |= SOUND_SAMPLEFLAG_ERROR;
        BAIL_MACRO("Core Audio: buffer smaller than a PCM frame", 0);
    }
    while (internal->buffer_size - total >= frame_size)
    {
        size_t remaining = internal->buffer_size - total;
        UInt32 bytes = remaining > UINT32_MAX ? UINT32_MAX : (UInt32)remaining;
        UInt32 frames = bytes / frame_size;
        UInt32 requested_frames = frames;
        AudioBufferList buffers;
        buffers.mNumberBuffers = 1;
        buffers.mBuffers[0].mNumberChannels = container->outputFormat->mChannelsPerFrame;
        buffers.mBuffers[0].mDataByteSize = frames * frame_size;
        buffers.mBuffers[0].mData = (UInt8 *)internal->buffer + total;
        OSStatus result = ExtAudioFileRead(container->extAudioFileRef, &frames, &buffers);
        if (result != noErr || frames > requested_frames)
        {
            sample->flags |= SOUND_SAMPLEFLAG_ERROR;
            SoundDecoder_SetError("Core Audio: failed to read audio frames");
            break;
        }
        if (frames == 0)
        {
            sample->flags |= SOUND_SAMPLEFLAG_EOF;
            break;
        }
        total += (size_t)frames * frame_size;
    }
    if (total < internal->buffer_size &&
        !(sample->flags & (SOUND_SAMPLEFLAG_EOF | SOUND_SAMPLEFLAG_ERROR)))
        sample->flags |= SOUND_SAMPLEFLAG_EAGAIN;
    return total;
} /* CoreAudio_read */


static int CoreAudio_rewind(Sound_Sample *sample)
{
	OSStatus error_result = noErr;	
	Sound_SampleInternal *internal = (Sound_SampleInternal *) sample->opaque;
	CoreAudioFileContainer* core_audio_file_container = (CoreAudioFileContainer *) internal->decoder_private;
	
	error_result = ExtAudioFileSeek(core_audio_file_container->extAudioFileRef, 0);
	if(error_result != noErr)
	{
		BAIL_MACRO("Core Audio: rewind failed", 0);
	}
	return(1);
} /* CoreAudio_rewind */

/* Note: I found this tech note:
 http://developer.apple.com/library/mac/#qa/qa2009/qa1678.html
 I don't know if this applies to us. So far, I haven't noticed the problem,
 so I haven't applied any of the techniques.
 */
static int CoreAudio_seek(Sound_Sample *sample, size_t ms)
{
    Sound_SampleInternal *internal = (Sound_SampleInternal *)sample->opaque;
    CoreAudioFileContainer *container = (CoreAudioFileContainer *)internal->decoder_private;
    /* The output keeps the file's sample rate; PCM has one frame per packet. */
    double frames = (ms / 1000.0) * container->outputFormat->mSampleRate;
    BAIL_IF_MACRO(!(frames >= 0) || frames >= (double)INT64_MAX,
                  "Core Audio: seek out of range", 0);
    BAIL_IF_MACRO(ExtAudioFileSeek(container->extAudioFileRef, (SInt64)frames) != noErr,
                  "Core Audio: seek failed", 0);
    return 1;
} /* CoreAudio_seek */

#endif /* __APPLE__ */


#endif /* ALMIXER_COMPILE_WITHOUT_SDL */

