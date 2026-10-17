//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _NetworkLibrary_H__
#define _NetworkLibrary_H__

#include "Corona/CoronaLua.h"
#include "Corona/CoronaMacros.h"
#include "Rtt_EmscriptenContainer.h"
#include "EmscriptenNetworkSupport.h"

// This corresponds to the name of the library, e.g. [Lua] require "plugin.library"
// where the '.' is replaced with '_'
CORONA_EXPORT int luaopen_network(lua_State *L);

namespace Corona
{

	class NetworkLibrary
	{
	public:
		typedef NetworkLibrary Self;

		static const char kName[];
		static const char kEvent[];

		int fSystemEventListener;

		NetworkLibrary();
		~NetworkLibrary();

		static int Open(lua_State *L);
		static int Finalizer(lua_State *L);
		static Self *ToLibrary(lua_State *L);
		static int ValueForKey(lua_State *L);
		static int request(lua_State *L);
		static int cancel(lua_State *L);
		static int getConnectionStatus(lua_State *L);
		static int ProcessSystemEvent(lua_State *L);
		static int AddSystemEventListener(lua_State *L, NetworkLibrary *networkLibrary);
		static int RemoveSystemEventListener(lua_State *L, int systemEventListenerRef);

		void onStarted(lua_State *L);
		void onSuspended(lua_State *L);
		void onResumed(lua_State *L);
		void onExiting(lua_State *L);
	};

#endif // _NetworkLibrary_H__
