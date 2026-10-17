//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "CoronaLog.h"
#include "CoronaLua.h"
#include "WinHttpRequestOperation.h"
#include "WindowsNetworkSupport.h"
#include "CharsetTranscoder.h"
#include <Shlobj.h>


#pragma region Constructors and Destructors
/// Creates a new HTTP request operation object.
WinHttpRequestOperation::WinHttpRequestOperation()
{
	fDownloadFileStream = NULL;
	fIsExecuting = false;
	fAsyncSession.Reset();
}

/// Destroys the HTTP request operation object. If this object is currently executing a request
/// operation, then this destructor will block while attempting to abort that operation.
WinHttpRequestOperation::~WinHttpRequestOperation()
{
	// If currently executing a request, then abort it. Block for up to 5 seconds until abort completes.
	// We need to do this to cleanup after the threaded WinHttp operation.
	if (IsExecuting())
	{
		RequestAbort();
		ProcessExecutionUntil(5000);
	}

	// Close the WinHttp session (if ever established) and remove the callback.
	// Only do this if we're not executing an async operation, otherwise we'll crash.
	// Hopefully, the above abort will stop a pending async operation.
	if (!IsExecuting() && fAsyncSession.SessionHandle)
	{
		::WinHttpSetStatusCallback(fAsyncSession.SessionHandle, NULL, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, NULL);
		::WinHttpCloseHandle(fAsyncSession.SessionHandle);
		fAsyncSession.SessionHandle = 0;
	}
}

#pragma endregion


