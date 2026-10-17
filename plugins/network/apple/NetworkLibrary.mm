//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#import "NetworkLibrary.h"

#include "CoronaLibrary.h"
#include "CoronaRuntime.h"

#import "AppleNetworkSupport.h"
#import "AppleNetworkRequest.h"
#import "PlatformReachability.h"
#import "AppleReachability.h"

#import <SystemConfiguration/SystemConfiguration.h>
#import <netinet/in.h>

// ----------------------------------------------------------------------------

CORONA_EXPORT int CoronaPluginLuaLoad_network( lua_State * );

namespace Corona
{

// ----------------------------------------------------------------------------

class NetworkLibrary
{
	public:
		typedef NetworkLibrary Self;

	public:
		static const char kName[];
		static const char kEvent[];

    private:
        int fSystemEventListener;
		NSMutableDictionary *fReachabilityListeners;
		ConnectionManager *fConnectionManager;

	protected:
		NetworkLibrary();

	public:
		static int Open( lua_State *L );

	protected:
		static int Finalizer( lua_State *L );

	public:
		static Self *ToLibrary( lua_State *L );

	protected:
		static int ValueForKey( lua_State *L );

	protected:
		static int setStatusListener( lua_State *L );
		static int request( lua_State *L );
		static int cancel( lua_State *L );
		static int getConnectionStatus( lua_State *L );

	protected:
        void onStarted( lua_State *L );
        void onSuspended( lua_State *L );
        void onResumed( lua_State *L );
        void onExiting( lua_State *L );
    
