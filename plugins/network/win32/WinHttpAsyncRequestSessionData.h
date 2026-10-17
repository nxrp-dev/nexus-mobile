//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// With contributions from Dianchu Technology
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _WinHttpAsyncRequestSessionData_H_
#define _WinHttpAsyncRequestSessionData_H_

#include "WinHttpRequestError.h"
#include <WinHttp.h>

#include "WindowsNetworkSupport.h"

#define SESSION_TX_BUFFER_SIZE 262144
#define SESSION_RX_BUFFER_SIZE 262144 // Orignal 8192 was recommended by Microsoft's WinHttp documentation, has 4.4Mbps download speed at maximum.

/// Stores information needed by a threaded HTTP request.
/// Provides fields to be monitored by the main thread to control the async operation.
struct WinHttpAsyncRequestSessionData
{
	/// The handle returned by the WinHttpOpen() function.
	HINTERNET SessionHandle;

	/// The handle returned by the WinHttpConnect() function.
	HINTERNET ConnectionHandle;

	/// The handle returned by the WinHttpOpenRequest() function.
	HINTERNET RequestHandle;

	/// Signal indicating that WinHttp is done with all resources associated
	/// with this data structure.
	bool RequestComplete;

	bool IsFirstProcessingPassForRequest;

	Body* RequestBody;

	/// File stream to an open file containing the request body to upload (if any)
	FILE* UploadFileStream;

	DWORD RequestBodyBytesCurrent;   // Number of request body bytes sent by the sending thread
	DWORD RequestBodyBytesProcessed; // Number of request body bytes processed by the monitoring thread 
	DWORD RequestBodyBytesTotal;     // Total number of request body bytes to be sent

	// UTFString to collect response headers, and flag indicating that headers
	// have been received and may be read.
	UTF8String ResponseHeaders;
	bool ResponseHeadersReady;

	/// Buffer used by the thread to copy response data to.
	/// The main thread is expected to copy these bytes to its own buffer.
	char ReceiveBuffer[SESSION_RX_BUFFER_SIZE];

	/// The number of bytes received and copied into "ReceiveBuffer".
	/// To be used by the main thread to copy the received bytes to its own buffer.
	/// The main thread is expected to set this field to zero after copying the received bytes.
	int ReceivedByteCount;

	/// The HTTP status code that was received in the HTTP response's header.
	/// Set to -1 if a response has not been received.
	int ReceivedStatusCode;

	/// Set true to have the async operation aborted. This flag is monitored by the threaded
	/// HTTP request operation and will abort when it is able to.
	bool WasAbortRequested;

	/// Set true by the threaded operation when the request operation has ended.
	bool HasAsyncOperationEnded;

	/// Set to true by the processing thread to indicate that HasAsyncOperationEnded has been
	/// processed.
	bool EndOfOperationProcessed;

	/// Indicates if an error has occurred by the end of the async operation.
	/// This field should be ignored by the main thread until "HasAsyncOperationEnded" has been set true.
	WinHttpRequestError ErrorResult;


	/// Initializes this session object for a new asynchronous HTTP request operation.
	/// Never call this function if it is currently being used by an active async operation.
	void Reset()
	{
		RequestComplete = false;
		IsFirstProcessingPassForRequest = true;
		RequestBodyBytesCurrent = 0;
		RequestBodyBytesProcessed = 0;
		RequestBodyBytesTotal = 0;
		ResponseHeaders.empty();
		ResponseHeadersReady = false;
		ReceivedByteCount = 0;
		ReceivedStatusCode = -1;
		WasAbortRequested = false;
		HasAsyncOperationEnded = false;
		EndOfOperationProcessed = false;
		ErrorResult = kWinHttpRequestErrorNone;
	}

	/// Creates a new session object for an asynchronous HTTP request operation.
	WinHttpAsyncRequestSessionData()
	{
		SessionHandle = NULL;
		ConnectionHandle = NULL;
		RequestHandle = NULL;

		RequestBody = NULL;
		UploadFileStream = NULL;

		Reset();
	}
};

#endif
