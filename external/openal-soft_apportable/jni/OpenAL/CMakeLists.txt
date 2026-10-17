cmake_minimum_required(VERSION 3.4.1)
project( openal )

include_directories(
	include
	OpenAL32/Include
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}	-fpic -ffunction-sections -funwind-tables -fstack-protector -fno-short-enums")

add_definitions(
	-DAL_BUILD_LIBRARY
	-DAL_ALEXT_PROTOTYPES
#	-DPOST_FROYO
	-DHAVE_GCC_VISIBILITY
	-DOPENAL_FIXED_POINT
	-DOPENAL_FIXED_POINT_SHIFT=16
	-DMAX_SOURCES_LOW=32
	-DMAX_SOURCES_START=32
	-DMAX_SOURCES_HIGH=32
)

add_library( openal SHARED 
	Alc/android.c
	OpenAL32/alAuxEffectSlot.c
	OpenAL32/alBuffer.c
	OpenAL32/alDatabuffer.c
	OpenAL32/alEffect.c
	OpenAL32/alError.c
	OpenAL32/alExtension.c
	OpenAL32/alFilter.c
	OpenAL32/alListener.c
	OpenAL32/alSource.c
	OpenAL32/alState.c
	OpenAL32/alThunk.c
	Alc/ALc.c
	Alc/alcConfig.c
	Alc/alcEcho.c
	Alc/alcModulator.c
	Alc/alcReverb.c
	Alc/alcRing.c
	Alc/alcThread.c
	Alc/ALu.c
	Alc/bs2b.c
	Alc/null.c
	Alc/panning.c
	Alc/mixer.c
	Alc/audiotrack.c
	Alc/opensles.c
)

target_link_libraries ( openal log dl OpenSLES )
