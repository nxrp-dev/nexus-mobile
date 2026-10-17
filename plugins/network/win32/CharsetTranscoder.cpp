//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "CharsetTranscoder.h"

#include "WindowsNetworkSupport.h"

#include <algorithm>

bool CharsetTranscoder::isInitialized = false;
CharsetNameCodepageMap CharsetTranscoder::charsetCodepageMap;

void CharsetTranscoder::defineCharset(const char *charset, int codepage, const char *description)
{
	std::string charsetString = charset;
	charsetCodepageMap[charsetString] = codepage;
}

void CharsetTranscoder::initialize( )
{
	// The following are the charsets that should be supported from *any* Windows (PC) environment, per:
	//
	//     http://msdn.microsoft.com/en-us/library/aa288104(v=vs.71).aspx
	//
	// This more recent reference indicates that Windows should support several additional ISO code
	// pages, so those have been added.  Also, some common aliases have been added (such as gbk for gb2312).
	//
	//     http://msdn.microsoft.com/en-us/goglobal/bb964656.aspx
	//
	// It is possible that additional charset support could be available in some installations, but this
	// code will not identify or support them :(
	//
	// If you feel sad that we aren't supporting every known charset in the universe, see the list
	// of web site charset popularity here and you will feel better:
	//
	//     http://w3techs.com/technologies/overview/character_encoding/all
	//
	//
	defineCharset( "ansi_x3.4-1968",		1252,		"Western" );
	defineCharset( "ansi_x3.4-1986",		1252,		"Western" );
	defineCharset( "ascii",					1252,		"Western" );
	defineCharset( "big5",					950,		"Traditional Chinese (BIG5)" );
	defineCharset( "chinese",				936,		"Chinese Simplified" );
	defineCharset( "cp367",					1252,		"Western" );
	defineCharset( "cp819", 				1252,		"Western" );
	defineCharset( "csascii", 				1252,		"Western" );
	defineCharset( "csbig5", 				950,		"Traditional Chinese (BIG5)" );
	defineCharset( "cseuckr", 				949,		"Korean" );
	defineCharset( "cseucpkdfmtjapanese", 	932,		"Japanese (EUC)" );
	defineCharset( "csgb2312", 				936,		"Chinese Simplified (GB2312)" );
	defineCharset( "csiso2022jp", 			932,		"Japanese (JIS-Allow 1 byte Kana)" );
	defineCharset( "csiso2022kr", 			50225,		"Korean (ISO)" );
	defineCharset( "csiso58gb231280", 		936,		"Chinese Simplified (GB2312)" );
	defineCharset( "csisolatin2", 			28592,		"Central European (ISO)" );
	defineCharset( "csisolatinhebrew", 		1255,		"Hebrew (ISO-Visual)" );
	defineCharset( "cskoi8r", 				20866,		"Cyrillic (KOI8-R)" );
	defineCharset( "csksc56011987",			949,		"Korean" );
	defineCharset( "csshiftjis", 			932,		"Shift-JIS" );
	defineCharset( "euc-kr", 				949,		"Korean" );
	defineCharset( "gb2312", 				936,		"Chinese Simplified (GB2312)" );
	defineCharset( "gb_2312-80", 			936,		"Chinese Simplified (GB2312)" );
	defineCharset( "gbk",	 				936,		"Chinese Simplified (GB2312)" );
	defineCharset( "hebrew", 				1255,		"Hebrew" );
	defineCharset( "hz-gb-2312", 			936,		"Chinese Simplified (HZ)" );
	defineCharset( "ibm367", 				1252,		"Western" );
	defineCharset( "ibm819", 				1252,		"Western" );
	defineCharset( "ibm852", 				852,		"Central European (DOS)" );
	defineCharset( "ibm866", 				866,		"Cyrillic (DOS)" );
	defineCharset( "iso-2022-jp", 			932,		"Japanese (JIS)" );
	defineCharset( "iso-2022-kr", 			50225,		"Korean (ISO)" );
	defineCharset( "iso-8859-1", 			1252,		"Western" );
	defineCharset( "iso-8859-2", 			28592,		"Central European (ISO)" );
	defineCharset( "iso-8859-3",			28593,		"ISO 8859-3 Latin 3" );
	defineCharset( "iso-8859-4",			28594,		"ISO 8859-4 Baltic" );
	defineCharset( "iso-8859-5",			28595,		"ISO 8859-5 Cyrillic" );
	defineCharset( "iso-8859-6",			28596,		"ISO 8859-6 Arabic" );
	defineCharset( "iso-8859-7",			28597,		"ISO 8859-7 Greek" );
	defineCharset( "iso-8859-8", 			1255,		"Hebrew (ISO-Visual)" );
	defineCharset( "iso-8859-9",			28599,		"ISO 8859-9 Turkish" );
	defineCharset( "iso-8859-11",			874,		"Thai" );
	defineCharset( "iso-8859-13",			28603,		"ISO 8859-13 Estonian" );
	defineCharset( "iso-8859-15",			28605,		"ISO 8859-15 Latin 9" );
	defineCharset( "iso-ir-100", 			1252,		"Western" );
	defineCharset( "iso-ir-101", 			28592,		"Central European (ISO)" );
	defineCharset( "iso-ir-138", 			1255,		"Hebrew (ISO-Visual)" );
	defineCharset( "iso-ir-149", 			949,		"Korean" );
	defineCharset( "iso-ir-58",				936,		"Chinese Simplified (GB2312)" );
	defineCharset( "iso-ir-6",				1252,		"Western" );
	defineCharset( "iso646-us",				1252,		"Western" );
	defineCharset( "iso8859-1",				1252,		"Western" );
	defineCharset( "iso8859-2",				28592,		"Central European (ISO)" );
	defineCharset( "iso_646.irv:1991", 		1252,		"Western" );
	defineCharset( "iso_8859-1", 			1252,		"Western" );
	defineCharset( "iso_8859-1:1987", 		1252,		"Western" );
	defineCharset( "iso_8859-2", 			28592,		"Central European (ISO)" );
	defineCharset( "iso_8859-2:1987", 		28592,		"Central European (ISO)" );
	defineCharset( "iso_8859-8", 			1255,		"Hebrew (ISO-Visual)" );
	defineCharset( "iso_8859-8:1988", 		1255,		"Hebrew (ISO-Visual)" );
	defineCharset( "koi8-r", 				20866,		"Cyrillic (KOI8-R)" );
	defineCharset( "korean", 				949,		"Korean" );
	defineCharset( "ks-c-5601",				949,		"Korean" );
	defineCharset( "ks-c-5601-1987",		949,		"Korean" );
	defineCharset( "ks_c_5601",				949,		"Korean" );
	defineCharset( "ks_c_5601-1987", 		949,		"Korean" );
	defineCharset( "ks_c_5601-1989", 		949,		"Korean" );
	defineCharset( "ksc-5601", 				949,		"Korean" );
	defineCharset( "ksc5601", 				949,		"Korean" );
	defineCharset( "ksc_5601", 				949,		"Korean" );
	defineCharset( "l2", 					28592,		"Central European (ISO)" );
	defineCharset( "latin1", 				1252,		"Western" );
	defineCharset( "latin2", 				28592,		"Central European (ISO)" );
	defineCharset( "ms_kanji", 				932,		"Shift-JIS" );
	defineCharset( "shift-jis",				932,		"Shift-JIS" );
	defineCharset( "shift_jis",				932,		"Shift-JIS" );
	defineCharset( "tis-620",				874,		"Thai" );
	defineCharset( "us", 					1252,		"Western" );
	defineCharset( "us-ascii", 				1252,		"Western" );
	defineCharset( "utf-7",					CP_UTF7,	"Unicode (UTF-7)" );
	defineCharset( "utf-8",					CP_UTF8,	"Unicode (UTF-8)" );
	defineCharset( "utf-16",				1200,		"Unicode (UTF-16)" );
	defineCharset( "windows-1250", 			1250,		"Central European (Windows)" );
	defineCharset( "windows-1251", 			1251,		"Cyrillic (Windows)" );
	defineCharset( "windows-1252", 			1252,		"Western" );
	defineCharset( "windows-1253",			1253,		"Greek (Windows)" );
	defineCharset( "windows-1254", 			1254,		"Turkish (Windows)" );
	defineCharset( "windows-1255", 			1255,		"Hebrew" );
	defineCharset( "windows-1256", 			1256,		"Arabic" );
	defineCharset( "windows-1257", 			1257,		"Baltic (Windows)" );
	defineCharset( "windows-1258", 			1258,		"Vietnamese" );
	defineCharset( "windows-874", 			874,		"Thai" );
	defineCharset( "x-cp1250", 				1250,		"Central European (Windows)" );
	defineCharset( "x-cp1251", 				1251,		"Cyrillic (Windows)" );
	defineCharset( "x-euc",					932,		"Japanese (EUC)" );
	defineCharset( "x-euc-jp", 				932,		"Japanese (EUC)" );
	defineCharset( "x-sjis", 				932,		"Shift-JIS" );
	defineCharset( "x-x-big5", 				950,		"Traditional Chinese (BIG5)" );

	isInitialized = true;
}

