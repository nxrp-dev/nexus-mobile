//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#import "CoronaLua.h"
#import "AppleNetworkSupport.h"
#import "AppleNetworkRequest.h"

#import <stdlib.h>
#import <QuartzCore/QuartzCore.h>

// #define NETWORK_DEBUG_VERBOSE 1

void debug( NSString *message, ... )
{
#ifdef NETWORK_DEBUG_VERBOSE
	va_list args;
	va_start(args, message);

	NSLogv( [NSString stringWithFormat:@"DEBUG: %@", message], args );

	va_end(args);

	return;
#endif
}

void error( NSString *message, ... )
{
	va_list args;
	va_start(args, message);
    
	NSLogv( [NSString stringWithFormat:@"ERROR: network: %@", message], args );
    
	va_end(args);
	return;
}

// ---------------------------------------------------------------------------

void paramValidationFailure( lua_State *luaState, NSString *message, ...)
{
	const char *where = "";

	// For now we're just going to log this.  We take a lua_State in case we decide at some point that
	// we want to do more (like maybe throw a Lua exception).
	//
	va_list args;
	va_start(args, message);

    if ( luaState != NULL )
    {
        // Include the location of the call from the Lua context (look 2 levels up for the caller)
        luaL_where( luaState, 2 );
        {
            where = lua_tostring( luaState, -1 );
        }
        lua_pop( luaState, 1 );
    }

	if (where == NULL)
	{
		where = "";
	}

	NSLogv( [NSString stringWithFormat:@"ERROR: network: %sinvalid parameter: %@", where, message], args );

	va_end(args);
	return;
}

// ---------------------------------------------------------------------------

Boolean isudatatype(lua_State *L, int idx, const char *name)
{
    // returns true if a userdata is of a certain type
    if ( LUA_TUSERDATA != lua_type( L, idx ) )
        return 0;
    
    lua_getmetatable( L, idx );
    luaL_newmetatable ( L, name );
    int res = lua_equal( L, -2, -1 );
    lua_pop( L, 2 ); // pop both tables (metatables) off
    return ( 0 != res );
}

// ---------------------------------------------------------------------------

