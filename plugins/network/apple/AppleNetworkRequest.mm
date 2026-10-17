//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include <TargetConditionals.h>

#import "AppleNetworkRequest.h"

#import "AppleNetworkSupport.h"

// ----------------------------------------------------------------------------
// System Versioning Preprocessor Macros
//

#if TARGET_OS_IPHONE
	#import <UIKit/UIDevice.h>

	#define SYSTEM_VERSION_EQUAL_TO(v)                  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] == NSOrderedSame)
	#define SYSTEM_VERSION_GREATER_THAN(v)              ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] == NSOrderedDescending)
	#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)
	#define SYSTEM_VERSION_LESS_THAN(v)                 ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] == NSOrderedAscending)
	#define SYSTEM_VERSION_LESS_THAN_OR_EQUAL_TO(v)     ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedDescending)
#endif

// ----------------------------------------------------------------------------

// Workaround for Apple Radar: 10412199
// See also:
// http://stackoverflow.com/questions/2736967/nsmutableurlrequest-not-obeying-my-timeoutinterval
// https://devforums.apple.com/message/108292#108292
//
// The timeout property in CoronaURLRequest and the timeout-related methods in CoronaURLConnection
// are used to implement this workaround.  Note that the bug is fixed in iOS 6.0.
//

@interface CoronaURLRequest ()

@property (assign, readonly, getter=timeout) NSTimeInterval fTimeout;

@end

@implementation CoronaURLRequest

@synthesize fTimeout;

-(id)initWithNetworkRequestParameters:(NetworkRequestParameters*) requestParams error:(NSError**) error 
{
	NSURL *nsUrl = [NSURL URLWithString:requestParams.fRequestURL];

	self = [super initWithURL:nsUrl];
	if ( self )
	{
		[self setHTTPMethod:requestParams.fMethod];
		
		int nTimeout = 30; // Default to 30 seconds
		if ( requestParams.fTimeout > 0 )
		{
			nTimeout = requestParams.fTimeout;
		}
		fTimeout = nTimeout;
		[self setTimeoutInterval:nTimeout];
		
		NSStringEncoding requestBodyCharset = NSUTF8StringEncoding;
		Boolean wasCtHeaderSpecified = false;

		if ( requestParams.fRequestHeaders )
		{
			NSString *header;
			for ( header in requestParams.fRequestHeaders )
			{
				NSString *value = [requestParams.fRequestHeaders objectForKey:header];
				if ( [@"Content-Type" caseInsensitiveCompare:header] == NSOrderedSame )
				{
					debug(@"Found request Content-Type: %@", value);
					
					wasCtHeaderSpecified = true;

					if ( requestParams.fIsBodyTypeText )
					{
						// If the request body is text, we need to record the specified character encoding (if any) from
						// the supplied Content-Type header so we can apply it to the request body later.  If there is no
						// character encoding specified on the Content-Type header, then we need to set it's charset attribute
						// explicitly here (to the default).
						//
						NSString* ctCharset = getContentTypeEncoding( value );
						if ( nil != ctCharset )
						{
							NSStringEncoding encodingFromCharset = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding((CFStringRef)ctCharset));
							debug(@"Got encoding from charset: %@, encoding: %d", ctCharset, encodingFromCharset);
							if ( (int)encodingFromCharset > 0 )
							{
								debug(@"Encoding from charset was valid");
								requestBodyCharset = encodingFromCharset;
							}
							else
							{
								// This is bad, user supplied a charset in the Content-Type header that we don't support - error out.
								//
								NSString* errorMessage = [NSString stringWithFormat:@"Caller specified an unsupported character encoding in Content-Type charset, was: %@", ctCharset];
								debug(errorMessage);
								if ( nil != error )
								{
									*error = [NSError errorWithDomain:NSURLErrorDomain
																 code:NSURLErrorUnknown
															 userInfo:[NSDictionary dictionaryWithObject:errorMessage forKey:NSLocalizedDescriptionKey]];
								}
								[self release];
								return nil;
							}
						}
						else
						{
							// No charset was provided on the Content-Type header, and content is text, so we set the charset to the default (UTF-8).
							//
							value = [value stringByAppendingString:@"; charset=UTF-8"];
							debug(@"Adding default charset to Content-Type header: %@", value);
						}
					}
				}
				[self addValue:value forHTTPHeaderField:header];
			}
		}
		
		// We're only going to set the request body on PUT/POST (you get a "request body stream exhausted" hissyfit otherwise).
		//
		if (( [@"PUT" caseInsensitiveCompare:requestParams.fMethod] == NSOrderedSame ) ||
			( [@"POST" caseInsensitiveCompare:requestParams.fMethod] == NSOrderedSame ) ||
			( [@"PATCH" caseInsensitiveCompare:requestParams.fMethod] == NSOrderedSame ))
		{
			if ( requestParams.fRequestBody )
			{
				if ( ! wasCtHeaderSpecified )
				{
					// We check for the presence of a Content-Type request header on param validation whenever a request body
					// is specified, so we should never encounter this case.
					//
					debug(@"No Content-Type request header was provided for the POST/PUT");
				}

				if ([requestParams.fRequestBody isKindOfClass:[CoronaFileSpec class]])
				{
					// Stream from file
					//
					debug(@"Setting body stream to file: %@", ((CoronaFileSpec*)requestParams.fRequestBody).fFullPath);
					
					NSInputStream* bodyReader = [NSInputStream inputStreamWithFileAtPath:((CoronaFileSpec*)requestParams.fRequestBody).fFullPath];
					[self setHTTPBodyStream:bodyReader];
				}
				else
				{
					// Load from Lua string (text or binary)
					//
					if ([requestParams.fRequestBody isKindOfClass:[NSString class]])
					{
						debug(@"Setting body from text string: %@", (NSString*)requestParams.fRequestBody);
						[self setHTTPBody:[(NSString*)requestParams.fRequestBody dataUsingEncoding:requestBodyCharset]];
					}
					else if ([requestParams.fRequestBody isKindOfClass:[NSData class]])
					{
						debug(@"Setting body from binary string with length: %i", ((NSData*)requestParams.fRequestBody).length);
						[self setHTTPBody:(NSData*)requestParams.fRequestBody];
					}
				}
				
				[self setValue:[NSString stringWithFormat:@"%llu", requestParams.fRequestBodySize] forHTTPHeaderField:@"Content-Length"];
			}
		}

	}
	return self;
}