int CharsetTranscoder::getCodepageForCharset( const char *charset )
{
	if (!isInitialized)
	{
		initialize();
	}

	std::string charsetString = charset;
	std::transform(charsetString.begin(), charsetString.end(), charsetString.begin(), ::tolower);

	return CharsetTranscoder::charsetCodepageMap[charsetString];
}

bool CharsetTranscoder::isSupportedEncoding( const char *charset )
{
	if (!isInitialized)
	{
		initialize();
	}

	return ( 0 != CharsetTranscoder::getCodepageForCharset( charset ) );
}

bool CharsetTranscoder::transcode( std::string *text, const char *srcCharset, const char *dstCharset )
{
	if (!isInitialized)
	{
		initialize();
	}

	if ( (NULL != text) && ( text->size() < 1 ) )
	{
		return false;
	}

	bool bRet = false;

	int cpSrc = CharsetTranscoder::getCodepageForCharset( srcCharset );
	int cpDst = CharsetTranscoder::getCodepageForCharset( dstCharset );

	if ( ( 0 != cpSrc ) && ( 0 != cpDst ) )
	{
		// We got two valid codepages, let's see if we can convert from one to the other...
		//
		int wideBufferLen = MultiByteToWideChar(cpSrc, 0, text->c_str(), text->size(), NULL, 0);
		if ( 0 != wideBufferLen )
		{
			WCHAR * wchars = new WCHAR[wideBufferLen + 1];

			if ( 0 != MultiByteToWideChar(cpSrc, 0, text->c_str(), text->size(), wchars, wideBufferLen) )
			{
				// Ensure result is null terminated
				wchars[wideBufferLen] = 0;

				int mbBufferLen = WideCharToMultiByte( cpDst, 0, wchars, wideBufferLen, NULL, 0, NULL, NULL );
				if ( 0 != mbBufferLen )
				{
					std::string dstText( mbBufferLen, 0 );
					if ( 0 != WideCharToMultiByte( cpDst, 0, wchars, wideBufferLen, &dstText[0], mbBufferLen, NULL, NULL ) )
					{
						debug("Successfully transcoded from %s to %s", srcCharset, dstCharset);
						text->assign(dstText);
						bRet = true;
					}
				}
			}
			delete [] wchars;
		}
	}

	return bRet;
}