#pragma region Execution Functions
/// Starts the HTTP request operation.  This method will return true if successful, otherwise false.  Further
/// asynchronous processing will be required to complete the operation.  You are expected to poll the ProcessExecution()
/// function at regular intervals until this object flags that it is finished.
///
bool WinHttpRequestOperation::Execute()
{
	// Do not continue if this object is already executing an HTTP request operation.
	if (IsExecuting())
	{
		return false;
	}

	// Initialize variables for a new HTTP request.
	fIsExecuting = true;

	// Get method...
	const WCHAR* wideMethod = getWCHARs(fRequestParams->getRequestMethod());
	std::wstring method = std::wstring(wideMethod);
	delete [] wideMethod;

	// Get URL params...
	const WCHAR* wideUrl = getWCHARs(fRequestParams->getRequestUrl());
	URL_COMPONENTS urlInfo;
	memset(&urlInfo, 0, sizeof(urlInfo));
	urlInfo.dwStructSize = sizeof(urlInfo);
    urlInfo.dwHostNameLength = (DWORD)-1;
	urlInfo.dwUrlPathLength = (DWORD)-1;
	urlInfo.dwSchemeLength = (DWORD)-1;
	urlInfo.dwUserNameLength = (DWORD)-1;
	urlInfo.dwPasswordLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wideUrl, 0, 0, &urlInfo))
    {
		CORONA_LOG("Failure cracking URL - %s", fRequestParams->getRequestUrl().c_str());
		fAsyncSession.ErrorResult = kWinHttpRequestErrorInvalidUrl;
		fAsyncSession.HasAsyncOperationEnded = true;
        return false;
    }

	std::wstring hostName = std::wstring(urlInfo.lpszHostName, urlInfo.dwHostNameLength);
	std::wstring urlPath = std::wstring(urlInfo.lpszUrlPath, urlInfo.dwUrlPathLength); 
	INTERNET_PORT port = urlInfo.nPort;
	bool isHttps = (INTERNET_SCHEME_HTTPS == urlInfo.nScheme);

	std::wstring username;
	std::wstring password;
	if (urlInfo.dwUserNameLength > 0)
	{
		username = std::wstring(urlInfo.lpszUserName, urlInfo.dwUserNameLength);
	}
	if (urlInfo.dwPasswordLength > 0)
	{
		password = std::wstring(urlInfo.lpszPassword, urlInfo.dwPasswordLength);
	}

	delete [] wideUrl;

	// Create the WinHttp session handle if not done already.
	// This session handle gets re-used on every call to Execute(). It is closed by the destructor.
	if (0 == fAsyncSession.SessionHandle)
	{
		fAsyncSession.SessionHandle = ::WinHttpOpen(
			NULL, 
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 
			WINHTTP_FLAG_ASYNC
			);
		if (fAsyncSession.SessionHandle)
		{
			::WinHttpSetStatusCallback(
				fAsyncSession.SessionHandle,
				WinHttpRequestOperation::OnAsyncWinHttpStatusChanged,
				WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 
				NULL
				);
		}
	}
	if (0 == fAsyncSession.SessionHandle)
	{
		// Unable to create a WinHttp session. Flag it as an internal error and give up.
		fAsyncSession.ErrorResult = kWinHttpRequestErrorInternal;
		fAsyncSession.HasAsyncOperationEnded = true;
		return false;
	}

	int timeoutMs = fRequestParams->getTimeout() * 1000;
	if (!WinHttpSetTimeouts( fAsyncSession.SessionHandle, timeoutMs, timeoutMs, timeoutMs, timeoutMs ))
	{
		CORONA_LOG("Error setting WinHttp timeouts to %u ms", timeoutMs);
	}

	// Configure the connection to the server (provided by the URL).
	// This does not actually establish a socket connection.
	fAsyncSession.ConnectionHandle = ::WinHttpConnect(
		fAsyncSession.SessionHandle,
		hostName.c_str(),
		port, 
		0
		);
	if (NULL == fAsyncSession.ConnectionHandle)
	{
		fAsyncSession.ErrorResult = GetRequestErrorFromWinHttpError(::GetLastError());
		fAsyncSession.HasAsyncOperationEnded = true;
		return false;
	}

	// Configure a request in WinHttp with the URL and HTTP command/verb.
	fAsyncSession.RequestHandle = ::WinHttpOpenRequest(
		fAsyncSession.ConnectionHandle, 
		method.c_str(),
		urlPath.c_str(), 
		NULL,
		WINHTTP_NO_REFERER, 
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		isHttps ? WINHTTP_FLAG_SECURE : 0
		);
	if (NULL == fAsyncSession.RequestHandle)
	{
		fAsyncSession.ErrorResult = GetRequestErrorFromWinHttpError(::GetLastError());
		fAsyncSession.HasAsyncOperationEnded = true;
		return false;
	}

	if (! fRequestParams->getHandleRedirects())
	{
		DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;

		BOOL result = ::WinHttpSetOption(fAsyncSession.RequestHandle, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
		debug("Disabling automatic redirects (%d, %d)", result, ::GetLastError());
	}

	if (WinHttpSetOption(fAsyncSession.RequestHandle, WINHTTP_OPTION_CLIENT_CERT_CONTEXT, WINHTTP_NO_CLIENT_CERT_CONTEXT, 0) == false)
	{
		DWORD result = ::GetLastError();
		debug("Failed to set WINHTTP_NO_CLIENT_CERT_CONTEXT (%d, %d)", result, ::GetLastError());
	}

	// Add basic auth credentials if username/password set in URL
	//
	if ((username.size() > 0) && (password.size() > 0))
	{
		::WinHttpSetCredentials( 
			fAsyncSession.RequestHandle, 
			WINHTTP_AUTH_TARGET_SERVER, 
			WINHTTP_AUTH_SCHEME_BASIC,
			username.c_str(),
			password.c_str(),
			NULL
			);
	}

	// Prepare the request headers and body.
	//
	fAsyncSession.RequestBody = fRequestParams->getRequestBody();

	// See if we have a "Content-Type" header.
	//
	// Note: We check for the presence of a Content-Type request header on param validation whenever a request body
	// is specified, so we don't need worry about adding a default Content-Type header.
	//
	UTF8String *contentTypeValue = fRequestParams->getRequestHeaderValue("Content-Type");
	if (NULL != contentTypeValue)
	{
		if ( TYPE_STRING == fAsyncSession.RequestBody->bodyType )
		{
			// When we have a text string request body, we need to analyze and charset encoding
			// specified in the Content-Type header, and if not utf-8, we need to apply said encoding
			// to the text string.
			//
			char *contentEncoding = getContentTypeEncoding( contentTypeValue->c_str() );
			if ( NULL != contentEncoding )
			{
				debug("Got request content encoding of: %s", contentEncoding);

				if ( 0 != _strcmpi( "utf-8", contentEncoding ) )
				{
					// Found content encoding other than utf-8
					//
					debug("Transcoding request body from utf-8 to %s", contentEncoding);
					if (!CharsetTranscoder::transcode(fAsyncSession.RequestBody->bodyString, "utf-8", contentEncoding))
					{
						debug("Transcode failed");
					}
				}
				free(contentEncoding);
			}
			else
			{
				// No charset provided, adding explicit UTF-8
				contentTypeValue->append("; charset=UTF-8");
			}
		}
	}

	// Prepare the request headers for the request call.
	//
	std::wstring headers;

	const WCHAR* wideHeaders = getWCHARs(fRequestParams->getRequestHeaderString());
	if (NULL != wideHeaders)
	{
		headers = std::wstring(wideHeaders);
		delete [] wideHeaders;
	}

	// If the body is from a file, we need to open it here...
	//
	if ( TYPE_FILE == fAsyncSession.RequestBody->bodyType )
	{
		wchar_t *utf16FullPath = NULL;
		try
		{
			CoronaFileSpec* fileSpec = fAsyncSession.RequestBody->bodyFile;
			utf16FullPath = CreateUtf16StringFrom(fileSpec->getFullPath().c_str());
			::_wfopen_s(&fAsyncSession.UploadFileStream, utf16FullPath, L"rb+");
		}
		catch (...) { }
		DestroyUtf16String(utf16FullPath);
		if (NULL == fAsyncSession.UploadFileStream)
		{
			CORONA_LOG("Error opening request body file");
			fAsyncSession.ErrorResult = kWinHttpRequestErrorInternal;
			fAsyncSession.HasAsyncOperationEnded = true;
			return false;
		}
	}

	// This will fail for files > 4Gb (which requires WinHttp 5.1 anyway).
	fAsyncSession.RequestBodyBytesTotal = (DWORD)fRequestParams->getRequestBodySize();

	debug("Request body size: %u", fAsyncSession.RequestBodyBytesTotal);

	// As long as dwTotalLength is provided in the WinHttpSendRequest call below, the
	// Content-Length header will automatically be added (if not already present) - per 
	// the API documentation.

	// Start the asynchronous request. This is a non-blocking call.
	//
	BOOL wasSuccessful = ::WinHttpSendRequest(
		fAsyncSession.RequestHandle,
		headers.c_str(), 
		-1,
		WINHTTP_NO_REQUEST_DATA,
		0,
		fAsyncSession.RequestBodyBytesTotal,
		(DWORD_PTR)&fAsyncSession
		);
	if (!wasSuccessful)
	{
		fAsyncSession.ErrorResult = GetRequestErrorFromWinHttpError(::GetLastError());
		fAsyncSession.HasAsyncOperationEnded = true;
		return false;
	}

	return true;
}

RequestCanceller* WinHttpRequestOperation::ExecuteRequest( NetworkRequestParameters *requestParams, const std::shared_ptr<WinHttpRequestOperation>& thiz)
{
	fRequestParams = requestParams;
	fRequestState = new NetworkRequestState( thiz, requestParams->getRequestUrl(), requestParams->isDebug() );

	debug("Executing request");
	Execute(); // No need to check for errors, as they will be handled and dispatched asynchronously.

	return fRequestState->getRequestCanceller();
}
 
