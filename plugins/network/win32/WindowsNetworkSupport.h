//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "CoronaLua.h"

#ifndef _WindowsNetworkSupport_H_
#define _WindowsNetworkSupport_H_

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <map>
#include <vector>
#include <string>
#include <memory>

typedef std::map<std::string, std::string>	StringMap;
typedef std::vector<unsigned char>			ByteVector;
typedef std::string							UTF8String;

void debug( char *message, ... );

// ----------------------------------------------------------------------------

void paramValidationFailure( lua_State *luaState, char *message, ... );

// ----------------------------------------------------------------------------

bool isudatatype(lua_State *L, int idx, const char *name);

// ----------------------------------------------------------------------------

UTF8String pathForTemporaryFileWithPrefix(const char *prefix, UTF8String pathDir);

// ----------------------------------------------------------------------------

char * getContentType( const char *contentTypeHeader );
char * getContentTypeEncoding( const char *contentTypeHeader );

bool isContentTypeXML ( const char *contentType );

bool isContentTypeHTML ( const char *contentType );

bool isContentTypeText ( const char *contentType );

char * getEncodingFromContent ( const char *contentType, const char *content );

// ----------------------------------------------------------------------------

UTF8String utf8_encode( const WCHAR * wideString );
UTF8String utf8_encode( const WCHAR * wideString, int wideStringLen );
const WCHAR * getWCHARs ( UTF8String string );

// ----------------------------------------------------------------------------

class WinHttpRequestOperation;

class RequestCanceller
{
public:
	static const char * getMetatableName( );

	static void registerClassWithLuaState( lua_State * L );

	static RequestCanceller * checkWithLuaState( lua_State * L, int index );

	RequestCanceller(const std::shared_ptr<WinHttpRequestOperation>& requestOperation );
	~RequestCanceller( );

	void AddRef();
	void Release();

	int pushToLuaState( lua_State * L );

	bool isCancelled( );
	void cancel( );

private:

	std::shared_ptr<WinHttpRequestOperation> fRequestOperation;
	int fRefCount;
	bool fIsCancelled;

};

// ----------------------------------------------------------------------------

typedef enum {
	UNKNOWN		= 0,
	Upload		= 1,
	Download	= 2,
	None		= 3,
} ProgressDirection;

ProgressDirection getProgressDirectionFromString( const char *progressString );
const char* getProgressDirectionName( ProgressDirection progressDirection );

// ----------------------------------------------------------------------------

class CoronaFileSpec
{
public:

	CoronaFileSpec( const char *filename, void *baseDirectory, const char *fullPath, bool isResourceFile )
	{
		fFilename = filename;
		fBaseDirectory = baseDirectory;
		fFullPath = fullPath;
		fIsResourceFile = isResourceFile;
	}

	CoronaFileSpec( CoronaFileSpec *fileSpec )
	{
		fFilename		= fileSpec->fFilename;
		fBaseDirectory	= fileSpec->fBaseDirectory;
		fFullPath		= fileSpec->fFullPath;
		fIsResourceFile = fileSpec->fIsResourceFile;
	}

	~CoronaFileSpec( )
	{
	}

	UTF8String getFilename( )
	{
		return fFilename;
	}

	void * getBaseDirectory( )
	{
		return fBaseDirectory;
	}

	UTF8String getFullPath( )
	{
		return fFullPath;
	}

	bool isResourceFile( )
	{
		return fIsResourceFile;
	}

private:

	UTF8String	fFilename;
	void*		fBaseDirectory;
	UTF8String	fFullPath;
	bool		fIsResourceFile;

};

// ----------------------------------------------------------------------------

typedef enum 
{
	TYPE_NONE,
	TYPE_STRING,
	TYPE_BYTES,
	TYPE_FILE,
} BodyType;

typedef struct
{
	BodyType bodyType;
	union
	{
		UTF8String* bodyString;
		ByteVector* bodyBytes;
		CoronaFileSpec* bodyFile;
	};
} Body;

// ----------------------------------------------------------------------------

class NetworkRequestState
{
public:

	NetworkRequestState(const std::shared_ptr<WinHttpRequestOperation>& requestOperation, UTF8String url, bool isDebug );
	~NetworkRequestState();
	
	void setError( UTF8String *message = NULL );
	void setPhase( const char *phase );
	void setStatus( int status );
	void setResponseHeaders( const char *headers );
	void setResponseType( const char *responseType );
	void setBytesEstimated( long long nBytesTransferred );
	void setBytesTransferred( long long nBytesTransferred );
	void incrementBytesTransferred( int newBytesTransferred );
	void setDebugValue( char *debugValue, char *debugKey );

	bool isError( );
	StringMap getResponseHeaders( );
	UTF8String getResponseHeaderValue( const char *headerKey );
	Body* getResponseBody( );
	const char* getPhase( );
	RequestCanceller* getRequestCanceller( );

	int pushToLuaState( lua_State *L );

private:

	bool			fIsError;
	UTF8String		fPhase;
	int				fStatus;
	UTF8String		fRequestURL;
	StringMap		fResponseHeaders;
	UTF8String		fResponseType;
	Body			fResponseBody;
	RequestCanceller* fRequestCanceller;
	long long		fBytesEstimated;
	long long		fBytesTransferred;
	StringMap		fDebugValues;

};

// ----------------------------------------------------------------------------

class LuaCallback
{
public:

	LuaCallback( lua_State* luaState, CoronaLuaRef luaReference );
	~LuaCallback();

	bool callWithNetworkRequestState( NetworkRequestState *requestState );
	void unregister();

private:

	lua_State* fLuaState;
	CoronaLuaRef fLuaReference;

	std::string fLastNotificationPhase;
	DWORD fMinNotificationIntervalMs;
	DWORD fLastNotificationTime;

};

// ----------------------------------------------------------------------------

class NetworkRequestParameters
{
public:

	NetworkRequestParameters( lua_State *L );
	~NetworkRequestParameters();

	bool isValid( );

	UTF8String getRequestUrl( );
	UTF8String getRequestMethod( );
	ProgressDirection getProgressDirection( );
	UTF8String getRequestHeaderString( );
	StringMap* getRequestHeaders( );
	UTF8String* getRequestHeaderValue( const char *headerKey );
	Body* getRequestBody( );
	long long getRequestBodySize( );
	CoronaFileSpec* getResponseFile( );
	LuaCallback* getLuaCallback( );
	int getTimeout( );
	bool isDebug( );
	bool getHandleRedirects( );

private:

	UTF8String		fRequestUrl;
	UTF8String		fMethod;
	ProgressDirection fProgressDirection;
	StringMap		fRequestHeaders;
	bool			fIsBodyTypeText;
	int				fTimeout;
	bool			fIsDebug;
	Body			fRequestBody;
	long long		fRequestBodySize;
	CoronaFileSpec*	fResponseFile;

	LuaCallback*	fLuaCallback;
	bool			fIsValid;
	bool			fHandleRedirects;
};

#endif