	protected:
        static int ProcessSystemEvent( lua_State *L );
        static int AddSystemEventListener( lua_State *L, NetworkLibrary *networkLibrary );
        static int RemoveSystemEventListener( lua_State *L, int systemEventListenerRef );
};

// ----------------------------------------------------------------------------

// This corresponds to the name of the library, e.g. [Lua] require "plugin.library"
const char NetworkLibrary::kName[] = "plugin.network";

// This corresponds to the event name, e.g. [Lua] event.name
const char NetworkLibrary::kEvent[] = "networkLibraryEvent";

NetworkLibrary::NetworkLibrary()
{
 	debug(@"NetworkLibrary::NetworkLibaray");
    fSystemEventListener = LUA_REFNIL;
	fReachabilityListeners = [[NSMutableDictionary alloc] init];
	fConnectionManager = [[ConnectionManager alloc] init];
}

// CoronaRuntimeListener
void
NetworkLibrary::onStarted( lua_State *L )
{
    debug(@"onStarted");
}

// CoronaRuntimeListener
void
NetworkLibrary::onSuspended( lua_State *L )
{
    debug(@"onSuspended");
}

// CoronaRuntimeListener
void
NetworkLibrary::onResumed( lua_State *L )
{
    debug(@"onResumed");
}

// CoronaRuntimeListener
void
NetworkLibrary::onExiting( lua_State *L )
{
    debug(@"onExiting");
    fSystemEventListener = NetworkLibrary::RemoveSystemEventListener(L, fSystemEventListener );
    
    // Cancel any open connections
	//
	[fConnectionManager cancelAllConnections];
}
    
int
NetworkLibrary::Open( lua_State *L )
{
	// debug(@"Module being registered - system version: %@", [[UIDevice currentDevice] systemVersion]);

	// Register __gc callback
	const char kMetatableName[] = __FILE__; // Globally unique string to prevent collision
	CoronaLuaInitializeGCMetatable( L, kMetatableName, Finalizer );
	
	// Register the NSRequestCanceller Lua "class" (metatable)
	//
	[NSRequestCanceller registerClassWithLuaState:L];
	
	// Functions in library
	const luaL_Reg kVTable[] =
	{
		{ "setStatusListener", setStatusListener },
		{ "request_native", request },
		{ "cancel", cancel },
		{ "getConnectionStatus", getConnectionStatus },

		{ NULL, NULL }
	};

	Self *library = new Self;

    library->fSystemEventListener = NetworkLibrary::AddSystemEventListener( L, library );

	// Store the library singleton in the registry so it persists
	// using kMetatableName as the unique key.
	CoronaLuaPushUserdata( L, library, kMetatableName );
	lua_pushstring( L, kMetatableName );
	lua_settable( L, LUA_REGISTRYINDEX );

	// Leave "library" on top of stack
	// Set library as upvalue for each library function
	lua_CFunction factory = Corona::Lua::Open< CoronaPluginLuaLoad_network >;
	int result = CoronaLibraryNewWithFactory( L, factory, kVTable, library );

	if ( result > 0 )
	{
		lua_pushcclosure( L, ValueForKey, 0 );
		CoronaLibrarySetExtension( L, -2 );
		
	}

	return result;
}
	
int
NetworkLibrary::Finalizer( lua_State *L )
{
	debug(@"Unloading network library");

	Self *library = (Self *)CoronaLuaToUserdata( L, 1 );
	
	// Release any remaining reachability listeners
	//
	
	NSArray *reachabilityHosts = [library->fReachabilityListeners allKeys];
	for( NSString *host in reachabilityHosts )
	{
		AppleReachability *pReachability = (AppleReachability*)[[library->fReachabilityListeners objectForKey:host] pointerValue];

		// Remove it from the dictionary
		[library->fReachabilityListeners removeObjectForKey:host];
		
		// Dereference any current listener
		pReachability->SetListener(L, nil);
		
		// Release it...
		delete pReachability;
		
	}
	
	[library->fConnectionManager cancelAllConnections];

	[library->fReachabilityListeners release];
	[library->fConnectionManager release];

	delete library;

	return 0;
}

NetworkLibrary *
NetworkLibrary::ToLibrary( lua_State *L )
{
	// library is pushed as part of the closure
	Self *library = (Self *)lua_touserdata( L, lua_upvalueindex( 1 ) );
	return library;
}

int
NetworkLibrary::ValueForKey( lua_State *L )
{
	int result = 0;

	const char *key = lua_tostring( L, 2 );
	if ( key )
	{
		if ( 0 == strcmp( key, "canDetectNetworkStatusChanges" ) )
		{
			// On Mac/IOS, we can detect
			lua_pushboolean( L, 1 );
			result = 1;
		}
	}
	
	return result;
}

// [Lua] network.setStatusListener( )
int
NetworkLibrary::setStatusListener( lua_State *L )
{
	debug(@"NetworkLibrary::setStatusListener()");
	
	Self *library = ToLibrary( L );

	// First argument - host name (required)
	//
	if ( LUA_TSTRING == lua_type( L, 1 ) )
	{
		const char *host = lua_tostring( L, 1 );
		NSString *hostString = [NSString stringWithUTF8String:host];

		AppleReachability *pReachability = (AppleReachability*)[[library->fReachabilityListeners objectForKey:hostString] pointerValue];

		// Second argument - listener (optional)
		//
		if ( lua_isnoneornil( L, 2 ) )
		{
			// nil listener, remove reachability for this host, if any
			//
			if (nil != pReachability)
			{
				// Remove it from the dictionary
				[library->fReachabilityListeners removeObjectForKey:hostString];

				// Dereference any current listener
				pReachability->SetListener(L, nil);

				// Release it...
				delete pReachability;
			}
		}
		else if ( CoronaLuaIsListener( L, 2, PlatformReachability::kReachabilityListenerEvent ) )
		{
			// listener provided.  If existing reachability for this host, update listener, else
			// add new reachabiity for this host.
			//
			CoronaLuaRef ref = CoronaLuaNewRef( L, 2 );

			if (nil == pReachability)
			{
				// Create a new Reachability for this host...
				pReachability = new AppleReachability( L, PlatformReachability::kHostName, host );
				
				// Add it from the dictionary
				[library->fReachabilityListeners setObject:[NSValue valueWithPointer:pReachability] forKey:hostString];
			}
			
			// Set/update the listener ref
			//
			pReachability->SetListener(L, ref);
		}
		else
		{
			paramValidationFailure( L, @"Second argument to network.setStatusListener(), if provided, must be a listener or nil" );
		}
	}
	else
	{
		paramValidationFailure( L, @"First argument to network.setStatusListener() should be a host name string" );
	}
	
	return 0;
}

// [Lua] network.request( )
int
NetworkLibrary::request( lua_State *L )
{
	debug(@"request() called");

	Self *library = ToLibrary( L );

	int nPushed = 0;
	
	NetworkRequestParameters* requestParams = [[NetworkRequestParameters alloc] initWithLuaState: L];
	if (requestParams.fIsValid)
	{
		NSError* error = nil;
		CoronaURLRequest* request = [[[CoronaURLRequest alloc] initWithNetworkRequestParameters:requestParams error:&error] autorelease];
		if ( nil == request )
		{
			[requestParams release];
			luaL_error( L, [error.localizedDescription UTF8String] );
			return 0;
		}
		
		CoronaURLConnection *connection = [[CoronaURLConnection alloc] initWithRequest:request networkRequestParameters:requestParams connectionManager:library->fConnectionManager];
		[connection start];
		
		[requestParams autorelease];
		
		NSRequestCanceller* requestCanceller = connection.fNetworkRequestState.fRequestCanceller;
		nPushed = [requestCanceller pushToLuaState: L];
	}
	
	debug(@"Exiting request()");
	
	return nPushed;	
}

// [Lua] network.cancel( )
int
NetworkLibrary::cancel( lua_State *L )
{
	int nPushed = 0;
	
	if ( ! lua_isnil( L, 1 ) && isudatatype( L, 1, [NSRequestCanceller metatableName]) )
	{
		NSRequestCanceller* requestCanceller = [NSRequestCanceller checkWithLuaState: L index: 1];
#if TARGET_OS_IPHONE
		debug(@"cancel() for request: %@", requestCanceller.fConnection.originalRequest.URL);
#endif // TARGET_OS_IPHONE
		[requestCanceller cancel];
		
		lua_pushboolean( L, 1 );
		nPushed++;
	}
	else
	{
		paramValidationFailure( L, @"network.cancel() expects a requestId returned from a call to network.request()" );
	}
	
	return nPushed;
}

// [Lua] network.getConnectionStatus( )
int
NetworkLibrary::getConnectionStatus( lua_State *L )
{
	// Create "zero" Internet socket addr
	struct sockaddr_in zeroAddr;
	bzero(&zeroAddr, sizeof(zeroAddr));
	zeroAddr.sin_len = sizeof(zeroAddr);
	zeroAddr.sin_family = AF_INET;
	
	SCNetworkReachabilityRef target = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *) &zeroAddr);
	
	SCNetworkReachabilityFlags flags;
	SCNetworkReachabilityGetFlags(target, &flags);
	CFRelease(target);
	
	bool isConnected = (flags & kSCNetworkFlagsReachable);
	bool isMobile = false;

