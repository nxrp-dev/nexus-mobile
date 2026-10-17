//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#import <Foundation/Foundation.h>

#import "CoronaRuntime.h"
#import "CoronaLua.h"

void debug( NSString *message, ... );
void error( NSString *message, ... );

// ----------------------------------------------------------------------------

void paramValidationFailure( lua_State *luaState, NSString *message, ... );

// ----------------------------------------------------------------------------

Boolean isudatatype(lua_State *luaState, int idx, const char *name);

// ----------------------------------------------------------------------------

NSString* getContentTypeEncoding( NSString* contentTypeHeader );

Boolean isContentTypeXML ( NSString* contentType );

Boolean isContentTypeHTML ( NSString* contentType );

Boolean isContentTypeText ( NSString* contentType );

NSString* getEncodingFromContent ( NSString* contentType, NSString* content );

// ----------------------------------------------------------------------------

@class CoronaURLConnection;

// ----------------------------------------------------------------------------

@interface ConnectionManager : NSObject

@property (nonatomic, retain) NSMutableArray* fConnectionList;

-(id)init;
-(void)onStartConnection: (CoronaURLConnection *) connection;
-(void)onEndConnection: (CoronaURLConnection *) connection;
-(void)cancelAllConnections;

@end

// ----------------------------------------------------------------------------

@interface NSRequestCanceller : NSObject

@property (nonatomic, assign) CoronaURLConnection* fConnection;
@property Boolean fIsCancelled;

+(const char*)metatableName;
+(void)registerClassWithLuaState: (lua_State *) luaState;
+(NSRequestCanceller*)checkWithLuaState: (lua_State *) luaState index: (int) index;

-(id)initWithURLConnection: (CoronaURLConnection *) connection;
-(int)pushToLuaState: (lua_State*) luaState;
-(void)cancel;

@end

// ----------------------------------------------------------------------------

typedef enum {
	UNKNOWN		= 0,
	Upload		= 1,
	Download	= 2,
	None		= 3,
} ProgressDirection;

@interface NSString (progress)

+ (NSString*)stringWithProgressDirection:(ProgressDirection)direction;
- (ProgressDirection)progressDirectionFromString;

@end

// ----------------------------------------------------------------------------

@interface CoronaFileSpec : NSObject

@property (nonatomic, retain)	NSString*	fFilename;
@property (nonatomic)			void*		fBaseDirectory;
@property (nonatomic, retain)	NSString*	fFullPath;
@property (nonatomic)			Boolean		fIsResourceFile;

-(id)initWithFilename: (NSString*) filename baseDirectory:(void*)baseDirectory fullPath:(NSString*)fullPath isResourceFile:(Boolean)isResourceFile;

@end

// ----------------------------------------------------------------------------

@interface NetworkRequestState : NSObject

@property (nonatomic)			Boolean             fIsError;
@property (nonatomic, retain)	NSString*           fName;
@property (nonatomic, retain)	NSString*           fPhase;
@property (nonatomic)			NSInteger           fStatus;
@property (nonatomic, retain)	NSString*           fRequestURL;
@property (nonatomic, retain)	NSDictionary*       fResponseHeaders;
@property (nonatomic, retain)	NSString*           fResponseType;
@property (nonatomic, retain)	NSObject*           fResponse;	// NSString, NSData, or CoronaFileSpec
@property (nonatomic, retain)   NSRequestCanceller* fRequestCanceller;
@property (nonatomic)			long long           fBytesTransferred;
@property (nonatomic)			long long           fBytesEstimated;
@property (nonatomic, retain)	NSDictionary*       fDebugValues;

-(id)initWithUrl: (NSString *) url isDebug:(Boolean)isDebug;

-(void)setDebugValue: (NSString *) debugValue forKey:(NSString *) debugKey;

-(int)pushToLuaState: (lua_State *) luaState;

@end

// ----------------------------------------------------------------------------

@interface LuaCallback : NSObject

@property (nonatomic, assign)	lua_State*          fLuaState;
@property (nonatomic)			CoronaLuaRef		fLuaReference;

@property (nonatomic, readonly) NSString*			fLastNotificationPhase;
@property (nonatomic)			long				fMinNotificationIntervalMs;
@property (nonatomic)			double				fLastNotificationTime;

-(id)initWithLuaState: (lua_State*) luaState reference: (CoronaLuaRef) luaReference;

-(Boolean)callWithNetworkRequestState: (NetworkRequestState*) networkRequestState;

-(void)invalidate;

@end

// ----------------------------------------------------------------------------

@interface NetworkRequestParameters : NSObject

@property (nonatomic, assign) NSString*			fRequestURL;
@property (nonatomic, assign) NSString*			fMethod;
@property (nonatomic, retain) NSDictionary*		fRequestHeaders;
@property (nonatomic) Boolean					fIsBodyTypeText;
@property (nonatomic) ProgressDirection			fProgressDirection;
@property (nonatomic) int						fTimeout;
@property (nonatomic) Boolean					fIsDebug;
@property (nonatomic, retain) NSObject*			fRequestBody; // NSString, NSData, or CoronaFileSpec
@property (nonatomic)         long long         fRequestBodySize;
@property (nonatomic, retain) CoronaFileSpec*	fResponse;
@property (nonatomic, retain) LuaCallback*		fLuaCallback;
@property (nonatomic) Boolean                   fIsValid;
@property (nonatomic) Boolean                   fHandleRedirects;

-(id)initWithLuaState: (lua_State*) luaState;

-(void)invalidate;

@end