@end

// ----------------------------------------------------------------------------

@interface CoronaConnectionDelegate : NSObject

- (void)connection:(CoronaURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
- (void)connection:(CoronaURLConnection *)connection didReceiveData:(NSData *)data;
- (void)connectionDidFinishLoading:(CoronaURLConnection *)connection;
- (void)connection:(CoronaURLConnection *)connection didFailWithError:(NSError*)error;
- (void)connection:(CoronaURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite;
- (NSURLRequest *)connection:(CoronaURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse;

- (NSString *)pathForTemporaryFileWithPrefix:(NSString *)prefix withDir:(NSString *)pathDir;

@end

// ----------------------------------------------------------------------------

@interface CoronaURLConnection ()

@property (nonatomic, retain) NetworkRequestParameters* fRequestParameters;
@property (nonatomic, retain) NSHTTPURLResponse *fResponse;
@property (nonatomic, retain) NSString* fTempFilePath;
@property (nonatomic, retain) NSFileHandle *fTempFile;
@property (nonatomic, retain) NSMutableData *fData;

@end

@implementation CoronaURLConnection

@synthesize fRequestParameters;
@synthesize fConnectionManager;
@synthesize fNetworkRequestState;
@synthesize fResponse;
@synthesize fTempFilePath;
@synthesize fTempFile;
@synthesize fData;

@synthesize fDelegate; // TODO: Remove when iOS workaround for Radar 10412199 is removed

// ----------------------------------------------------------------------------

#if TARGET_OS_IPHONE

// Workaround for Apple Radar: 10412199
// See also:
// http://stackoverflow.com/questions/2736967/nsmutableurlrequest-not-obeying-my-timeoutinterval
// https://devforums.apple.com/message/108292#108292
//
// setTimeoutForRequest / didTimeout / cancelTimeout are only needed for the workaround.
//

// Workaround for Apple Radar: 10412199
- (void)setTimeoutForRequest:(NSURLRequest*)request delegate:(id)delegate
{
	if (SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"6.0"))
	{
		NSLog(@"setTimeoutForRequest should not be called on iOS 6.0 or greater");
		return;
	}
	
	// On iOS, the timeout is not honored (i.e. it's always 240 sec)
	// when the request has a body
	if ( [request HTTPBody] )
	{
		NSTimeInterval timeout = [(CoronaURLRequest*)request timeout];
		if ( timeout > 0 )
		{
			[self performSelector:@selector(didTimeout:) withObject:delegate afterDelay:timeout];
		}
	}
}

// Workaround for Apple Radar: 10412199
- (void)didTimeout:(id)delegate
{
	if (SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"6.0"))
	{
		NSLog(@"didTimeout should not be called on iOS 6.0 or greater");
		return;
	}

	bool bReportError = [self reportErrors];
	if (bReportError)
	{
		if (fRequestParameters)
		{
			NSString *urlRequest = fRequestParameters.fRequestURL;
			NSString *msg = [NSString stringWithFormat:@"Connection timeout: %@", urlRequest];
			NSLog(@"%@",msg);
		}
	}

	//didFailWithError calls [connection end] which calls [self release]
	//This change was added to cancel at the NSURLConnection level only
	//and lets the didFailWithError routine call end/release
	[super cancel];

	NSError *error = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorTimedOut userInfo:[NSDictionary dictionaryWithObject:@"The request timed out." forKey:NSLocalizedDescriptionKey]];
	[delegate connection:self didFailWithError:error];
}