/// This function is expected to be called at regular intervals after calling Execute(). It polls the
/// thread handling the asynchronous operation, synchs its data to the main thread, and invokes
/// the LuaResource listener if the operation has been detected to be finished.
void WinHttpRequestOperation::ProcessExecution()
{
	// Do not continue if we're not executing an operation.
	if (!IsExecuting())
	{
		return;
	}

	LuaCallback* luaCallback = fRequestParams->getLuaCallback();

	if (fAsyncSession.IsFirstProcessingPassForRequest)
	{
		fAsyncSession.IsFirstProcessingPassForRequest = false;

		if (Upload == fRequestParams->getProgressDirection())
		{
			// If caller specified Upload progress, send it now (before uploading starts).
			//
			fRequestState->setPhase("began");
			fRequestState->setBytesEstimated(fAsyncSession.RequestBodyBytesTotal);
			if (NULL != luaCallback)
			{
				luaCallback->callWithNetworkRequestState( fRequestState );
			}
		}
	}

	// We're going to copy the current request body count since it could change (be overwritten
	// by the WinHttp thread) while we're playing with it.
	DWORD currentBytes = fAsyncSession.RequestBodyBytesCurrent;
	if (currentBytes != fAsyncSession.RequestBodyBytesProcessed)
	{
		// New bytes have been uploaded...
		//
		fAsyncSession.RequestBodyBytesProcessed = currentBytes;

		if (Upload == fRequestParams->getProgressDirection())
		{
			// If caller specified Upload progress, notify them that more bytes have been uploaded...
			//
			debug("Request body written %u of %u bytes", fAsyncSession.RequestBodyBytesProcessed, fAsyncSession.RequestBodyBytesTotal);
			fRequestState->setPhase("progress");
			fRequestState->setBytesTransferred(fAsyncSession.RequestBodyBytesProcessed);
			if (NULL != luaCallback)
			{
				luaCallback->callWithNetworkRequestState( fRequestState );
			}
		}
	}

	if (fAsyncSession.ResponseHeadersReady)
	{
		fRequestState->setStatus( fAsyncSession.ReceivedStatusCode );
		fRequestState->setResponseHeaders(fAsyncSession.ResponseHeaders.c_str());

		long contentLength = -1;
		DWORD defaultContentAllocation = 8192;

		UTF8String contentLengthText = fRequestState->getResponseHeaderValue("Content-Length");
		if (contentLengthText.size() > 0)
		{
			contentLength = atoi(contentLengthText.c_str());
		}

		Body* body = fRequestState->getResponseBody();

		CoronaFileSpec *responseFile = fRequestParams->getResponseFile();
		if ( ( NULL != responseFile ) && ( HTTP_STATUS_OK == fAsyncSession.ReceivedStatusCode ) )
		{
			// Set up the response body...
			//
			body->bodyType = TYPE_FILE;
			body->bodyFile = new CoronaFileSpec( responseFile );


			UTF8String pathDir;
			UTF8String fullPath = responseFile->getFullPath();
			const size_t lastIndex = fullPath.rfind('\\');
			if (std::string::npos != lastIndex)
			{
				pathDir = fullPath.substr(0, lastIndex+1);
				wchar_t *utf16DirectoryPath = CreateUtf16StringFrom(pathDir.c_str());
				::SHCreateDirectoryEx(NULL, utf16DirectoryPath, NULL);
				DestroyUtf16String(utf16DirectoryPath);
			}
			
			fTempDownloadFilePath = pathForTemporaryFileWithPrefix("download", pathDir);
			debug("Temp file path: %s", fTempDownloadFilePath.c_str());

			// Create/open the download temp file.
			wchar_t *utf16FilePath = CreateUtf16StringFrom(fTempDownloadFilePath.c_str());
			try
			{
				fDownloadFileStream = NULL;
				::_wfopen_s(&fDownloadFileStream, utf16FilePath, L"wb+");
			}
			catch (...) { }
			DestroyUtf16String(utf16FilePath);
			if ( NULL == fDownloadFileStream )
			{
				CORONA_LOG("Error creating temp file for download");
				fAsyncSession.ErrorResult = kWinHttpRequestErrorInternal;
				fAsyncSession.HasAsyncOperationEnded = true;
			}
		}
		else
		{
			// Now that we have the response headers, we can determine if the response body will be text
			// or binary, and set it up accordingly so that it can collect response packets appropriately.
			//
			char *contentType = NULL;
			char *contentEncoding = NULL;

			UTF8String contentTypeHeader = fRequestState->getResponseHeaderValue("Content-Type");
			if (contentTypeHeader.length() > 0)
			{
				contentType = getContentType( contentTypeHeader.c_str() );
				contentEncoding = getContentTypeEncoding( contentTypeHeader.c_str() );
			}
			if ( ( NULL != contentEncoding ) || ( ( NULL != contentType ) && isContentTypeText( contentType ) ) )
			{
				// If the Content-Type has a charset, or if it is "texty", then let's treat it as text
				debug("treating content as text");
				body->bodyType = TYPE_STRING;
				body->bodyString = new UTF8String( );
				body->bodyString->reserve( contentLength > 0 ? contentLength : defaultContentAllocation );
			}
			else
			{
				debug("treating content as binary");
				body->bodyType = TYPE_BYTES;
				body->bodyBytes = new ByteVector( );
				body->bodyBytes->reserve(  contentLength > 0 ? contentLength : defaultContentAllocation  );
			}

			if ( NULL != contentType )
			{
				free(contentType);
			}
			if ( NULL != contentEncoding )
			{
				free(contentEncoding);
			}
		}

		if (Upload != fRequestParams->getProgressDirection())
		{
			// If caller specified Download or no progress, we will populate the estimated bytes with the
			// response content length.
			//
			fRequestState->setBytesEstimated( contentLength );
		}

		if (Download == fRequestParams->getProgressDirection())
		{
			// If caller specified Download progress, send began progress (now that we may know the 
			// response size).
			//
			fRequestState->setPhase("began");
			if (NULL != luaCallback)
			{
				luaCallback->callWithNetworkRequestState( fRequestState );
			}
		}

		// Clear headers buffer and signal (so we won't process them again)
		fAsyncSession.ResponseHeaders.clear();
		fAsyncSession.ResponseHeadersReady = false;
	}

	// If data has been received by the thread, then append it to the result buffer or file.
	//
	if (fAsyncSession.ReceivedByteCount > 0)
	{
		debug("Got %u bytes", fAsyncSession.ReceivedByteCount);
		Body* body = fRequestState->getResponseBody();
		switch (body->bodyType)
		{
			case TYPE_FILE:
				if (fDownloadFileStream)
				{
					size_t bytesWritten = 0;
					try
					{
						bytesWritten = ::fwrite(
							fAsyncSession.ReceiveBuffer, 
							sizeof(fAsyncSession.ReceiveBuffer[0]),
							(size_t)fAsyncSession.ReceivedByteCount, 
							fDownloadFileStream
							);
					}
					catch (...) { }
				}
				else
				{
					CORONA_LOG("Downloading file bytes, but no open file stream");
				}
				break;

			case TYPE_STRING:
				body->bodyString->append(fAsyncSession.ReceiveBuffer, fAsyncSession.ReceivedByteCount);
				break;

			case TYPE_BYTES:
				body->bodyBytes->insert(
					body->bodyBytes->end(), 
					(unsigned char *)fAsyncSession.ReceiveBuffer, 
					(unsigned char *)fAsyncSession.ReceiveBuffer + fAsyncSession.ReceivedByteCount
					);
				break;
		}

		if (Upload != fRequestParams->getProgressDirection())
		{
			fRequestState->incrementBytesTransferred(fAsyncSession.ReceivedByteCount);
		}

		if (Download == fRequestParams->getProgressDirection())
		{
			// If caller specified Download progress, notify them that more bytes have been downloaded...
			//
			debug("Response data received: %u bytes", fAsyncSession.ReceivedByteCount);
			fRequestState->setPhase("progress");
			if (NULL != luaCallback)
			{
				luaCallback->callWithNetworkRequestState( fRequestState );
			}
		}

		// Signal the WinHttp thread that we're ready for more data
		//
		fAsyncSession.ReceivedByteCount = 0; 

		BOOL wasSuccessful = ::WinHttpReadData(
			fAsyncSession.RequestHandle,
			fAsyncSession.ReceiveBuffer,
			sizeof(fAsyncSession.ReceiveBuffer), 
			NULL
			);
		if (FALSE == wasSuccessful)
		{
			debug("Failed to post request for more response data");
			fAsyncSession.ErrorResult = kWinHttpRequestErrorUnknown;
			fAsyncSession.HasAsyncOperationEnded = true;
		}
	}

	// If the async operation has been flagged to end, then close the WinHttp session.
	if (fAsyncSession.HasAsyncOperationEnded && !fAsyncSession.EndOfOperationProcessed)
	{
		fAsyncSession.EndOfOperationProcessed = true;

		debug("Request operation has ended, processing...");

		// Close the WinHttp request and connection, if not done already by an abort.
		//
		HINTERNET requestHandle = fAsyncSession.RequestHandle;
		fAsyncSession.RequestHandle = 0;
		if (requestHandle)
		{
			debug("Closing request handle (end of data)");
			::WinHttpCloseHandle(requestHandle);
		}

		HINTERNET connectionHandle = fAsyncSession.ConnectionHandle;
		fAsyncSession.ConnectionHandle = 0;
		if (connectionHandle)
		{
			debug("Closing connection handle (end of data)");
			::WinHttpCloseHandle(connectionHandle);
		}

		// Propagate the async session's error state or status code to the result state object.
		if ( ( kWinHttpRequestErrorNone != fAsyncSession.ErrorResult ) || fAsyncSession.WasAbortRequested )
		{
			fRequestState->setError(GetMessageFromWinHttpError(fAsyncSession.ErrorResult));


			// Close any open files, delete any temp files
			//
			if (fAsyncSession.UploadFileStream)
			{
				// Uploading from file - close file.
				try
				{
					::fclose(fAsyncSession.UploadFileStream);
				}
				catch (...) { }
				fAsyncSession.UploadFileStream = NULL;
			}

			if (fDownloadFileStream)
			{
				// Downloading to file - close file.
				try
				{
					::fclose(fDownloadFileStream);
				}
				catch (...) { }
				fDownloadFileStream = NULL;
			}

			if (fTempDownloadFilePath.size() > 0)
			{
				// Delete temp file...
				wchar_t *utf16TempFilePath = CreateUtf16StringFrom(fTempDownloadFilePath.c_str());
				if ( DeleteFileW( utf16TempFilePath ) )
				{
					debug("Successfully deleted temp file");
					fTempDownloadFilePath.clear();
				}
				else
				{
					CORONA_LOG("Error deleting temp file");
				}
				DestroyUtf16String(utf16TempFilePath);
			}
		}
		else
		{
			// Success!
			//
			fRequestState->setStatus( fAsyncSession.ReceivedStatusCode );

			// Body download complete, do any required post-processing...
			//
			Body* body = fRequestState->getResponseBody();
			switch (body->bodyType)
			{
				case TYPE_FILE:
				{
					if (fDownloadFileStream)
					{
						// Downloading to file - close file.
						try
						{
							::fclose(fDownloadFileStream);
						}
						catch (...) { }
						fDownloadFileStream = NULL;

						// Rename temp file to final file (with overwrite)
						wchar_t *utf16SourceFilePath = CreateUtf16StringFrom(fTempDownloadFilePath.c_str());
						wchar_t *utf16TargetFilePath = CreateUtf16StringFrom(body->bodyFile->getFullPath().c_str());
						if (MoveFileExW( utf16SourceFilePath, utf16TargetFilePath, MOVEFILE_REPLACE_EXISTING ))
						{
							debug("File successfully renamed");
							fTempDownloadFilePath.clear();
						}
						else
						{
							if (DeleteFileW( utf16SourceFilePath ))
							{
								CORONA_LOG("Failed to rename temp download file to final download file");
							}
							else
							{
								CORONA_LOG("Failed to rename temp download file to final download file; failed to clean temp download");
							}
						}
						DestroyUtf16String(utf16SourceFilePath);
						DestroyUtf16String(utf16TargetFilePath);
					}
					else
					{
						CORONA_LOG("Download to file complete, but no open file stream");
					}
				}
				break;

				case TYPE_STRING:
				{
					// Decode text string response content based on charset.  Default encoding is
					// assumed to be utf-8, so if no charset is specified, or if it is specified
					// and equal to utf-8, we take no action.
					//
					UTF8String contentType = fRequestState->getResponseHeaderValue("Content-Type");
					Body *responseBody = fRequestState->getResponseBody();
					char *contentEncoding = getContentTypeEncoding( contentType.c_str() );
					if ( NULL != contentEncoding )
					{
						debug("Charset from protocol: %s", contentEncoding);
						fRequestState->setDebugValue("charset", contentEncoding);
						fRequestState->setDebugValue("charsetSource", "protocol");
					}
					else 
					{
						contentEncoding = getEncodingFromContent( contentType.c_str(), responseBody->bodyString->c_str() );
						if ( NULL != contentEncoding )
						{
							debug("Charset from content: %s", contentEncoding);
							fRequestState->setDebugValue("charset", contentEncoding);
							fRequestState->setDebugValue("charsetSource", "content");
						}
						else
						{
							debug("Charset implicit (text default): utf-8");
							fRequestState->setDebugValue("charset", "utf-8");
							fRequestState->setDebugValue("charsetSource", "implicit");
						}
					}

					if ( NULL != contentEncoding )
					{
						debug("Got response content encoding of: %s", contentEncoding);

						if ( 0 != _strcmpi( "utf-8", contentEncoding ) )
						{
							// Found content encoding other than utf-8
							//
							debug("Transcoding response body from %s to utf-8", contentEncoding);
							if (!CharsetTranscoder::transcode(responseBody->bodyString, contentEncoding, "utf-8"))
							{
								debug("Transcode failed");
							}
						}
						free(contentEncoding);
					}
				}
				break;
			}
		}

		if (NULL != luaCallback)
		{
			// Send the final callback notification (unless the request was cancelled)
			//
			if (!fAsyncSession.WasAbortRequested) // kWinHttpRequestErrorAborted
			{
				fRequestState->setPhase("ended");
				luaCallback->callWithNetworkRequestState( fRequestState );
			}

			luaCallback->unregister();
		}

		debug("Request operaton processing complete");
	}

	if (fAsyncSession.RequestComplete)
	{
		// Release resources...
		//
		debug("Releasing request operation resources");
		delete fRequestParams;
		fRequestParams = NULL;
		delete fRequestState;
		fRequestState = NULL;
		fAsyncSession.Reset();
			
		// Flag that execution has ended (puts this object back into the pool).
		//
		fIsExecuting = false;
	}
}

