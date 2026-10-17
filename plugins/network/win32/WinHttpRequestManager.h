//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "CoronaLua.h"

#ifndef _WinHttpRequestManager_H_
#define _WinHttpRequestManager_H_

#include "WinTimer.h"

#include "WinHttpRequestOperation.h"

#include "WindowsNetworkSupport.h"

#include <list>
#include <memory>


/// Class supporting concurrent asynchronous HTTP requests.
/// Can set up a LuaResource listener to notify a Lua script the result of this operation.
class WinHttpRequestManager : public WinTimer
{
public:
	WinHttpRequestManager();
	virtual ~WinHttpRequestManager();

	RequestCanceller* SendNetworkRequest( NetworkRequestParameters *requestParams );

	int ActiveRequestCount();
	void ProcessRequests();
	void ProcessRequestsUntil(int timeoutInMilliseconds);
	void AbortAllRequests();
	void OnTimer( );

private:
	/// Typedef for a WinHttpRequestOperation STL list.
	typedef std::list< std::shared_ptr<WinHttpRequestOperation> > WinHttpRequestOperationList;

	/// Collection of HTTP request operations.
	WinHttpRequestOperationList fRequests;

	/// Collection used to temporarily store all requests to be processed by the ProcessRequests() function.
	/// This member variable is to only be used by the ProcessRequests() function.
	WinHttpRequestOperationList fTemporaryRequestList;

	/// Set true if in the middle of processing requests.
	bool fIsProcessingRequests;
};

#endif
