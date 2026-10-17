 
Contributor: Robert Dickinson (bob.dickinson at gmail)


New network request() and cancel() API implementations
======================================================================

New features:
--------------------------------

Request is cancellable

Reports progress (either upload or download)

Provides access to full response status line and response headers

Supports upload/download of text or binary content as Lua string

Supports upload from file / download to file

On download, determines whether content to be downloaded is text, and if so, determines correct charset encoding and decodes content correctly (this includes not only evaluating the Content-Type header for a charset, but looking at the Content-Type to see if it is likely text, and if so, looking ahead into HTML and XML document content for meta headers that indicate the charset encoding).

On upload of text content, applies and specified charset encoding (base on charset in Content-Type header)

File uploads and downloads are streamed (in the old code, even network.download downloaded the entire file into memory before writing it out on some platforms)

On download to file, the file is not written/over-written unless the download is successful (success response code and completes successfully)

Extensive parameter validation

New network.upload() entrypoint to make file uploads easier

The network event listener can now be either a function or table listener


Implementation / Testing notes:
--------------------------------

main.lua contains a lua-TestMore based test suite (53 test "jobs" with 414 total tests, all of which should pass on all four platforms).  I made slight modifications to lua-TestMore (I started with the mainline distribution, not the Corona version), but I don't see any reason why the included tests wouldn't work with the Corona version.

I implemented a test server for my unit tests and deployed it to Heroku (it's always up, and it made testing over 3G/LTE easier, among other things).  The server is based on httpbin (which is itself a pretty thin Flask app).  I added a couple of endpoints and some static content as required by the unit tests.  

In order for the various file upload/download tests to work, PNGCRUSH is disabled on the iOS builds using:

	export CORONA_COPY_PNG_PRESERVE="--preserve"

There is an undocumented parameter to network.request() called "debug".  If set to "true", a debug table will be created in the response event that will contain a variety of diagnostic information (including the text of any error/exception).  This was needed in order to do some of the test validation ("gray box" testing). 

There are debug() methods on both iOS and Android that report a significant amount of detail during execution of the code to the console.  The implementations are commented out in this drop.


Open Issues:
--------------------------------

Existing unit tests - I would have liked to have access to the existing unit tests for these APIs so that I could have verified them myself before submitting this code.

Closed bugs - I would have liked to have access to the list of closed bugs concerning these APIs in order to make sure that I didn't reintroduce any of them (and to add unit tests for any that aren't currently covered).

Open bugs - I would have liked to have access to the list of open bugs concerning these APIs in order to see if there were any that apply to this code and would be easy to fix before release.

I still need to take another pass at the Markdown docs for this and make sure it all matches the implementation.  I'll note the backward compatibility issues at that time.
