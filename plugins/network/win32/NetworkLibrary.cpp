//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "NetworkLibrary.h"

#include "CoronaLibrary.h"
#include "CoronaLua.h"

#include "WindowsNetworkSupport.h"
#include "WinHttpRequestManager.h"
#include "WinInetConnectivity.h"

#include "WinTimer.h"

// ----------------------------------------------------------------------------

CORONA_EXPORT int CoronaPluginLuaLoad_network( lua_State * );

namespace Corona
{

// ----------------------------------------------------------------------------

class NetworkLibrary : WinHttpRequestManager
{
	public:
		typedef NetworkLibrary Self;

	public:
		static const char kName[];
		static const char kEvent[];

		int fSystemEventListener;

	protected:
		NetworkLibrary();
		~NetworkLibrary();

	public:
		static int Open( lua_State *L );

	protected:
		static int Finalizer( lua_State *L );

	public:
		static Self *ToLibrary( lua_State *L );

	protected:
		static int ValueForKey( lua_State *L );

	protected:
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
	debug("NetworkLibrary::NetworkLibaray");
	fSystemEventListener = LUA_REFNIL;
}

NetworkLibrary::~NetworkLibrary()
{
	debug("NetworkLibrary::~NetworkLibaray");
}

// CoronaRuntimeListener
void
NetworkLibrary::onStarted( lua_State *L ) 
{
	debug("onStarted");
}

// CoronaRuntimeListener
void
NetworkLibrary::onSuspended( lua_State *L )
{
	debug("onSuspended");
}

// CoronaRuntimeListener
void 
NetworkLibrary::onResumed( lua_State *L ) 
{
	debug("onResumed");
}

// CoronaRuntimeListener
void 
NetworkLibrary::onExiting( lua_State *L ) 
{
	debug("onExiting");
	fSystemEventListener = NetworkLibrary::RemoveSystemEventListener(L, fSystemEventListener );

	debug("Aborting any active requests");
	AbortAllRequests();
	ProcessRequestsUntil(5000);
}

int
NetworkLibrary::Open( lua_State *L )
{
	debug("Module being registered: %s", kName);

	// Register __gc callback
	const char kMetatableName[] = __FILE__; // Globally unique string to prevent collision
	CoronaLuaInitializeGCMetatable( L, kMetatableName, Finalizer );
	
	// Register the RequestCanceller Lua "class" (metatable)
	//
	RequestCanceller::registerClassWithLuaState( L );

	// Functions in library
	const luaL_Reg kVTable[] =
	{
		{ "request_native", request },
		{ "cancel", cancel },
		{ "getConnectionStatus", getConnectionStatus },

		{ NULL, NULL }
	};

	// Set library as upvalue for each library function
	Self *library = new Self;

	library->fSystemEventListener = NetworkLibrary::AddSystemEventListener( L, library );

	// The NetworkLibrary *is* the WinHttpRequestManager (which *is* the WinTimer), so we start
	// it here so it can start servicing requests.
	library->Start();

	// Store the library singleton in the registry so it persists
	// using kMetatableName as the unique key.
	CoronaLuaPushUserdata( L, library, kMetatableName );
	lua_pushstring( L, kMetatableName );
	lua_settable( L, LUA_REGISTRYINDEX );

	// Leave "library" on top of stack
	// Set library as upvalue for each library function
	lua_CFunction factory = Corona::Lua::Open< CoronaPluginLuaLoad_network >;
	return CoronaLibraryNewWithFactory( L, factory, kVTable, library );
}

int
NetworkLibrary::Finalizer( lua_State *L )
{
	Self *library = (Self *)CoronaLuaToUserdata( L, 1 );

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
		// if ( 0 == strcmp( key, "propertyName" ) )
	}
	
	return result;
}

// [Lua] network.request( )
int
NetworkLibrary::request( lua_State *L )
{
	debug("NetworkLibrary::request()");

	Self *library = NetworkLibrary::ToLibrary( L );

	int nPushed = 0;

	NetworkRequestParameters *requestParams = new NetworkRequestParameters( L );
	if (requestParams->isValid())
	{
		debug("Params valid, sending network request....");
		RequestCanceller *requestCanceller = library->SendNetworkRequest( requestParams );
		nPushed += requestCanceller->pushToLuaState( L );
	}

	return nPushed;	
}

// [Lua] network.cancel( )
int
NetworkLibrary::cancel( lua_State *L )
{
	debug("NetworkLibrary::cancel()");

	Self *library = NetworkLibrary::ToLibrary( L );

	int nPushed = 0;

	if ( ! lua_isnil( L, 1 ) && isudatatype( L, 1, RequestCanceller::getMetatableName()) )
	{
		RequestCanceller* requestCanceller = RequestCanceller::checkWithLuaState( L, 1 );
		requestCanceller->cancel();
		
		lua_pushboolean( L, 1 );
		nPushed++;
	}
	else
	{
		paramValidationFailure( L, "network.cancel() expects a requestId returned from a call to network.request()" );
	}

	return nPushed;
}

// [Lua] network.getConnectionStatus( )
int
NetworkLibrary::getConnectionStatus( lua_State *L )
{	
	DWORD connectionFlags = WinInetConnectivity::getConnectedState();
	bool isMobile = (connectionFlags & INET_CONNECTION_MODEM) != 0;
	bool isConnected = isMobile || ((connectionFlags & INET_CONNECTION_LAN) != 0);

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
		debug("Added system event listener");
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
		debug("Removed system event listener");
		
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