// Workaround for Apple Radar: 10412199
- (void)cancelTimeout
{
	if (SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"6.0"))
	{
		NSLog(@"cancelTimeout should not be called on iOS 6.0 or greater");
		return;
	}

	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(didTimeout:) object:fDelegate];
}

#endif // TARGET_OS_IPHONE

- (bool)reportErrors
{
	bool result = true;
	if (fRequestParameters)
	{
		NSString *urlRequest = fRequestParameters.fRequestURL;
		if ([urlRequest rangeOfString:@"stats.coronalabs.com"].length > 0 || // contains
		    [urlRequest hasPrefix:@"https://api.intercom.io"]  ||
		    [urlRequest hasPrefix:@"https://monetize-api.coronalabs.com"]  ||
			[urlRequest rangeOfString:@"coronalabs.com/links/homescreen/manualfeed.json"].length > 0 ||
			[urlRequest rangeOfString:@"coronalabs.com/links/homescreen/feed.json"].length > 0)
		{
			result = false;
		}
	}
	return result;
}


// ----------------------------------------------------------------------------

-(id)initWithRequest:(CoronaURLRequest *)request networkRequestParameters:(NetworkRequestParameters*)requestParams connectionManager:(ConnectionManager *)connectionManager;
{
	CoronaConnectionDelegate* delegate = [[CoronaConnectionDelegate alloc] init];
	[delegate autorelease];

	self = [super initWithRequest:request delegate:delegate];
	if ( self )
	{
		self.fConnectionManager = connectionManager;
		[fConnectionManager onStartConnection:self];
		
		self.fRequestParameters = requestParams;
		fNetworkRequestState = [[NetworkRequestState alloc] initWithUrl: [requestParams fRequestURL] isDebug: [requestParams fIsDebug]];
		
		NSRequestCanceller* requestCanceller = [[NSRequestCanceller alloc] initWithURLConnection:self];
		self.fNetworkRequestState.fRequestCanceller = requestCanceller;
		[requestCanceller release];

		fData = [[NSMutableData alloc] initWithCapacity:0];
		fResponse = nil;
		
#if TARGET_OS_IPHONE
		if (SYSTEM_VERSION_LESS_THAN(@"6.0"))
		{
			// Workaround for Apple Radar: 10412199
			self.fDelegate = delegate;
			[self setTimeoutForRequest:request delegate:delegate];
		}
#endif // TARGET_OS_IPHONE

		if ( Upload == fRequestParameters.fProgressDirection )
		{
			fNetworkRequestState.fBytesEstimated = fRequestParameters.fRequestBodySize;
			fNetworkRequestState.fPhase = @"began";
			[fRequestParameters.fLuaCallback callWithNetworkRequestState: fNetworkRequestState];
		}
	}
	return self;
}

-(void)invalidate
{
	// We have to invalidate stale data in the request params, e.g. LuaState ptr
	[self.fRequestParameters invalidate];
}

-(void)cancel
{
	// Don't allow cancel in the "ended" phase as things are in an indeterminate state
	if (! [self.fNetworkRequestState.fPhase isEqualToString:@"ended"])
	{
		[super cancel];
		[self end];
	}
}

