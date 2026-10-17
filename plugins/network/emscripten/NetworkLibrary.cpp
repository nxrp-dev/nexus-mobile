//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Assert.h"
#include "NetworkLibrary.h"
#include "Corona/CoronaLibrary.h"
#include "EmscriptenNetworkSupport.h"
#include "Core/Rtt_Types.h"

#ifdef EMSCRIPTEN
#include "emscripten/emscripten.h"	
#endif

#ifdef WIN32
#define EMSCRIPTEN_KEEPALIVE
#endif

CORONA_EXPORT int CoronaPluginLuaLoad_network(lua_State *);

extern "C"
{
#ifdef WIN32
	int jsNetworkRequest(const char* url, const char* method, const char* headers, const unsigned char* buf, int buflen, bool progress, NetworkRequestParameters* request) { return 0; }
#else
	extern int jsNetworkRequest(const char* url, const char* method, const char* headers, const unsigned char* buf, int buflen, bool progress, NetworkRequestParameters* request);
#endif

	void EMSCRIPTEN_KEEPALIVE jsNetworkDispatch(NetworkRequestParameters* requestParams, int state, int status, int bodylen, const Uint8* body, const char* headers)
	{
		NetworkRequestState requestState;
		requestState.fResponseBody.bodyType = TYPE_NONE;
		requestState.fResponseBody.bodyBytes = NULL;

		if (status == 200 && state == 4 && headers != NULL)
		{
			requestState.setResponseHeaders(headers);
		}

		requestState.setURL(requestParams->getRequestUrl());
		requestState.setStatus(status);

		// It is worth noting that browsers report a status of 0 in case of XMLHttpRequest errors too.
		if (status != 200)
		{
			UTF8String *message = new  UTF8String("Network request failed");
			requestState.setError(message);
		}

		CoronaFileSpec *responseFile = requestParams->getResponseFile();
		if (responseFile != NULL)
		{
			if (status == 200)
			{
				requestState.fResponseBody.bodyType = TYPE_FILE;
				requestState.fResponseBody.bodyFile = new CoronaFileSpec(responseFile);
				UTF8String responseFileFullPath = responseFile->getFullPath();

				::FILE* f = ::fopen(responseFileFullPath.c_str(), "wb");
				if (f)
				{
					fwrite(body, 1, bodylen, f); 
					fclose(f);
				}
			}
		}
		else
		if (body != NULL && bodylen > 0)
		{
			requestState.fResponseBody.bodyType = TYPE_BYTES;
			requestState.fResponseBody.bodyBytes = new ByteVector(body, body + bodylen);
		}

		requestState.setPhase("ended");
		requestState.setBytesEstimated(bodylen);
		requestState.setBytesTransferred(bodylen);

		LuaCallback* func = requestParams->getLuaCallback();
		if (func)
		{ 
			func->callWithNetworkRequestState(&requestState);
		}

		delete requestParams;
	}
} 

// This corresponds to the name of the library, e.g. [Lua] require "plugin.library"
const char NetworkLibrary::kName[] = "plugin.network";

// This corresponds to the event name, e.g. [Lua] event.name
const char NetworkLibrary::kEvent[] = "networkLibraryEvent";

NetworkLibrary::NetworkLibrary()
{
	fSystemEventListener = LUA_REFNIL;
}

NetworkLibrary::~NetworkLibrary()
{
}

// CoronaRuntimeListener
void		NetworkLibrary::onStarted(lua_State *L)
{
}

// CoronaRuntimeListener
void		NetworkLibrary::onSuspended(lua_State *L)
{
}

// CoronaRuntimeListener
void		NetworkLibrary::onResumed(lua_State *L)
{
}

// CoronaRuntimeListener
void		NetworkLibrary::onExiting(lua_State *L)
{
	fSystemEventListener = NetworkLibrary::RemoveSystemEventListener(L, fSystemEventListener);

	// todo
	//		AbortAllRequests();
	//		ProcessRequestsUntil(5000);
}