/// Blocking call which processes the currently executed operation until it has ended or
/// the given timeout has been reached.
/// @param timeoutInMilliseconds The maximum amount of time to process the currently active operation.
void WinHttpRequestOperation::ProcessExecutionUntil(int timeoutInMilliseconds)
{
	// First, process execution before entering the below loop.
	// This is because the operation may be ready to end now, making the Sleep() function below unnecessary.
	ProcessExecution();

	// Process execution until the operation has ended or the given timeout has been reached.
	int endTime = (int)::GetTickCount() + timeoutInMilliseconds;
	while (fIsExecuting && ((endTime - (int)::GetTickCount()) > 0))
	{
		ProcessExecution();
		::Sleep(10);
	}
}

/// Determines if this object is in the middle of an HTTP request operation.
/// @return Returns true if currently executing an HTTP request operation. Returns false if not.
bool WinHttpRequestOperation::IsExecuting()
{
	return fIsExecuting;
}

/// Request to have the currently active HTTP request operation be aborted.
/// The abort will not happen immediately since an HTTP request is executed asynchronously.
/// You must poll the IsExecuting() function to detect when the abort has occurred.
void WinHttpRequestOperation::RequestAbort()
{
	// Do not continue if we're not currently executing an operation. Nothing to abort.
	if (!IsExecuting())
	{
		return;
	}

	// Flag that the current operation was aborted.
	//
	fAsyncSession.ErrorResult = kWinHttpRequestErrorAborted;
	fAsyncSession.WasAbortRequested = true;
	fAsyncSession.HasAsyncOperationEnded = true;

	// Close the WinHttp request and connection.
	//
	HINTERNET requestHandle = fAsyncSession.RequestHandle;
	fAsyncSession.RequestHandle = 0;
	if (requestHandle)
	{
		debug("Closing request handle (request abort)");
		::WinHttpCloseHandle(requestHandle);
	}

	HINTERNET connectionHandle = fAsyncSession.ConnectionHandle;
	fAsyncSession.ConnectionHandle = 0;
	if (connectionHandle)
	{
		debug("Closing connection handle (request abort)");
		::WinHttpCloseHandle(connectionHandle);
	}
}