// Doing meaningful cleanup in the destructor turned out to be kind of a train wreck (in part because
// it was sometimes called from another thread, and it part because it was sometimes called during/after
// the Lua state tear down).
//
// So now we have an explicit "end" method that is called from one of the three possible states under
// which a connection is closed/completed (was cancelled, finished, or finished with error), which
// cleans up the connection (in each of these cases, we are gauranteed that the connection object
// will receive no futher messages).
//
-(void)end
{
	[fConnectionManager onEndConnection:self];

    // The request canceller(s) will hang out until Lua GCs them, so we need to nil out their
	// connection to prevent them from trying to reference it later...
	//
	fNetworkRequestState.fRequestCanceller.fConnection = nil;

#if TARGET_OS_IPHONE
	if (SYSTEM_VERSION_LESS_THAN(@"6.0"))
	{
		// Workaround for Apple Radar: 10412199
		[self cancelTimeout];
	}
#endif // TARGET_OS_IPHONE

    self.fRequestParameters = nil;
    self.fConnectionManager = nil;
	self.fNetworkRequestState = nil;
	self.fResponse = nil;
	self.fTempFilePath = nil;
	self.fTempFile = nil;
	self.fData = nil;

	debug(@"Connection retain count before release on end: %i", self.retainCount);
	[self release];
}

-(void)dealloc
{
	debug(@"Dealloc CoronaURLConnection: %@", self);
	[super dealloc];
}

@end

// ----------------------------------------------------------------------------

@implementation CoronaConnectionDelegate

