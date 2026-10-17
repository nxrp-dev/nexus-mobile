//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __WinInetConnectivity_H__
#define __WinInetConnectivity_H__

#include <windows.h>

// Local system has a valid connection to the Internet, but it might or might not be currently connected.
#define INET_CONNECTION_CONFIGURED 0x40

// Local system uses a local area network to connect to the Internet.
#define INET_CONNECTION_LAN 0x02

// Local system uses a modem to connect to the Internet.
#define INET_CONNECTION_MODEM 0x01

// Local system is in offline mode.
#define INET_CONNECTION_OFFLINE 0x20

// Local system uses a proxy server to connect to the Internet. 
#define INET_CONNECTION_PROXY 0x04

// ----------------------------------------------------------------------------

class WinInetConnectivity
{
	public:
		static DWORD getConnectedState();
};

// ----------------------------------------------------------------------------

#endif // __WinTimer_H__
