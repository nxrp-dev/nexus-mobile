//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _WinHttpRequestResult_H_
#define _WinHttpRequestResult_H_

#include "WinHttpRequestError.h"
#include "WinString.h"


/// Stores the result of an HTTP request operation such as the response and any error that has occurred.
struct WinHttpRequestResult
{
public:
	/// Indicates the error that has occurred during an HTTP request operation.
	/// Set to None if no error has occurred.
	WinHttpRequestError Error;

	/// The HTTP status code that was received in the response packet from the server.
	/// Set to -1 if a response was not received.
	int StatusCode;

	/// Set to the body of the HTTP response if not downloading to file.
	/// If downloading to file, then this is set to path\file name the response body was written to.
	/// Set to an empty string if an error occurred.
	WinString Response;


	/// Creates a new initialized result object.
	WinHttpRequestResult()
	{
		Error = kWinHttpRequestErrorNone;
		StatusCode = -1;
	}
};


#endif