- (NSString *)pathForTemporaryFileWithPrefix:(NSString *)prefix withDir:(NSString *)pathDir 
{
	NSString *  result;
	CFUUIDRef   uuid;
	CFStringRef uuidStr;

	uuid = CFUUIDCreate(NULL);
	assert(uuid != NULL);

	uuidStr = CFUUIDCreateString(NULL, uuid);
	assert(uuidStr != NULL);

	result = [pathDir stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@", prefix, uuidStr]];
	
	assert(result != nil);

	CFRelease(uuidStr);
	CFRelease(uuid);

	return result;
}

- (void)connection:(CoronaURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	// Initial response phase (end of headers).  Watch out for redirects when recording
	// response info...
	//
	connection.fResponse = (NSHTTPURLResponse *)response;
	
	debug(@"Got inital response / headers - status: %i", connection.fResponse.statusCode);

	connection.fNetworkRequestState.fResponseHeaders = connection.fResponse.allHeaderFields;
	
	if ( Upload != connection.fRequestParameters.fProgressDirection )
	{
		connection.fNetworkRequestState.fBytesEstimated = connection.fResponse.expectedContentLength;
		connection.fNetworkRequestState.fBytesTransferred = 0;
	}
	
	if ( Download == connection.fRequestParameters.fProgressDirection )
	{
		connection.fNetworkRequestState.fStatus = connection.fResponse.statusCode;
		connection.fNetworkRequestState.fPhase = @"began";
		[connection.fRequestParameters.fLuaCallback callWithNetworkRequestState: connection.fNetworkRequestState];
	}

#if TARGET_OS_IPHONE
	if (SYSTEM_VERSION_LESS_THAN(@"6.0"))
	{
		// Workaround for Apple Radar: 10412199
		[connection cancelTimeout];
	}
#endif // TARGET_OS_IPHONE

	// Per Apple, this can be called multiple times, for example in the case of a redirect, so each time
	// we reset the data.  (Note than in practice, using both direct and relative redirects, this does not
	// appear to actually get called multiple times, but better safe than sorry).
	//
	if ( nil != connection.fTempFile )
	{
		[connection.fTempFile closeFile];

		NSError *error = nil;
		[[NSFileManager defaultManager] removeItemAtPath: connection.fTempFilePath error: &error];
		connection.fTempFilePath = nil;
		connection.fTempFile = nil;
	}
	else
	{
		[connection.fData setLength:0];		
	}
}

- (void)connection:(CoronaURLConnection *)connection didReceiveData:(NSData *)data
{
	debug(@"Got some data");

	// This method is called incrementally as the server sends data; we must concatenate the data to assemble the response

	if ( nil == connection.fTempFile )
	{
		// We only want to write result to file if result was success and a response file was indicated.  We create it
		// here because there's no point in creating it until we actually have some data to write to it.
		//
		if ( ( 200 == connection.fResponse.statusCode ) && ( connection.fRequestParameters.fResponse ) )
		{
			// Create a temp file to catch the data...
			//

			NSString *pathDir = nil;
			if (connection.fRequestParameters.fResponse.fFullPath)
			{
				pathDir = [connection.fRequestParameters.fResponse.fFullPath stringByDeletingLastPathComponent];
			}
			
			
			connection.fTempFilePath = [self pathForTemporaryFileWithPrefix:@"download" withDir:pathDir];
			debug(@"Creating temp file for download at path %@", connection.fTempFilePath);

			[[NSFileManager defaultManager] createFileAtPath:connection.fTempFilePath contents:nil attributes:nil];
			connection.fTempFile = [NSFileHandle fileHandleForWritingAtPath:connection.fTempFilePath];
		}
	}
	
	long long totalLength = 0;
	
	if ( connection.fTempFile )
	{
		[connection.fTempFile writeData:data];
		totalLength = [connection.fTempFile seekToEndOfFile]; // Also positions for next write, if any...
	}
	else
	{
		[connection.fData appendData:data];
		totalLength = connection.fData.length;
	}
		
	if ( Upload != connection.fRequestParameters.fProgressDirection )
	{
		connection.fNetworkRequestState.fBytesTransferred = totalLength;
	}

	if ( Download == connection.fRequestParameters.fProgressDirection )
	{
		connection.fNetworkRequestState.fPhase = @"progress";
		[connection.fRequestParameters.fLuaCallback callWithNetworkRequestState: connection.fNetworkRequestState];
	}
}

- (void)connectionDidFinishLoading:(CoronaURLConnection *)connection
{
	// This method is called when the response is complete and no further data is coming

	debug(@"Got response");

	NSMutableData *data = connection.fData;
	
	if ( nil != connection.fTempFile )
	{
		// Overwrite possible existing file with temp file contents (mostly) atomically...
		//
		[connection.fTempFile closeFile];

		debug(@"Moving file at path %@ to path %@", connection.fTempFilePath, connection.fRequestParameters.fResponse.fFullPath);
		NSError *error = nil;
		if ( [[NSFileManager defaultManager] fileExistsAtPath:connection.fRequestParameters.fResponse.fFullPath] )
		{
			[[NSFileManager defaultManager] removeItemAtPath:connection.fRequestParameters.fResponse.fFullPath error: &error];
		}
		[[NSFileManager defaultManager] moveItemAtPath:connection.fTempFilePath toPath:connection.fRequestParameters.fResponse.fFullPath error: &error];
		if ( error )
		{
			// Failure to rename temp file to desired output file.  Error out...
			//
			connection.fNetworkRequestState.fIsError = true;
			connection.fNetworkRequestState.fResponse = error.localizedDescription;
			connection.fNetworkRequestState.fResponseType = @"text";
			[connection.fNetworkRequestState setDebugValue: error.localizedDescription forKey: @"errorMessage"];
			debug(@"Temp file rename error: %@", error);
		}
		
		connection.fTempFilePath = nil;
		connection.fTempFile = nil;

		connection.fNetworkRequestState.fResponse = connection.fRequestParameters.fResponse;
		connection.fNetworkRequestState.fResponseType = @"binary";
	}
	else
	{
		// Output to Lua string...
		//
		debug(@"Got response of length: %i", data.length);
		
		NSString* charset = connection.fResponse.textEncodingName;
		if ( charset )
		{
			debug(@"Charset from protocol: %@", charset);
			[connection.fNetworkRequestState setDebugValue: charset forKey: @"charset"];
			[connection.fNetworkRequestState setDebugValue: @"protocol" forKey: @"charsetSource"];
		}
		else if ( isContentTypeText( connection.fResponse.MIMEType ) )
		{
			NSString* prefix = [[NSString alloc] initWithData:[data subdataWithRange:NSMakeRange(0, fmin([data length],1000))] encoding:NSASCIIStringEncoding];
			charset = getEncodingFromContent( connection.fResponse.MIMEType, prefix );
			if ( charset )
			{
				debug(@"Charset from content: %@", charset);
				[connection.fNetworkRequestState setDebugValue: charset forKey: @"charset"];
				[connection.fNetworkRequestState setDebugValue: @"content" forKey: @"charsetSource"];
			}
			else
			{
				charset = @"UTF-8";
				debug(@"Charset implicit (text default): %@", charset);
				[connection.fNetworkRequestState setDebugValue: charset forKey: @"charset"];
				[connection.fNetworkRequestState setDebugValue: @"implicit" forKey: @"charsetSource"];
			}
		}

		NSStringEncoding encoding = 0;
		if ( charset )
		{
			encoding = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding((CFStringRef)charset));
			debug(@"Got encoding: %i", encoding);
			if ( (int)encoding <= 0 )
			{
				debug(@"Character encoding specified was invalid (was: %@), will decode content using UTF-8 instead", charset);
				encoding = NSUTF8StringEncoding;
			}
		}
		
		if ( 0 != encoding )
		{
			debug(@"writing data as text to response string");
			NSString *responseString = [[NSString alloc] initWithData:data encoding:encoding];
			if (responseString != nil)
			{
				connection.fNetworkRequestState.fResponse = responseString;
				[responseString autorelease];
				connection.fNetworkRequestState.fResponseType = @"text";
			}
			else
			{
				connection.fNetworkRequestState.fResponse = data;
				connection.fNetworkRequestState.fResponseType = @"binary";
			}
		}
		else
		{
			debug(@"writing binary data to response string");
			connection.fNetworkRequestState.fResponse = data;
			connection.fNetworkRequestState.fResponseType = @"binary";
		}
	}

	connection.fNetworkRequestState.fStatus = connection.fResponse.statusCode;
	connection.fNetworkRequestState.fPhase = @"ended";
	[connection.fRequestParameters.fLuaCallback callWithNetworkRequestState: connection.fNetworkRequestState];
	
	[connection end];
}