#if TARGET_OS_IPHONE
	isMobile = (flags & kSCNetworkReachabilityFlagsIsWWAN);
#endif

	lua_createtable(L, 0, 2);
	int luaTableStackIndex = lua_gettop(L);
	
	lua_pushboolean(L, isConnected);
	lua_setfield(L, luaTableStackIndex, "isConnected");
	
	lua_pushboolean(L, isMobile);
	lua_setfield(L, luaTableStackIndex, "isMobile");
	
	return 1;
}

// This static method receives "system" event messages from Corona, at which point it determines the instance
// that registered the listener and dispatches the system events to that instance.
//
int
NetworkLibrary::ProcessSystemEvent( lua_State *luaState )
{
    void *ud = lua_touserdata( luaState, lua_upvalueindex( 1 ) );
    NetworkLibrary *networkLibrary = (NetworkLibrary *)ud;
    lua_getfield( luaState, 1, "type" );
    const char *eventType = lua_tostring( luaState, -1 );
    lua_pop( luaState, 1);
    
    if ( 0 == strcmp("applicationStart", eventType) )
    {
        networkLibrary->onStarted( luaState );
    }
    else if ( 0 == strcmp("applicationSuspend", eventType) )
    {
        networkLibrary->onSuspended( luaState );
    }
    else if ( 0 == strcmp("applicationResume", eventType) )
    {
        networkLibrary->onResumed( luaState );
    }
    else if ( 0 == strcmp("applicationExit", eventType) )
    {
        networkLibrary->onExiting( luaState );
    }
    
    return 0;
}

