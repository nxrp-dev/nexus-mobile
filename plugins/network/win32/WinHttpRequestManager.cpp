//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "CoronaLua.h"

#include "WinHttpRequestManager.h"
#include "WinHttpRequestOperation.h"


#pragma region Constructors and Destructors
/// Creates a new manager object for handling concurrent async HTTP requests.
WinHttpRequestManager::WinHttpRequestManager()
{
	fIsProcessingRequests = false;
}

/// Destructor. Destroys this object and aborts any active HTTP requests.
WinHttpRequestManager::~WinHttpRequestManager()
{
}

#pragma endregion


#pragma region Public Functions

RequestCanceller* WinHttpRequestManager::SendNetworkRequest( NetworkRequestParameters *requestParams )
{
	WinHttpRequestOperationList::iterator iter;
	std::shared_ptr<WinHttpRequestOperation> requestPointer;

	// Check if this object's list contains any inactive request objects that we can re-use.
	// Each request object is at least 8 KB (due to its receive buffer). So, try to be memory efficient.
	for (iter = fRequests.begin(); iter != fRequests.end(); iter++)
	{
		if ((*iter)->IsExecuting() == false)
		{
			*iter = std::make_shared<WinHttpRequestOperation>();
			requestPointer = *iter;
			break;
		}
	}

	// If there are no request objects that we can re-use, then create a new one and add it to the list.
	if (NULL == requestPointer)
	{
		fRequests.push_back(std::make_shared<WinHttpRequestOperation>());
		requestPointer = fRequests.back();
	}

	// Execute HTTP request.
	return requestPointer->ExecuteRequest( requestParams, requestPointer );
}

/// Gets the number of concurrent HTTP requests that are currently being executed by this object.
/// @return The number of HTTP requests being exected. Returns zero if there are no active requests.
int WinHttpRequestManager::ActiveRequestCount()
{
	WinHttpRequestOperationList::iterator iter;
	int count = 0;

	for (iter = fRequests.begin(); iter != fRequests.end(); iter++)
	{
		if ((*iter)->IsExecuting())
		{
			count++;
		}
	}
	return count;
}

/// Polls all active HTTP requests to see if they have completed their work.
/// This function is expected to be called at regular intervals). It polls every asynchronous
/// HTTP request, synchs their data to the main thread, checks if request operation have completed,
/// and invokes LuaResource listeners if assigned.
void WinHttpRequestManager::ProcessRequests()
{
	WinHttpRequestOperationList::iterator iter;
	int index;

	// Do not continue if this function is in the middle of processing requests.
	// This can happen if a processed request has ended whose Lua listener calls this function again.
	if (fIsProcessingRequests)
	{
		return;
	}

	// Flag that we're processing requests.
	fIsProcessingRequests = true;

	// Copy requests to be processed to a temporary list. This is to prevent a race condition where
	// a processed requests that finishes, invokes a Lua listener, and which then attempts to send
	// another HTTP request won't break the STL list's iterator.
	for (iter = fRequests.begin(), index = 0; iter != fRequests.end(); iter++, index++)
	{
		if (index < (int)fTemporaryRequestList.size())
		{
			WinHttpRequestOperationList::iterator tempIter;
			int tempIndex;
			for (tempIter = fTemporaryRequestList.begin(), tempIndex = 0;
				 tempIter != fTemporaryRequestList.end(); tempIter++, tempIndex++)
			{
				if (tempIndex == index)
				{
					*tempIter = *iter;
					break;
				}
			}
		}
		else
		{
			fTemporaryRequestList.push_back(*iter);
		}
	}
	while (fTemporaryRequestList.size() > fRequests.size())
	{
		fTemporaryRequestList.pop_back();
	}

	// Process all requests that were copied into the temporary list. This makes it race condition proof.
	for (iter = fTemporaryRequestList.begin(); iter != fTemporaryRequestList.end(); iter++)
	{
		if (*iter != NULL)
		{
			(*iter)->ProcessExecution();
		}
	}

	// Finished processing requests. Clearing this flag allows this function to be called again.
	fIsProcessingRequests = false;
}

/// Blocking call which polls all active HTTP requests to see if they have completed their work.
/// This function is expected to be called at regular intervals). It polls every asynchronous
/// HTTP request, synchs their data to the main thread, checks if request operation have completed,
/// and invokes LuaResource listeners if assigned.
/// @param timeoutInMilliseconds The maximum amount of time to process all active HTTP requests.
void WinHttpRequestManager::ProcessRequestsUntil(int timeoutInMilliseconds)
{
	int endTime = (int)::GetTickCount() + timeoutInMilliseconds;
	do
	{
		ProcessRequests();
	} while (((endTime - (int)::GetTickCount()) > 0) && (ActiveRequestCount() > 0));
}

/// Aborts all active HTTP requests.
/// This is a non-blocking call and HTTP requests will not be aborted immediately. You must still
/// call the ProcessRequests() function repeatedly to process the abort.
void WinHttpRequestManager::AbortAllRequests()
{
	WinHttpRequestOperationList::iterator iter;

	for (iter = fRequests.begin(); iter != fRequests.end(); iter++)
	{
		(*iter)->RequestAbort();
	}
}

void WinHttpRequestManager::OnTimer()
{
	ProcessRequests();
}

#pragma endregion