- (void)connection:(CoronaURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
	// Body upload status for PUT/POST requests

	if ( Upload == connection.fRequestParameters.fProgressDirection )
	{
		connection.fNetworkRequestState.fBytesTransferred = totalBytesWritten;
		connection.fNetworkRequestState.fPhase = @"progress";
		[connection.fRequestParameters.fLuaCallback callWithNetworkRequestState: connection.fNetworkRequestState];
	}

	debug(@"Sent body data - bytes: %i, total bytes: %i, expected total bytes: %i", bytesWritten, totalBytesWritten, totalBytesExpectedToWrite);
}

- (void)connection:(CoronaURLConnection *)connection didFailWithError:(NSError *)theError
{
#if TARGET_OS_IPHONE
	if (SYSTEM_VERSION_LESS_THAN(@"6.0"))
	{
		// Workaround for Apple Radar: 10412199
		[connection cancelTimeout];
	}
#endif // TARGET_OS_IPHONE

	if ( connection.fTempFile )
	{
		// Delete the temp file
		//
		[connection.fTempFile closeFile];
		
		NSError *fileError = nil;
		[[NSFileManager defaultManager] removeItemAtPath: connection.fTempFilePath error: &fileError];
		connection.fTempFilePath = nil;
		connection.fTempFile = nil;
	}
	
	NSString *errorMessage = [theError localizedDescription];
	
	
	bool bReportError = true;
	if (connection)
	{
		bReportError = [connection reportErrors];
	}
	
	if (bReportError)
	{
		error(@"network request failed: %@ [%d: %@]",
				connection.fRequestParameters.fRequestURL, theError.code, errorMessage);
	}
	
	[connection.fNetworkRequestState setDebugValue: errorMessage forKey: @"errorMessage"];

	connection.fNetworkRequestState.fIsError = true;
	connection.fNetworkRequestState.fResponse = errorMessage;
	connection.fNetworkRequestState.fResponseType = @"text";
	connection.fNetworkRequestState.fStatus = -1;
	connection.fNetworkRequestState.fPhase = @"ended";
	[connection.fRequestParameters.fLuaCallback callWithNetworkRequestState: connection.fNetworkRequestState];
	
	[connection end];
	
}

- (NSURLRequest *)connection:(CoronaURLConnection *)connection willSendRequest:(NSURLRequest *)request redirectResponse:(NSURLResponse *)redirectResponse
{
    if (! connection.fRequestParameters.fHandleRedirects && redirectResponse != nil)
    {
        // denied (or this is an internal state indicated by the nil redirectResponse)
        return nil;
    }
    else
    {
        // continue as normal
        return request;
    }
}

@end

// ----------------------------------------------------------------------------

