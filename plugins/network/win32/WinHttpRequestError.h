//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _WinHttpRequestError_H_
#define _WinHttpRequestError_H_


/// Indicates the error that might have occurred during an HTTP requestion operation.
enum WinHttpRequestError
{
	/// No error has occurred.
	kWinHttpRequestErrorNone,

	/// Unable to connect to the server.
	kWinHttpRequestErrorConnectionFailure,

	/// The HTTP request was aborted on the local machine.
	kWinHttpRequestErrorAborted,

	/// The server failed to respond in time.
	kWinHttpRequestErrorTimedOut,

	/// Could not connect because the URL was invalid.
	kWinHttpRequestErrorInvalidUrl,

	/// The server requires an certificate authentication from the client.
	kWinHttpRequestErrorCertificateRequired,

	/// The server requires login access. Failed to log in to the server.
	kWinHttpRequestErrorLoginFailure,

	/// An internal error on the client side such as out of memory, invalid handles, etc.
	kWinHttpRequestErrorInternal,

	/// An unknown error has occurred during the request.
	kWinHttpRequestErrorUnknown
};

#endif