int
NetworkLibrary::AddSystemEventListener( lua_State *L, NetworkLibrary *networkLibrary )
{
	int ref = LUA_REFNIL;
	
	// Does the equivalent of the following Lua code:
	//   Runtime:addEventListener( "system", ProcessSystemEvent )
	// which is equivalent to:
	//   local f = Runtime.addEventListener
	//   f( Runtime, "system", ProcessSystemEvent )
	CoronaLuaPushRuntime( L ); // push 'Runtime'
	if ( lua_type( L, -1 ) == LUA_TTABLE )
	{
		lua_getfield( L, -1, "addEventListener" ); // push 'f', i.e. Runtime.addEventListener
		lua_insert( L, -2 ); // swap so 'f' is below 'Runtime'
		lua_pushstring( L, "system" );

		// Push ProcessSystemEvent as closure so it has access to 'networkLibrary'
		lua_pushlightuserdata( L, networkLibrary ); // Assumes 'networkLibrary' lives for lifetime of plugin
		lua_pushcclosure( L, &NetworkLibrary::ProcessSystemEvent, 1 );
		
		// Register reference to C closure so we can use later when we need to remove the listener
		ref = luaL_ref(L, LUA_REGISTRYINDEX);
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		
		// Lua stack order (from lowest index to highest):
		// f
		// Runtime
		// "system"
		// ProcessSystemEvent (closure)
		CoronaLuaDoCall( L, 3, 0 );
	}
	else
	{
		lua_pop( L, 1 ); // pop nil
	}

	return ref;
}

int
NetworkLibrary::RemoveSystemEventListener( lua_State *L, int systemEventListenerRef )
{
    // Does the equivalent of the following Lua code:
    //   Runtime:removeEventListener( "system", ProcessSystemEvent )
    // which is equivalent to:
    //   local f = Runtime.removeEventListener
    //   f( Runtime, "system", ProcessSystemEvent )
    CoronaLuaPushRuntime( L ); // push 'Runtime'
	if ( lua_type( L, -1 ) == LUA_TTABLE )
	{
		lua_getfield( L, -1, "removeEventListener" ); // push 'f', i.e. Runtime.removeEventListener
		lua_insert( L, -2 ); // swap so 'f' is below 'Runtime'
		lua_pushstring( L, "system" );

		// Push reference to the C closure that was used in "addEventListener"
		lua_rawgeti(L, LUA_REGISTRYINDEX, systemEventListenerRef);

		// Lua stack order (from lowest index to highest):
		// f
		// Runtime
		// "system"
		// ProcessSystemEvent (closure)
		CoronaLuaDoCall( L, 3, 0 );
		debug(@"Removed system event listener");

		luaL_unref(L, LUA_REGISTRYINDEX,  systemEventListenerRef);
	}
	else
	{
		lua_pop( L, 1 ); // pop nil
	}
	return LUA_REFNIL;
}

// ----------------------------------------------------------------------------

} // namespace Corona

// ----------------------------------------------------------------------------

CORONA_EXPORT int luaopen_network( lua_State *L )
{
	return Corona::NetworkLibrary::Open( L );
}