int		NetworkLibrary::Open(lua_State *L)
{
	// Register __gc callback
	const char kMetatableName[] = __FILE__; // Globally unique string to prevent collision
	CoronaLuaInitializeGCMetatable(L, kMetatableName, Finalizer);

	// Register the RequestCanceller Lua "class" (metatable)
	//
	//		RequestCanceller::registerClassWithLuaState( L );

	// Functions in library
	const luaL_Reg kVTable[] =
	{
		{"request_native", request},
		{"cancel", cancel},
		{"getConnectionStatus", getConnectionStatus},

		{NULL, NULL}
	};

	// Set library as upvalue for each library function
	Self *library = new Self;

	library->fSystemEventListener = NetworkLibrary::AddSystemEventListener(L, library);

	// Store the library singleton in the registry so it persists
	// using kMetatableName as the unique key.
	CoronaLuaPushUserdata(L, library, kMetatableName);
	lua_pushstring(L, kMetatableName);
	lua_settable(L, LUA_REGISTRYINDEX);

	// Leave "library" on top of stack
	// Set library as upvalue for each library function
	lua_CFunction factory = Corona::Lua::Open < CoronaPluginLuaLoad_network > ;
	return CoronaLibraryNewWithFactory(L, factory, kVTable, library);
}

int
NetworkLibrary::Finalizer(lua_State *L)
{
	Self *library = (Self *)CoronaLuaToUserdata(L, 1);

	delete library;

	return 0;
}

NetworkLibrary*		NetworkLibrary::ToLibrary(lua_State *L)
{
	// library is pushed as part of the closure
	Self *library = (Self *)lua_touserdata(L, lua_upvalueindex(1));
	return library;
}

int	NetworkLibrary::ValueForKey(lua_State *L)
{
	int result = 0;

	const char *key = lua_tostring(L, 2);
	if (key)
	{
		// if ( 0 == strcmp( key, "propertyName" ) )
	}

	return result;
}

// [Lua] network.request( )
int	NetworkLibrary::request(lua_State *L)
{
	Self *library = NetworkLibrary::ToLibrary(L);

	// it will br deleted by callback
	NetworkRequestParameters* requestParams = new NetworkRequestParameters(L);
	if (requestParams->isValid() == false)
	{
		delete requestParams;
		return 0;
	}

	const Body* body = requestParams->getRequestBody();
	const unsigned char* buf = NULL;
	int buflen = 0;
	UTF8String postParams;
	switch (body->bodyType)
	{
	case TYPE_STRING:
		Rtt_ASSERT(body->bodyString);
		postParams = *body->bodyString;
		buf = (const unsigned char*)body->bodyString->c_str();
		buflen = body->bodyString->size();
		break;

	case TYPE_BYTES:
		Rtt_ASSERT(body->bodyString);
		buf = &body->bodyBytes->operator[](0);
		buflen = body->bodyBytes->size();
		break;

	case TYPE_NONE:
		break;

	default:
		Rtt_ASSERT(0 && "todo");
		break;
	}

	int xml = jsNetworkRequest(
		requestParams->getRequestUrl().c_str(),
		requestParams->getRequestMethod().c_str(),
		requestParams->getRequestHeaderString().c_str(),
		buf,
		buflen,
		requestParams->getProgressDirection(),
		requestParams);

	return 0;
}

// [Lua] network.cancel( )
int	NetworkLibrary::cancel(lua_State *L)
{
	Self *library = NetworkLibrary::ToLibrary(L);

	//int requestID = luaL_checkinteger(L, 1);
	//library->removeRequestParams(requestID);
	lua_pushboolean(L, 1);

	return 1;
}