#pragma endregion


#pragma region WinHttp Functions
/// Converts the given WinHttp error value taken from GetLastError() to a WinHttpRequestError value.
/// @param errorValue Error value received from Window's GetLastError() function after calling
///                   a WinHttp function.
/// @return Returns a WinHttpRequestError enum value matching the WinHttp error value.
WinHttpRequestError WinHttpRequestOperation::GetRequestErrorFromWinHttpError(DWORD errorValue)
{
	switch (errorValue)
	{
		case ERROR_WINHTTP_TIMEOUT:
			debug("WinHttp error ERROR_WINHTTP_TIMEOUT");
			return kWinHttpRequestErrorTimedOut;
		case ERROR_WINHTTP_INVALID_URL:
			debug("WinHttp error ERROR_WINHTTP_INVALID_URL");
			return kWinHttpRequestErrorInvalidUrl;
		case ERROR_WINHTTP_OPERATION_CANCELLED:
			debug("WinHttp error ERROR_WINHTTP_OPERATION_CANCELLED");
			return kWinHttpRequestErrorAborted;
		case ERROR_WINHTTP_CANNOT_CONNECT:
			debug("WinHttp error ERROR_WINHTTP_CANNOT_CONNECT");
			return kWinHttpRequestErrorConnectionFailure;
		case ERROR_WINHTTP_CONNECTION_ERROR:
			debug("WinHttp error ERROR_WINHTTP_CONNECTION_ERROR");
			return kWinHttpRequestErrorConnectionFailure;
		case ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED:
			debug("WinHttp error ERROR_WINHTTP_CLIENT_AUTH_CERT_NEEDED");
			return kWinHttpRequestErrorCertificateRequired;
		case ERROR_WINHTTP_LOGIN_FAILURE:
			debug("WinHttp error ERROR_WINHTTP_LOGIN_FAILURE");
			return kWinHttpRequestErrorLoginFailure;
		default:
			debug("WinHttp error (unknown): %u", errorValue);
	}
	return kWinHttpRequestErrorInternal;
}

