//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////
#include "WinInetConnectivity.h"

#include "WindowsNetworkSupport.h"

#include "wininet.h"

DWORD WinInetConnectivity::getConnectedState()
{
	DWORD connectionFlags = 0;
	BOOL isConnected = InternetGetConnectedState( &connectionFlags, 0 );

	debug("InternetGetConnectedState - isConnected: %s, dwFlags: %u", isConnected ? "true" : "false", connectionFlags);

	return connectionFlags;
}