// [Lua] network.getConnectionStatus( )
int		NetworkLibrary::getConnectionStatus(lua_State *L)
{
	//		DWORD connectionFlags = WinInetConnectivity::getConnectedState();
	bool isMobile = false; // fixme, (connectionFlags & INET_CONNECTION_MODEM) != 0;
	bool isConnected = true; // fixme, isMobile || ((connectionFlags & INET_CONNECTION_LAN) != 0);

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
int		NetworkLibrary::ProcessSystemEvent(lua_State *luaState)
{
	void *ud = lua_touserdata(luaState, lua_upvalueindex(1));
	NetworkLibrary *networkLibrary = (NetworkLibrary *)ud;

	lua_getfield(luaState, 1, "type");
	const char *eventType = lua_tostring(luaState, -1);
	lua_pop(luaState, 1);

	if (0 == strcmp("applicationStart", eventType))
	{
		networkLibrary->onStarted(luaState);
	}
	else if (0 == strcmp("applicationSuspend", eventType))
	{
		networkLibrary->onSuspended(luaState);
	}
	else if (0 == strcmp("applicationResume", eventType))
	{
		networkLibrary->onResumed(luaState);
	}
	else if (0 == strcmp("applicationExit", eventType))
	{
		networkLibrary->onExiting(luaState);
	}
	return 0;
}

int		NetworkLibrary::AddSystemEventListener(lua_State *L, NetworkLibrary *networkLibrary)
{
	int ref = LUA_REFNIL;

	// Does the equivalent of the following Lua code:
	//   Runtime:addEventListener( "system", ProcessSystemEvent )
	// which is equivalent to:
	//   local f = Runtime.addEventListener
	//   f( Runtime, "system", ProcessSystemEvent )
	CoronaLuaPushRuntime(L); // push 'Runtime'

	if (lua_type(L, -1) == LUA_TTABLE)
	{
		lua_getfield(L, -1, "addEventListener"); // push 'f', i.e. Runtime.addEventListener
		lua_insert(L, -2); // swap so 'f' is below 'Runtime'
		lua_pushstring(L, "system");

		// Push ProcessSystemEvent as closure so it has access to 'networkLibrary'
		lua_pushlightuserdata(L, networkLibrary); // Assumes 'networkLibrary' lives for lifetime of plugin
		lua_pushcclosure(L, &NetworkLibrary::ProcessSystemEvent, 1);

		// Register reference to C closure so we can use later when we need to remove the listener
		ref = luaL_ref(L, LUA_REGISTRYINDEX);
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

		// Lua stack order (from lowest index to highest):
		// f
		// Runtime
		// "system"
		// ProcessSystemEvent (closure)
		CoronaLuaDoCall(L, 3, 0);
	}
	else
	{
		lua_pop(L, 1); // pop nil
	}
	return ref;
}

int		NetworkLibrary::RemoveSystemEventListener(lua_State *L, int systemEventListenerRef)
{
	// Does the equivalent of the following Lua code:
	//   Runtime:removeEventListener( "system", ProcessSystemEvent )
	// which is equivalent to:
	//   local f = Runtime.removeEventListener
	//   f( Runtime, "system", ProcessSystemEvent )
	CoronaLuaPushRuntime(L); // push 'Runtime'

	if (lua_type(L, -1) == LUA_TTABLE)
	{
		lua_getfield(L, -1, "removeEventListener"); // push 'f', i.e. Runtime.removeEventListener
		lua_insert(L, -2); // swap so 'f' is below 'Runtime'
		lua_pushstring(L, "system");

		// Push reference to the C closure that was used in "addEventListener"
		lua_rawgeti(L, LUA_REGISTRYINDEX, systemEventListenerRef);

		// Lua stack order (from lowest index to highest):
		// f
		// Runtime
		// "system"
		// ProcessSystemEvent (closure)
		CoronaLuaDoCall(L, 3, 0);

		luaL_unref(L, LUA_REGISTRYINDEX, systemEventListenerRef);
	}
	else
	{
		lua_pop(L, 1); // pop nil
	}
	return LUA_REFNIL;
}

// ----------------------------------------------------------------------------

} // namespace Corona

// ----------------------------------------------------------------------------

CORONA_EXPORT int luaopen_network(lua_State *L)
{
	return Corona::NetworkLibrary::Open(L);
}