// Provides a human-readable error message from a WinHttpRequestOperation error code.
// N.B. - The caller is responsible for cleaning the memory of the returned UTF8String.
UTF8String* WinHttpRequestOperation::GetMessageFromWinHttpError(WinHttpRequestError errorCode)
{
	UTF8String* errorMessage = NULL;

	switch (errorCode)
	{
		case kWinHttpRequestErrorTimedOut:
			errorMessage = new UTF8String("Timed out");
			break;
		case kWinHttpRequestErrorInvalidUrl:
			errorMessage = new UTF8String("Invalid URL");
			break;
		case kWinHttpRequestErrorAborted:
			errorMessage = new UTF8String("Connection aborted");
			break;
		case kWinHttpRequestErrorConnectionFailure:
			errorMessage = new UTF8String("Connection failure");
			break;
		case kWinHttpRequestErrorCertificateRequired:
			errorMessage = new UTF8String("Certificate required");
			break;
		case kWinHttpRequestErrorLoginFailure:
			errorMessage = new UTF8String("Login failure");
			break;
		default:
			errorMessage = new UTF8String("Unknown error");
			break;
	}

	return errorMessage;
}

void CALLBACK debugAsyncCallback(
	HINTERNET hInternet, 
	DWORD_PTR dwContext, 
	DWORD dwInternetStatus,
	LPVOID lpvStatusInformation, 
	DWORD dwStatusInformationLength
	)
{
	switch (dwInternetStatus)
	{
		case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
			debug("WINHTTP_CALLBACK_STATUS_RESOLVING_NAME");
			break;

		case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
			debug("WINHTTP_CALLBACK_STATUS_NAME_RESOLVED");
			break;

		case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
			debug("WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER");
			break;

		case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
			debug("WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER");
			break;

		case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
			debug("WINHTTP_CALLBACK_STATUS_SENDING_REQUEST");
			break;

		case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
			debug("WINHTTP_CALLBACK_STATUS_REQUEST_SENT");
			break;

		case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
			debug("WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE");
			break;

		case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
			debug("WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED");
			break;

		case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
			debug("WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION");
			break;

		case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
			debug("WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED");
			break;

		case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
			debug("WINHTTP_CALLBACK_STATUS_HANDLE_CREATED");
			break;

		case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
			debug("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING");
			break;

		case WINHTTP_CALLBACK_STATUS_DETECTING_PROXY:
			debug("WINHTTP_CALLBACK_STATUS_DETECTING_PROXY");
			break;

		case WINHTTP_CALLBACK_STATUS_REDIRECT:
			debug("WINHTTP_CALLBACK_STATUS_REDIRECT");
			break;

		case WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE:
			debug("WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE");
			break;

		case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
			debug("WINHTTP_CALLBACK_STATUS_SECURE_FAILURE");
			break;

		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
			debug("WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE");
			break;

		case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
			debug("WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE");
			break;

		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
			debug("WINHTTP_CALLBACK_STATUS_READ_COMPLETE");
			break;

		case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
			debug("WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE");
			break;

		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
			debug("WINHTTP_CALLBACK_STATUS_REQUEST_ERROR");
			break;

		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
			debug("WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE");
			break;

		default:
			debug("WinHttpStatusChanged callback - unknown status: %u", dwInternetStatus);
			break;
	}
}

