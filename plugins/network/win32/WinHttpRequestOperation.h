//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _WinHttpRequestOperation_H_
#define _WinHttpRequestOperation_H_

#include "CoronaLua.h"

#include <windows.h>
#include <WinHttp.h>
#include "WinHttpAsyncRequestSessionData.h"

#include "WinHttpRequestError.h"

#include "WindowsNetworkSupport.h"


/// Class used to send an HTTP request to a server and wait for a response asynchronously.
///
class WinHttpRequestOperation
{
public:
	WinHttpRequestOperation();
	virtual ~WinHttpRequestOperation();

	RequestCanceller* ExecuteRequest( NetworkRequestParameters *requestParams, const std::shared_ptr<WinHttpRequestOperation>& thiz);
	bool IsExecuting();
	void ProcessExecution();
	void RequestAbort();

private:
	/// Stores data needed to perform an asynchronous HTTP request operation.
	/// This object's fields are changed on another thread.
	WinHttpAsyncRequestSessionData fAsyncSession;
	NetworkRequestParameters* fRequestParams;
	NetworkRequestState* fRequestState;

	/// Temp file path and file stream to open temp file that serves as the destination of the
	/// response body, if response body is directed to a file.
	UTF8String fTempDownloadFilePath;
	FILE* fDownloadFileStream;

	/// Set true if this object is in the middle of an HTTP request operation.
	bool fIsExecuting;

	bool Execute();
	void ProcessExecutionUntil(int timeoutInMilliseconds);

	static WinHttpRequestError GetRequestErrorFromWinHttpError(DWORD dwError);
	static UTF8String* WinHttpRequestOperation::GetMessageFromWinHttpError(WinHttpRequestError error);
	static void CALLBACK OnAsyncWinHttpStatusChanged(
				HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
				LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
	static wchar_t* CreateUtf16StringFrom(const char* utf8String);
	static void DestroyUtf16String(wchar_t *utf16String);
};

#endif
