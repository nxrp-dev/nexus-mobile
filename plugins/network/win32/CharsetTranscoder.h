//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _CharsetTranscoder_H_
#define _CharsetTranscoder_H_

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include "windows.h"
#include <map>
#include <string>

typedef std::map<std::string, int>	CharsetNameCodepageMap;

class CharsetTranscoder
{
public:

	static bool isSupportedEncoding( const char *charset );
	static bool transcode( std::string *text, const char *srcCharset, const char *dstCharset );

private:

	static bool isInitialized;
	static CharsetNameCodepageMap charsetCodepageMap;

	static void initialize( );
	static int getCodepageForCharset( const char *charset );
	static void defineCharset(const char *charset, int codepage, const char *description);

};

#endif