/// Static function called by WinHttp on another thread.
/// All connection, request, and response status changes are passed to this function.
/// @param hInternet Handle to the WinHttp "Open", "Connection", or "Request" operation.
/// @param dwContext Pointer to the WinHttpAsyncRequestSession object used to manage this operation.
/// @param dwInternetStatus Value indicating the reason this function was invoked.
/// @param lpvStatusInformation Data related to the operation identified by parameter "hInternet".
/// @param dwStatusInformationLength Data related to the operation identified by parameter "hInternet".
void WinHttpRequestOperation::OnAsyncWinHttpStatusChanged(
		HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
		LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	// debugAsyncCallback( hInternet, dwContext, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength );

	WinHttpAsyncRequestSessionData* asyncSessionPointer;
	DWORD statusCode;
	DWORD statusCodeSize;
	BOOL wasSuccessful;

	// Fetch the session object.
	if (NULL == dwContext)
	{
		return;
	}
	asyncSessionPointer = (WinHttpAsyncRequestSessionData*)dwContext;

	if (WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING == dwInternetStatus)
	{
		// *THIS* is our one and only signal that we will not be getting called for this request 
		// handle anymore and that we are free to release resources associated with dwContext.
		//
		// Per the WinHttpCloseHandle docs:
		//
		// "If an application associates a context data structure or object with the handle, it should
		// maintain that binding until the callback function receives a WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING
		// notification. This is the last callback notification WinHTTP sends prior to deleting a handle 
		// object from memory."
		//
		// And:
		//
		// "It might seem that the context data structure could then be freed immediately rather than having
		// to wait for a WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING notification, but this is not the case: WinHTTP
		// does not synchronize WinHttpSetStatusCallback with callbacks originating in worker threads. As a 
		// result, a callback could already be in progress from another thread, and the application could receive
		// a callback notification even after having NULLed-out the callback function pointer and deleted the 
		// handle's context data structure. Because of this potential race condition, be conservative in freeing the
		// context structure until after having received the WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING notification."
		//
		// So yeah, we're going to do that ;)
		//
		// Note that this is the request handle we're processing here (as it is what's assiciated with the context
		// data).  The connection handle is closed immediately after the request handle, but has no context data.
		// Since we're not re-using the connection handle and it's not associated with the context data, there is no
		// need to wait for it to signal that it's closed.
		//
		debug("Request handle closing: %u", hInternet);
		asyncSessionPointer->RequestComplete = true;
	}

	// Do not continue if the request operation has been flagged as completed.
	if (asyncSessionPointer->HasAsyncOperationEnded)
	{
		return;
	}

	// Perform the next WinHttp operation.
	switch (dwInternetStatus)
	{
		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:

			debug("WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE/WRITE_COMPLETE");

			if (dwStatusInformationLength == sizeof(DWORD))
			{
				DWORD bytesWritten = *(LPDWORD)lpvStatusInformation;
				debug("WinHttp thread - uploaded %u request body bytes", bytesWritten);
				asyncSessionPointer->RequestBodyBytesCurrent += bytesWritten;
			}

			if (asyncSessionPointer->RequestBodyBytesCurrent < asyncSessionPointer->RequestBodyBytesTotal)
			{
				// More bytes to uploade, let's do it!
				//
				DWORD bodyLen = 0;
				LPCVOID bodyPtr = NULL;
				bool bDeleteBodyPtr = false;

				if (NULL != asyncSessionPointer->RequestBody)
				{
					switch (asyncSessionPointer->RequestBody->bodyType)
					{
						case TYPE_STRING:
						{
							DWORD fullBodyLen = asyncSessionPointer->RequestBody->bodyString->length();
							const char *fullBodyPtr = asyncSessionPointer->RequestBody->bodyString->c_str();
							bodyPtr = &fullBodyPtr[asyncSessionPointer->RequestBodyBytesCurrent];
							bodyLen = min(SESSION_TX_BUFFER_SIZE, fullBodyLen - asyncSessionPointer->RequestBodyBytesCurrent); 
							debug("Uploading %u chars from text string", bodyLen);
						}
						break;

						case TYPE_BYTES:
						{
							DWORD fullBodyLen = asyncSessionPointer->RequestBody->bodyBytes->size();
							unsigned char * fullBodyPtr = &asyncSessionPointer->RequestBody->bodyBytes->at(0);
							bodyPtr = &fullBodyPtr[asyncSessionPointer->RequestBodyBytesCurrent];
							bodyLen = min(SESSION_TX_BUFFER_SIZE, fullBodyLen - asyncSessionPointer->RequestBodyBytesCurrent); 
							debug("Uploading %u bytes from binary string", bodyLen);
						}
						break;

						case TYPE_FILE:
						{
							size_t bytesRead = 0;
							size_t bufferSize = SESSION_TX_BUFFER_SIZE;
							char *buffer = new char[bufferSize];

							try
							{
								bytesRead = ::fread(
									buffer, 
									sizeof(buffer[0]), 
									bufferSize/sizeof(buffer[0]), 
									asyncSessionPointer->UploadFileStream 
									);
							}
							catch (...) { }
							if ( bytesRead > 0 )
							{
								debug("Successfully read %u bytes from request body file, uploading", bytesRead);
								bodyLen = bytesRead;
								bodyPtr = buffer;
								bDeleteBodyPtr = true;
							}
							else
							{
								CORONA_LOG("Error reading from request body file");
								delete [] buffer;
								asyncSessionPointer->ErrorResult = kWinHttpRequestErrorUnknown;
								asyncSessionPointer->HasAsyncOperationEnded = true;
								return;
							}
						}
						break;
					};
				}

				wasSuccessful = ::WinHttpWriteData(
					asyncSessionPointer->RequestHandle,
					bodyPtr,
					bodyLen,
					NULL
					);

				if (bDeleteBodyPtr)
				{
					delete [] bodyPtr;
				}

				if (!wasSuccessful)
				{
					debug("HTTP write failed - error: %u", ::GetLastError());
					asyncSessionPointer->ErrorResult = GetRequestErrorFromWinHttpError(::GetLastError());
					asyncSessionPointer->HasAsyncOperationEnded = true;
					return;
				}
			}
			else
			{
				// Done uploading body (if any)
				//

				// If we were uploading from a file, we're done now, so close it...
				//
				if ( NULL != asyncSessionPointer->UploadFileStream )
				{
					try
					{
						::fclose( asyncSessionPointer->UploadFileStream );
					}
					catch (...) { }
					asyncSessionPointer->UploadFileStream = NULL;
				}

				// Now lets read the response...
				//
				wasSuccessful = ::WinHttpReceiveResponse(asyncSessionPointer->RequestHandle, NULL);
				if (FALSE == wasSuccessful)
				{
					asyncSessionPointer->ErrorResult = kWinHttpRequestErrorUnknown;
					asyncSessionPointer->HasAsyncOperationEnded = true;
				}
			}
			break;

		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
			debug("WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE");

			// Get the status code.
			//
			statusCode = HTTP_STATUS_OK;
			statusCodeSize = sizeof(DWORD);
			wasSuccessful = ::WinHttpQueryHeaders(
				asyncSessionPointer->RequestHandle,
				WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX,
				&statusCode, 
				&statusCodeSize,
				WINHTTP_NO_HEADER_INDEX
				);
			if (FALSE == wasSuccessful)
			{
				CORONA_LOG("Failed to get response status");
				asyncSessionPointer->ErrorResult = kWinHttpRequestErrorUnknown;
				asyncSessionPointer->HasAsyncOperationEnded = true;
				break;
			}
			asyncSessionPointer->ReceivedStatusCode = statusCode;

			// Read the headers.
			//
			{
				DWORD dwSize = 0;
				WinHttpQueryHeaders( 
					asyncSessionPointer->RequestHandle, 
					WINHTTP_QUERY_RAW_HEADERS_CRLF,
					WINHTTP_HEADER_NAME_BY_INDEX, 
					NULL,
					&dwSize,
					WINHTTP_NO_HEADER_INDEX
					);

				// Allocate memory for the buffer.
				if( GetLastError( ) == ERROR_INSUFFICIENT_BUFFER )
				{
					const WCHAR* lpOutBuffer = new WCHAR[dwSize/sizeof(WCHAR)];

					// Now, use WinHttpQueryHeaders to retrieve the header.
					wasSuccessful = WinHttpQueryHeaders( 
						asyncSessionPointer->RequestHandle,
						WINHTTP_QUERY_RAW_HEADERS_CRLF,
						WINHTTP_HEADER_NAME_BY_INDEX,
						(LPVOID)lpOutBuffer, 
						&dwSize,
						WINHTTP_NO_HEADER_INDEX
						);
					if (FALSE == wasSuccessful)
					{
						CORONA_LOG("Failed to get response headers");
						asyncSessionPointer->ErrorResult = kWinHttpRequestErrorUnknown;
						asyncSessionPointer->HasAsyncOperationEnded = true;
						break;
					}

					asyncSessionPointer->ResponseHeaders = utf8_encode( lpOutBuffer, dwSize/sizeof(WCHAR) );
					asyncSessionPointer->ResponseHeadersReady = true;

					delete [] lpOutBuffer;
				}
			}

			// Fetch response data.
			wasSuccessful = ::WinHttpReadData(
				asyncSessionPointer->RequestHandle,
				asyncSessionPointer->ReceiveBuffer,
				sizeof(asyncSessionPointer->ReceiveBuffer), 
				NULL
				);
			if (FALSE == wasSuccessful)
			{
				asyncSessionPointer->ErrorResult = kWinHttpRequestErrorUnknown;
				asyncSessionPointer->HasAsyncOperationEnded = true;
			}
			break;

		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
			debug("WINHTTP_CALLBACK_STATUS_READ_COMPLETE");
			// Check if we have received any new data.
			if (dwStatusInformationLength > 0)
			{
				// Data has been received. Have the data copied to its target.
				asyncSessionPointer->ReceivedByteCount = (int)dwStatusInformationLength;
				debug("Processing thread signalled that %i new bytes are available", asyncSessionPointer->ReceivedByteCount);

				// Note: The processing thread will read more data after it has processed the pending data.
			}
			else
			{
				// All response data has been received. Inform main thread that we're done.
				debug("Signal the main thread that all response data has been received");
				asyncSessionPointer->ErrorResult = kWinHttpRequestErrorNone;
				asyncSessionPointer->HasAsyncOperationEnded = true;
			}
			break;

		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
			debug("WINHTTP_CALLBACK_STATUS_REQUEST_ERROR");
			asyncSessionPointer->ErrorResult = GetRequestErrorFromWinHttpError(::GetLastError());
			asyncSessionPointer->HasAsyncOperationEnded = true;
			break;

		case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
			debug("WINHTTP_CALLBACK_STATUS_SECURE_FAILURE");
			asyncSessionPointer->ErrorResult = kWinHttpRequestErrorCertificateRequired;
			asyncSessionPointer->HasAsyncOperationEnded = true;
			break;
	}
}

#pragma endregion


#pragma region Private Helper Functions
/// Converts the given UTF-8 string to a UTF-16 string and returns it.
/// @param utf8String The UTF-8 string to be converted. Can be NULL.
/// @return Returns a new UTF-16 string matching the given UTF-8 string.
///         You must destroy this returned string yourself by calling the DestroyUtf16String() function.
///         <br>
///         Returns NULL if given a NULL argument or if unable to convert the given string.
wchar_t* WinHttpRequestOperation::CreateUtf16StringFrom(const char* utf8String)
{
	wchar_t *utf16String = NULL;
	int conversionLength;

	if (utf8String)
	{
		conversionLength = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, NULL, 0);
		if (conversionLength > 0)
		{
			utf16String = (wchar_t*)malloc((size_t)conversionLength * sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, utf16String, conversionLength);
		}
	}
	return utf16String;
}

/// Deletes the given UTF-16 string from memory.
/// @param utf16String Pointer to the string returned by the CreateUtf16StringFrom() function.
void WinHttpRequestOperation::DestroyUtf16String(wchar_t *utf16String)
{
	if (utf16String)
	{
		free(utf16String);
	}
}

#pragma endregion