// Parse the "charset" parameter, if any, from a Content-Type header
//
NSString* getContentTypeEncoding( NSString* contentTypeHeader )
{
	NSString* charset = nil;
	if ( nil != contentTypeHeader )
	{
		NSArray* values = [contentTypeHeader componentsSeparatedByString:@";"];
		
		for (NSString* value in values)
		{
			value = [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
			
			if ( [[value lowercaseString] hasPrefix:@"charset="] )
			{
				charset = [value substringFromIndex:[@"charset=" length]];
				debug(@"Explicit charset was found in content type, was: %@", charset);
			}
		}
	}
	return charset;
}

Boolean isContentTypeXML ( NSString* contentType )
{
	return (
			[contentType hasPrefix:@"text/xml"] ||
			[contentType hasPrefix:@"application/xml"] ||
			[contentType hasPrefix:@"application/xhtml"] ||
			([contentType hasPrefix:@"application/"] && [contentType hasSuffix:@"+xml"]) // application/rss+xml, many others
			);
}

Boolean isContentTypeHTML ( NSString* contentType )
{
	return (
			[contentType hasPrefix:@"text/html"] ||
			[contentType hasPrefix:@"application/xhtml"]
			);
}

Boolean isContentTypeText ( NSString* contentType )
{
	// Text types, use utf-8 to decode if no encoding specified
	//
	return (
			isContentTypeXML(contentType) ||
			isContentTypeHTML(contentType) ||
			[contentType hasPrefix:@"text/"] ||
			[contentType hasPrefix:@"application/json"] ||
			[contentType hasPrefix:@"application/javascript"] ||
			[contentType hasPrefix:@"application/x-javascript"] ||
			[contentType hasPrefix:@"application/ecmascript"] ||
			[contentType hasPrefix:@"application/x-www-form-urlencoded" ]
			);
}

// For structured text types (html or xml) look for embedded encoding.  Here is a good overview of the
// state of this problem:
//
//     http://en.wikipedia.org/wiki/Character_encodings_in_HTML
//
NSString* getEncodingFromContent ( NSString* contentType, NSString* content )
{
	// Note: This logic accomodates the fact that application/xhtml (and the -xml variant) is both XML
	//       and HTML, and the rule is to use the XML encoding if present, else HTML.
	//
	NSString* charset = nil;
	
	//debug(@"Looking for embedded encoding in content: %@", content);
	
	if (isContentTypeXML(contentType))
	{
		// Look in XML encoding meta header (ex: http://www.nasa.gov/rss/breaking_news.rss)
		//
		//   <?xml version="1.0" encoding="utf-8"?>
		//
		NSRegularExpression *xmlMetaPattern =
		[[[NSRegularExpression alloc] initWithPattern: @"<?xml\\b[^>]*\\bencoding=[\'\"]([a-zA-Z0-9_\\-]+)[\'\"].*[^>]*?>"
											 options: NSRegularExpressionCaseInsensitive
											   error: nil] autorelease];
		
		NSTextCheckingResult *match = [xmlMetaPattern firstMatchInString:content options:0 range:NSMakeRange(0, [content length])];
		if (match)
		{
			charset = [content substringWithRange:[match rangeAtIndex:1]];
			debug(@"Found charset in XML meta header encoding attribute: %@", charset);
		}
	}
	
	if ((nil == charset) && (isContentTypeHTML(contentType)))
	{
		// Look in HTML meta "charset" tag (ex: http://www.android.com)
		//
		//   <meta charset="utf-8">
		//
		NSRegularExpression *metaCharsetPattern =
		[[[NSRegularExpression alloc] initWithPattern: @"<meta\\b[^>]*\\bcharset=[\'\"]([a-zA-Z0-9_\\-]+)[\'\"][^>]*>"
											 options: NSRegularExpressionCaseInsensitive
											   error: nil] autorelease];
		
		NSTextCheckingResult *match = [metaCharsetPattern firstMatchInString:content options:0 range:NSMakeRange(0, [content length])];
		if (match)
		{
			charset = [content substringWithRange:[match rangeAtIndex:1]];
			debug(@"Found charset in HTML meta charset header: %@", charset);
		}
		
		if ( nil == charset )
		{
			// Look in HTML HTTP Content-Type meta header (ex: http://www.cnn.com)
			//
			//   <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
			//
			// First we need to find the full tag for this header.  Since the attributes may be in any order, we will have to do a
			// second pass to extract the charset out of the "content" attribute.
			//
			NSRegularExpression *metaHttpCtHeaderPattern =
			[[[NSRegularExpression alloc] initWithPattern: @"<meta\\b[^>]*\\bhttp-equiv=[\'\"](?:Content-Type)[\'\"][^>]*>"
												 options: NSRegularExpressionCaseInsensitive
												   error: nil] autorelease];
			
			NSTextCheckingResult *match = [metaHttpCtHeaderPattern firstMatchInString:content options:0 range:NSMakeRange(0, [content length])];
			if (match)
			{
				NSString* httpMetaCtHeader = [content substringWithRange:[match range]];
				
				debug(@"Found httpMetaCtHeader: %@", httpMetaCtHeader);
				
				NSRegularExpression *metaHttpCtCharsetPattern =
				[[[NSRegularExpression alloc] initWithPattern: @"\\bcharset=([a-zA-Z0-9_\\-]+)\\b"
													 options: NSRegularExpressionCaseInsensitive
													   error: nil] autorelease];
				
				NSTextCheckingResult *match = [metaHttpCtCharsetPattern firstMatchInString:httpMetaCtHeader options:0 range:NSMakeRange(0, [httpMetaCtHeader length])];
				if (match)
				{
					charset = [httpMetaCtHeader substringWithRange:[match rangeAtIndex:1]];
					debug(@"Found charset in meta Content-Type header: %@", charset);
				}
			}
		}
	}
	
	return charset;
}

// ---------------------------------------------------------------------------

@implementation ConnectionManager

@synthesize fConnectionList;

-(id)init
{
	fConnectionList = [[NSMutableArray alloc] init];
	return self;
}

-(void)onStartConnection: (CoronaURLConnection *) connection
{
	[fConnectionList addObject:connection];
}

-(void)onEndConnection: (CoronaURLConnection *) connection
{
	[fConnectionList removeObject:connection];
}

-(void)cancelAllConnections
{
	// Close and release any open connections
	//
	
	//This isn't pretty, we're iterating in reverse because connection cancel calls onEndConnection
	//Eventually we will want to refactor the manager
	
	for (long i = (long)[fConnectionList count] -1; i >=0; i--)
	{
		CoronaURLConnection *connection = [fConnectionList objectAtIndex:i];
		debug(@"cancelAllConnections - cancelling connection %@", connection);

		// Must invalidate any stale pointers
		// We have a separate invalidate call b/c cancel is called also by timeout.
		[connection invalidate];

		[connection cancel];
	}
	[fConnectionList removeAllObjects];
}

-(void)dealloc
{
	self.fConnectionList = nil;
	[super dealloc];
}

@end

// ---------------------------------------------------------------------------

static int lua_RequestCanceller_destructor( lua_State* luaState )
{
	debug(@"RequestCanceller destructor");

	NSRequestCanceller* requestCanceller = [NSRequestCanceller checkWithLuaState: luaState index: 1];
	[requestCanceller release];

	return 0;
}

static int lua_RequestCanceller_comparator( lua_State* luaState )
{
	// For our purposes, two RequestCanceller userdatas that point to the same RequestCanceller are
	// considered equal...
	//
	debug(@"RequestCanceller comparator");

	NSRequestCanceller* requestCanceller1 = [NSRequestCanceller checkWithLuaState: luaState index: 1];
	NSRequestCanceller* requestCanceller2 = [NSRequestCanceller checkWithLuaState: luaState index: 2];

	lua_pushboolean( luaState, (requestCanceller1 == requestCanceller2));

	return 1;
}

@implementation NSRequestCanceller

@synthesize fConnection;
@synthesize fIsCancelled;

+(const char*)metatableName
{
	return "luaL_RequestCanceller";
}

+(void)registerClassWithLuaState: (lua_State *) luaState
{
	luaL_Reg sRequestStateRegs[] =
	{
		{ "__eq", lua_RequestCanceller_comparator },
		{ "__gc", lua_RequestCanceller_destructor },
		{ NULL, NULL }
	};

	luaL_newmetatable(luaState, [NSRequestCanceller metatableName]);

	luaL_register(luaState, NULL, sRequestStateRegs);
	lua_pushvalue(luaState, -1);

	lua_setfield(luaState, -1, "__index");

	return;
}

+(NSRequestCanceller*)checkWithLuaState: (lua_State *) luaState index: (int) index;
{
	// Checks that the argument is a userdata with the correct metatable
	//
	return *(NSRequestCanceller **)luaL_checkudata(luaState, index, [NSRequestCanceller metatableName]);
}

-(id)initWithURLConnection: (CoronaURLConnection *) connection
{
	self = [super init];
	
	self.fConnection = connection;
	self.fIsCancelled = false;
	
	return self;
}

-(int)pushToLuaState: (lua_State *) luaState
{
	// Create/push a userdata, point it to ourself.
	//
	NSRequestCanceller** userData = (NSRequestCanceller **)lua_newuserdata(luaState, sizeof(NSRequestCanceller *));
	*userData = self;

	// Set the metatable for the userdata (to ensure that Lua will call the registered garbage collector
	// method as appropriate).
	//
	luaL_getmetatable(luaState, [NSRequestCanceller metatableName]);
	lua_setmetatable(luaState, -2);

	// Retain on behalf of the LuaState, which is going to own a reference via userdata.  Lua will GC the
	// userdata, which will call the registered destructor, which will in turn release this reference.
	//
	[self retain];

	return 1; // 1 value pushed on the stack
}

-(void)cancel
{
	debug(@"cancel called");
	if ( self.fConnection && !self.fIsCancelled )
	{
		debug(@"Cancelling request");
		self.fIsCancelled = true;
		[self.fConnection cancel];
	}
}

-(void)dealloc
{
	debug(@"NSRequestCanceller dealloc");
	[super dealloc];
}

@end

// ----------------------------------------------------------------------------

@implementation NSString (progress)

+ (NSString*)stringWithProgressDirection:(ProgressDirection)direction
{
	switch (direction)
	{
		case Upload:
			return @"Upload";
			break;
		case Download:
			return @"Download";
			break;
		case None:
			return @"None";
			break;
		default:
			return @"UNKNOWN";
			break;
	}
}

- (ProgressDirection)progressDirectionFromString
{
	if ( [@"upload" caseInsensitiveCompare:self] == NSOrderedSame )
	{
		return Upload;
	}
	else if ( [@"download" caseInsensitiveCompare:self] == NSOrderedSame )
	{
		return Download;
	}
	else if ( [@"none" caseInsensitiveCompare:self] == NSOrderedSame )
	{
		return None;
	}
	
	return UNKNOWN;
}
@end

// ----------------------------------------------------------------------------

@implementation CoronaFileSpec

@synthesize fFilename;
@synthesize fBaseDirectory;
@synthesize fFullPath;
@synthesize fIsResourceFile;

-(id)initWithFilename: (NSString*) filename baseDirectory:(void*)baseDirectory fullPath:(NSString*)fullPath isResourceFile:(Boolean)isResourceFile
{
	self = [super init];

	self.fFilename = filename;
	fBaseDirectory = baseDirectory;
	self.fFullPath = fullPath;
	fIsResourceFile = isResourceFile;
	
	return self;
}

-(void)dealloc
{
	// Release all "retain" properties (by assigning nil via property setter)
	self.fFilename = nil;
	self.fFullPath = nil;

	[super dealloc];
	return;
}

@end

// ----------------------------------------------------------------------------

@implementation NetworkRequestState

@synthesize fIsError;
@synthesize fName;
@synthesize fPhase;
@synthesize fStatus;
@synthesize fRequestURL;
@synthesize fResponseHeaders;
@synthesize fResponseType;
@synthesize fResponse;
@synthesize fRequestCanceller;
@synthesize fBytesTransferred;
@synthesize fBytesEstimated;
@synthesize fDebugValues;

-(id)initWithUrl: (NSString *) url isDebug:(Boolean)isDebug
{
	self = [super init];

	self.fIsError = false;
	self.fName = @"networkRequest";
	self.fPhase = @"began";
	self.fStatus = -1;
	self.fRequestURL = url;
	self.fResponseHeaders = nil;
	self.fResponseType = @"text";
	self.fResponse = nil;
	self.fBytesTransferred = 0;
	self.fBytesEstimated = 0;
	self.fDebugValues = nil;
	
	if ( isDebug )
	{
		self.fDebugValues = [[NSMutableDictionary alloc] init];
		[self.fDebugValues setValue: @"true" forKey: @"isDebug"];
	}
	
	return self;
}

-(void)setDebugValue:(NSString *)debugValue forKey:(NSString *)debugKey
{
	if ( fDebugValues )
	{
		[fDebugValues setValue: debugValue forKey: debugKey];
	}
}

-(int)pushToLuaState: (lua_State *) luaState
{
	int luaTableStackIndex = lua_gettop( luaState );
	
	lua_pushboolean( luaState, fIsError );
	lua_setfield( luaState, luaTableStackIndex, "isError" );
	
	lua_pushstring( luaState, [fName UTF8String] );
	lua_setfield( luaState, luaTableStackIndex, "name" );

	lua_pushstring( luaState, [fPhase UTF8String] );
	lua_setfield( luaState, luaTableStackIndex, "phase" );
	
	if ( fResponseHeaders )
	{
		lua_createtable( luaState, 0, (int)[fResponseHeaders count]);
		int luaHeaderTableStackIndex = lua_gettop( luaState );
	
		NSString *key;
		for ( key in fResponseHeaders )
		{
			NSString *value = [fResponseHeaders objectForKey:key];
			lua_pushstring( luaState, [value UTF8String] );
			lua_setfield( luaState, luaHeaderTableStackIndex, [key UTF8String] );
		}
		
		lua_setfield( luaState, luaTableStackIndex, "responseHeaders" );
	}

	if ( fResponse )
	{
		lua_pushstring( luaState, [fResponseType UTF8String] );
		lua_setfield( luaState, luaTableStackIndex, "responseType" );
	
		if ([fResponse isKindOfClass:[NSString class]])
		{
			lua_pushstring( luaState, [(NSString*)fResponse UTF8String] );
		}
		else if ([fResponse isKindOfClass:[NSData class]])
		{
			NSData* data = (NSData*)fResponse;
			NSUInteger size = [data length] / sizeof(unsigned char);
			const char* array = (const char*) [data bytes];
			lua_pushlstring( luaState, array, size );
		}
		else if ([fResponse isKindOfClass:[CoronaFileSpec class]])
		{
			CoronaFileSpec* fileSpec = (CoronaFileSpec*)fResponse;
			
			lua_createtable( luaState, 0, 3 );
			int luaResponseTableStackIndex = lua_gettop( luaState );
			
			lua_pushstring( luaState, [fileSpec.fFilename UTF8String] );
			lua_setfield( luaState, luaResponseTableStackIndex, "filename" );
			
			lua_pushlightuserdata( luaState, fileSpec.fBaseDirectory );
			lua_setfield( luaState, luaResponseTableStackIndex, "baseDirectory" );
			
			lua_pushstring( luaState, [fileSpec.fFullPath UTF8String] );
			lua_setfield( luaState, luaResponseTableStackIndex, "fullPath" );
		}
		lua_setfield( luaState, luaTableStackIndex, "response" );
	}
	
	lua_pushinteger( luaState, fStatus );
	lua_setfield( luaState, luaTableStackIndex, "status" );
	
	lua_pushstring( luaState, [fRequestURL UTF8String] );
	lua_setfield( luaState, luaTableStackIndex, "url" );

	if ( fRequestCanceller )
	{
		[fRequestCanceller pushToLuaState:luaState];
		lua_setfield( luaState, luaTableStackIndex, "requestId" );
	}

	lua_pushnumber( luaState, fBytesTransferred );
	lua_setfield( luaState, luaTableStackIndex, "bytesTransferred" );
	
	lua_pushnumber( luaState, fBytesEstimated );
	lua_setfield( luaState, luaTableStackIndex, "bytesEstimated" );

	if ( fDebugValues )
	{
		lua_createtable( luaState, 0, (int)[fDebugValues count]);
		int luaDebugTableStackIndex = lua_gettop( luaState );
		
		NSString *key;
		for ( key in fDebugValues )
		{
			NSString *value = [fDebugValues objectForKey:key];
			lua_pushstring( luaState, [value UTF8String] );
			lua_setfield( luaState, luaDebugTableStackIndex, [key UTF8String] );
		}
		
		lua_setfield( luaState, luaTableStackIndex, "debug" );
	}

	return 1; // We left one item on the stack (the networkRequest table)
}

-(void)dealloc
{
    debug(@"Dealloc NetworkRequestState");
    
	// Release all "retain" properties (by assigning nil via property setter)
	self.fResponseHeaders = nil;
	self.fResponse = nil;
	self.fRequestCanceller = nil;
	self.fDebugValues = nil;

	[super dealloc];
	return;
}

@end

// ----------------------------------------------------------------------------

@implementation LuaCallback

@synthesize fLastNotificationPhase;
@synthesize fMinNotificationIntervalMs;
@synthesize fLastNotificationTime;

-(id)initWithLuaState: (lua_State*) luaState reference: (CoronaLuaRef) luaReference
{
	self = [super init];
	
	//Get the main thread state, in case we are on a 
	lua_State *coronaState = luaState;
	
	lua_State *mainState = CoronaLuaGetCoronaThread(luaState);
	if (NULL != mainState )
	{
		coronaState = mainState;
	}
	
	self.fLuaState = coronaState;
	self.fLuaReference = luaReference;
	
	fMinNotificationIntervalMs = 1000;
	fLastNotificationTime = 0;

	return self;
}

-(Boolean)callWithNetworkRequestState: (NetworkRequestState*) networkRequestState
{
	// We call the callback conditionally based on the following:
	//
	
	// Rule 1: We don't send notifications if the request has been cancelled.
	//
	//   Note: In practice, the request cancel is immediate and we never see this case,
	//         but we'll leave this in just in case it is possible with specific timing...
	//
	if ( networkRequestState.fRequestCanceller.fIsCancelled )
	{
		debug(@"Attempt to post call to callback after cancelling, ignoring");
		return false; // We did not post the callback
	}
	
	// Rule 2: We don't send multiple notifications of the same type (phase) within a certain
	//         interval, in order to avoid overrunning the listener.
	//
	double currentTime = CACurrentMediaTime();
	if ( [networkRequestState.fPhase isEqualToString:fLastNotificationPhase] && ( ( fLastNotificationTime + fMinNotificationIntervalMs/1000 ) > currentTime ) )
	{
		debug(@"Attempt to post call to callback for phase \"%@\" within notification interval, ignoring", networkRequestState.fPhase);
		return false; // We did not post the callback
	}
	else
	{
		fLastNotificationPhase = networkRequestState.fPhase;
		fLastNotificationTime = currentTime;
	}

	lua_State *L = self.fLuaState;
	if ( L )
	{
		CoronaLuaNewEvent( L, "networkRequest" );
		[networkRequestState pushToLuaState: L];
		CoronaLuaDispatchEvent( L, self.fLuaReference, 0 );
	}

	return true;
}

-(void)invalidate
{
	// LuaState is no longer available, so set it to NULL.
	// NOTE: all other methods should check for NULL before using this member
	self.fLuaState = NULL;
}

-(void)dealloc
{
	debug(@"unregistering LuaCallback");

	lua_State *L = self.fLuaState;
	if ( L )
	{
		CoronaLuaDeleteRef( L, self.fLuaReference );
	}
	self.fLuaReference = NULL;

	[super dealloc];

	debug(@"done unregistering LuaCallback");

	return;
}

@end

// ----------------------------------------------------------------------------

@implementation NetworkRequestParameters

@synthesize fRequestURL;
@synthesize	fMethod;
@synthesize fRequestHeaders;
@synthesize fIsBodyTypeText;
@synthesize fProgressDirection;
@synthesize fTimeout;
@synthesize fIsDebug;
@synthesize fRequestBody;
@synthesize fRequestBodySize;
@synthesize fResponse;
@synthesize fLuaCallback;
@synthesize fIsValid;
@synthesize fHandleRedirects;

-(id)initWithLuaState: (lua_State*) luaState;
{
	self = [super init];

	fLuaCallback = nil;
	fIsValid = false;

	int arg = 1;

	Boolean isInvalid = false;

	// First argument - url (required)
	//
	if ( LUA_TSTRING == lua_type( luaState, arg ) )
	{
		const char *url = lua_tostring( luaState, arg );
		fRequestURL = [NSString stringWithUTF8String:url];
		
		NSURL *nsUrl = [NSURL URLWithString:fRequestURL];
		if ( nil == nsUrl )
		{
			paramValidationFailure( luaState, [NSString stringWithFormat:@"URL argument was malformed URL: %@", fRequestURL] );
			isInvalid = true;
		}

	}
	else
	{
		paramValidationFailure( luaState, @"First argument to network.request() should be a URL string (got %s)", lua_typename(luaState, lua_type(luaState, arg)) );
		isInvalid = true;
	}

	++arg;

	// Second argument - method (optional)
	//
	if (!isInvalid)
	{
		if ( LUA_TSTRING == lua_type( luaState, arg ) )
		{
			const char *method = lua_tostring( luaState, arg );

			// This is validated in the Lua class
			fMethod = [NSString stringWithUTF8String:method];
			
			++arg;
		}
		else
		{
			fMethod = @"GET";
		}
	}

	// Third argument - listener (optional)
	//
	if (!isInvalid)
	{
		if ( CoronaLuaIsListener( luaState, arg, "networkRequest" ) )
		{
            CoronaLuaRef ref = CoronaLuaNewRef( luaState, arg );
			fLuaCallback = [[LuaCallback alloc] initWithLuaState: luaState reference: ref];

			++arg;
		}
	}

	// Fourth argument - params table (optional)
	//
	int paramsTableStackIndex = arg;
	if (!isInvalid && !lua_isnoneornil( luaState, arg ))
	{
		if ( LUA_TTABLE == lua_type( luaState, arg ) )
		{
            Boolean wasRequestContentTypePresent = false;

			lua_getfield( luaState, arg, "headers" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TTABLE == lua_type( luaState, -1 ) )
				{
					for (lua_pushnil(luaState); lua_next(luaState,-2); lua_pop(luaState,1))
					{
						// Fetch the table entry's string key.
						// An index of -2 accesses the key that was pushed into the Lua stack by luaState.next() up above.
						const char *keyName = lua_tostring(luaState, -2);
						if (keyName == nil)
						{
							// A valid key was not found. Skip this table entry.
							continue;
						}

						NSString *key = [NSString stringWithUTF8String:keyName];
						NSString *value = nil;

                        if ( [@"Content-Length" caseInsensitiveCompare:key] == NSOrderedSame )
                        {
							// You just don't worry your pretty little head about the Content-Length, we'll handle that...
							continue;                            
                        }

						// Fetch the table entry's value in string form.
						// An index of -1 accesses the entry's value that was pushed into the Lua stack by luaState.next() above.
						switch (lua_type(luaState, -1))
						{
							case LUA_TSTRING:
							{
								const char *stringValue = lua_tostring(luaState, -1);
								value = [NSString stringWithUTF8String:stringValue];
							}
							break;
								
							case LUA_TNUMBER:
							{
								double numericValue = lua_tonumber(luaState, -1);
								if (floor(numericValue) == numericValue)
								{
									value = [NSString stringWithFormat:@"%i", int(numericValue)];
								}
								else
								{
									value = [NSString stringWithFormat:@"%f", numericValue];
								}
							}
							break;
								
							case LUA_TBOOLEAN:
							{
								Boolean booleanValue = lua_toboolean(luaState, -1);
								value = booleanValue ? @"true" : @"false";
							}
							break;
						}
						
						if (value != nil)
						{
							debug(@"Header - %@: %@", key, value);
							
							if (fRequestHeaders == nil)
							{
								fRequestHeaders = [[NSMutableDictionary alloc] init];
							}
							
							if ( [@"Content-Type" caseInsensitiveCompare:key] == NSOrderedSame )
							{
                                wasRequestContentTypePresent = true;

								NSString* ctCharset = getContentTypeEncoding( value );
								if ( nil != ctCharset )
								{
									NSStringEncoding encodingFromCharset = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding((CFStringRef)ctCharset));
									if ( (int)encodingFromCharset <= 0 )
									{
										paramValidationFailure( luaState, @"'header' value for Content-Type header contained an unsupported character encoding: %@", ctCharset );
										isInvalid = true;
									}
								}
							}

							[fRequestHeaders setValue: value forKey: key];
						}
					}
				}
				else
				{
					paramValidationFailure( luaState, @"'headers' value of params table, if provided, should be a table (got %s)", lua_typename(luaState, lua_type(luaState, -1)) );
					isInvalid = true;
				}
			}
			lua_pop( luaState, 1);
			
			fIsBodyTypeText = true;
			lua_getfield( luaState, paramsTableStackIndex, "bodyType" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TSTRING == lua_type( luaState, -1 ) )
				{
					// If we got something, make sure it's a string
					const char *bodyTypeValue = lua_tostring( luaState, -1 );
					
					NSString *bodyType = [NSString stringWithUTF8String:bodyTypeValue];
					if ( [@"text" caseInsensitiveCompare:bodyType] == NSOrderedSame )
					{
						fIsBodyTypeText = true;
					}
					else if ( [@"binary" caseInsensitiveCompare:bodyType] == NSOrderedSame )
					{
						fIsBodyTypeText = false;
					}
					else
					{
						paramValidationFailure( luaState, @"'bodyType' value of params table was invalid, must be either \"text\" or \"binary\", but was: \"%s\"", bodyTypeValue );
						isInvalid = true;
					}
				}
				else
				{
					paramValidationFailure( luaState, @"'bodyType' value of params table, if provided, should be a string value (got %s)", lua_typename(luaState, lua_type(luaState, -1)) );
					isInvalid = true;
				}
			}
			lua_pop( luaState, 1 );
			
			lua_getfield( luaState, paramsTableStackIndex, "body" );
			if (!lua_isnil( luaState, -1 ))
			{
				// This can be either a Lua string containing the body, or a table with filename/baseDirectory that points to a body file.
				// If it's a string, it can either be "text" (Java String) or "binary" (byte[]), based on bodyType (above).
				//
				switch (lua_type(luaState, -1))
				{
					case LUA_TSTRING:
					{
						if (fIsBodyTypeText)
						{
							debug(@"Request body from String (text)");
							const char* requestValue = lua_tostring( luaState, -1);
							self.fRequestBody = [NSString stringWithUTF8String:requestValue];
							self.fRequestBodySize = ((NSString*)self.fRequestBody).length;
							
							if (!wasRequestContentTypePresent)
							{
								NSString *contentType = [[NSString alloc] initWithString:@"text/plain; charset=UTF-8"];
								[fRequestHeaders setValue: contentType forKey: @"Content-Type"];
								wasRequestContentTypePresent = true;
							}
						}
						else
						{
							debug(@"Request body from String (binary)");
							size_t dataSize;
							const char* requestValue = lua_tolstring( luaState, -1, &dataSize );
							self.fRequestBody = [NSData dataWithBytes: requestValue length: dataSize];
							self.fRequestBodySize = ((NSString*)self.fRequestBody).length;
							
							if (!wasRequestContentTypePresent)
							{
								NSString *contentType = [[NSString alloc] initWithString:@"application/octet-stream"];
								[fRequestHeaders setValue: contentType forKey: @"Content-Type"];
								wasRequestContentTypePresent = true;
							}
						}
					}
					break;
						
					case LUA_TTABLE:
					{
						// Body type for body from file is always binary
						//
						fIsBodyTypeText = false;
						
						// Extract filename/baseDirectory
						//
						lua_getfield( luaState, -1, "filename" ); // required
						
						if ( LUA_TSTRING == lua_type( luaState, -1 ) )
						{
							const char *filename = lua_tostring( luaState, -1 );
							lua_pop( luaState, 1 );
							
							void *baseDirectory = nil;
							lua_getfield( luaState, -1, "baseDirectory"); // optional
							if (!lua_isnoneornil( luaState, 1 ))
							{
								baseDirectory = lua_touserdata( luaState, -1 );
							}
							lua_pop( luaState, 1 );
							
							// Prepare and call Lua function
							int	numParams = 1;
							lua_getglobal( luaState, "_network_pathForFile" );
							lua_pushstring( luaState, filename );  // Push argument #1
							if ( baseDirectory )
							{
								lua_pushlightuserdata( luaState, baseDirectory ); // Push argument #2
								numParams++;
							}
							
							Corona::Lua::DoCall( luaState, numParams, 2); // 1/2 arguments, 2 returns
							
							Boolean isResourceFile = lua_toboolean( luaState, -1 );
							const char *path = lua_tostring( luaState, -2 );
							lua_pop( luaState, 2 ); // Pop results
							
							NSString* strFilename = [NSString stringWithUTF8String:filename];
							NSString* strPath = nil;
							if(path) {
								strPath = [NSString stringWithUTF8String:path];
							} else if([strFilename hasPrefix:@"file:///"]) {
								NSURL *fileUrl = [NSURL URLWithString:strFilename];
								strPath = [fileUrl path];
								path = [strPath UTF8String];
								strFilename = [strPath lastPathComponent];
							}
							
							debug(@"body pathForFile from LUA: %s, isResourceFile: %s", path, isResourceFile ? "true" : "false");
							
							CoronaFileSpec* fileSpec = [[CoronaFileSpec alloc] initWithFilename: strFilename baseDirectory: baseDirectory fullPath: strPath isResourceFile: isResourceFile];
							fRequestBody = fileSpec;
							
							NSError *attributesError = nil;
							NSDictionary *fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:fileSpec.fFullPath error:&attributesError];
							
							NSNumber *fileSizeNumber = [fileAttributes objectForKey:NSFileSize];
							self.fRequestBodySize = [fileSizeNumber longLongValue];
							
							debug(@"Size of body file is: %li", self.fRequestBodySize);
						}
						else
						{
							paramValidationFailure( luaState, @"body 'filename' value is required and must be a string value" );
							isInvalid = true; 
						}
					}
					break;
						
					default:
					{
						paramValidationFailure( luaState, @"Either body string or table specifying body file is required if 'body' is specified" );
						isInvalid = true;
					}
					break;
				}


				if ( ( NULL != self.fRequestBody ) && !wasRequestContentTypePresent )
                {
                    paramValidationFailure( luaState, @"Request Content-Type header is required when request 'body' is specified" );
                    isInvalid = true;
                }
				
			}
			lua_pop( luaState, 1 );
			
			fProgressDirection = None;
			lua_getfield( luaState, paramsTableStackIndex, "progress" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TSTRING == lua_type( luaState, -1 ) )
				{
					const char *progress = lua_tostring( luaState, -1 );
					
					fProgressDirection = [[NSString stringWithUTF8String:progress] progressDirectionFromString];
					if ( UNKNOWN == fProgressDirection )
					{
						paramValidationFailure( luaState, @"'progress' value of params table was invalid, if provided, must be either \"upload\" or \"download\", but was: \"%s\"", progress );
						isInvalid = true;
					}
					
					debug(@"Progress: %@", [NSString stringWithProgressDirection:fProgressDirection]);
				}
				else
				{
					paramValidationFailure( luaState, @"'progress' value of params table, if provided, should be a string value (got %s)", lua_typename(luaState, lua_type(luaState, -1)) );
					isInvalid = true;
				}
			}
			lua_pop( luaState, 1 );
			
			fResponse = nil;
			lua_getfield( luaState, paramsTableStackIndex, "response" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TTABLE == lua_type( luaState, -1 ) )
				{
					// Extract filename/baseDirectory
					//
					lua_getfield( luaState, -1, "filename" ); // required
					
					if ( LUA_TSTRING == lua_type( luaState, -1 ) )
					{
						const char *filename = lua_tostring( luaState, -1 );
						lua_pop( luaState, 1 );
						
						void *baseDirectory = nil;
						lua_getfield( luaState, -1, "baseDirectory"); // optional
						if (!lua_isnoneornil( luaState, 1 ))
						{
							baseDirectory = lua_touserdata( luaState, -1 );
						}
						else
						{
							lua_getglobal( luaState, "system");
							lua_getfield(luaState, -1, "DocumentsDirectory");
							baseDirectory = lua_touserdata( luaState, -1 );
							lua_pop(luaState, 2);
							
						}
						
						lua_pop( luaState, 1 );
						
						// Prepare and call Lua function
						int	numParams = 1;
						lua_getglobal( luaState, "_network_pathForFile" );
						lua_pushstring( luaState, filename );  // Push argument #1
						if ( baseDirectory )
						{
							lua_pushlightuserdata( luaState, baseDirectory ); // Push argument #2
							numParams++;
						}
						
						Corona::Lua::DoCall( luaState, numParams, 2 ); // 1/2 arguments, 2 returns
						
						Boolean isResourceFile = lua_toboolean( luaState, -1 );
						const char *path = lua_tostring( luaState, -2 );
						lua_pop( luaState, 2 ); // Pop results
						
						NSString* strFilename = [NSString stringWithUTF8String:filename];
						NSString* strPath = [NSString stringWithUTF8String:path];
						
						debug(@"response pathForFile from LUA: %s, isResourceFile: %s", path, isResourceFile ? "true" : "false");
						fResponse = [[CoronaFileSpec alloc] initWithFilename: strFilename baseDirectory: baseDirectory fullPath: strPath isResourceFile: isResourceFile ];
					}
					else
					{
						paramValidationFailure( luaState, @"response 'filename' value is required and must be a string value" );
						isInvalid = true;                        
					}
				}
				else
				{
					paramValidationFailure( luaState, @"'response' value of params table, if provided, should be a table specifying response location values" );
					isInvalid = true;
				}
			}
			lua_pop( luaState, 1 );
			
			lua_getfield( luaState, paramsTableStackIndex, "timeout" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TNUMBER == lua_type( luaState, -1 ) )
				{
					fTimeout = lua_tonumber( luaState, -1 );
					debug(@"Request timeout provided, was: %i", fTimeout);
				}
				else
				{
					paramValidationFailure( luaState, @"'timeout' value of params table, if provided, should be a numeric value (got %s)", lua_typename(luaState, lua_type(luaState, -1)) );
					isInvalid = true;
				}
			}
			lua_pop( luaState, 1 );
			
			fIsDebug = false;
			lua_getfield( luaState, paramsTableStackIndex, "debug" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TBOOLEAN == lua_type( luaState, -1 ) )
				{
					fIsDebug = lua_toboolean( luaState, -1 );
				}
			}
			lua_pop( luaState, 1 );
			
			fHandleRedirects = true;
			lua_getfield( luaState, paramsTableStackIndex, "handleRedirects" );
			if (!lua_isnil( luaState, -1 ))
			{
				if ( LUA_TBOOLEAN == lua_type( luaState, -1 ) )
				{
					fHandleRedirects = lua_toboolean( luaState, -1 );
				}
				else
				{
					paramValidationFailure( luaState, @"'handleRedirects' value of params table, if provided, should be a boolean value (got %s)", lua_typename(luaState, lua_type(luaState, -1)) );
					isInvalid = true;
				}
			}
			lua_pop( luaState, 1 );
		}
		else
		{
			paramValidationFailure( luaState, @"Fourth argument to network.request(), if provided, should be a params table (got %s)", lua_typename(luaState, lua_type(luaState, -1)) );
			isInvalid = true;
		}
	}

	fIsValid = !isInvalid;
	return self;
}

-(void)dealloc
{
	debug(@"dealloc HttpRequestParams");
	
	// Release all "retain" properties (by assigning nil via property setter)
	debug(@"Releasing request body: %@", self.fRequestBody);
	self.fRequestBody = nil;
	debug(@"Releasing request headers: %@", self.fRequestHeaders);
	self.fRequestHeaders = nil;
	debug(@"Releasing response: %@", self.fResponse);
	self.fResponse = nil;
	debug(@"Releasing Lua callback: %@", self.fLuaCallback);
	self.fLuaCallback = nil;
	
	[super dealloc];

	debug(@"done dealloc HttpRequestParams");

	return;
}

-(void)invalidate
{
	// Invalidate the Lua callback and any stale data it uses, e.g. LuaState ptr
	[self.fLuaCallback invalidate];
}

@end
