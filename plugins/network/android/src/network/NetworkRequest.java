//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

package network;

import java.io.*;
import java.net.*;
import java.util.*;
import java.util.regex.*;
import java.util.concurrent.atomic.*;
import java.nio.charset.*;

import java.util.concurrent.CopyOnWriteArrayList;

import com.naef.jnlua.*;

import com.ansca.corona.*;
import com.ansca.corona.notifications.*;

// Needed for CoronaFileSpec (on-device, for resource files)
//
import android.content.Context;
import android.util.Base64;

/**
 * Implements the network request() function in Lua.
 */

public class NetworkRequest implements com.naef.jnlua.NamedJavaFunction
{
	private LuaLoader fLoader;

	private static final String EVENT_NAME = "networkRequest";

	// Using a CopyOnWriteArrayList even with its performance hit because there shouldn't be too many open at a time so even if
	// everything is copied everytime it shouldn't be too much.
	private CopyOnWriteArrayList<AsyncNetworkRequestRunnable> fOpenRequests = null;

	public NetworkRequest(LuaLoader loader)
	{
		this.fLoader = loader;
		this.fOpenRequests = new CopyOnWriteArrayList<AsyncNetworkRequestRunnable>();
	}

	public void abortOpenConnections( CoronaRuntime runtime )
	{
		for ( AsyncNetworkRequestRunnable connection : this.fOpenRequests )
		{
			debug("Aborting connection");
			connection.abort( runtime );
		}
		this.fOpenRequests.clear();
	}

	public static void debug( String message, Object ... vargs )
	{
		//System.out.println("NETWORK DEBUG: " + String.format(message, vargs));
	}

	public static void error( String message, Object ... vargs )
	{
		if (vargs.length > 0) {
			android.util.Log.e("Corona", "ERROR: network: " + String.format(message, vargs));
		} else {
			android.util.Log.e("Corona", "ERROR: network: " + message);
		}
	}

	public static void paramValidationFailure( LuaState luaState, String message, Object ... vargs )
	{
		// For now we're just going to log this.  We take a LuaState in case we decide at some point that
		// we want to do more (like maybe throw a Lua exception).
		//

		// Our version of JNLua doesn't implement luaL_where so no location info for us on Android

		error(String.format(message, vargs));
	}

	// Concatenate header values from the list of header values for a given header, per
	// the HTTP protocol rules...
	//
	private static String concatHeaderValues( List<String> strings )
	{
		// RFC 2616, Section "4.2 Message Headers" says:
		//
		// Multiple message-header fields with the same field-name MAY be
		// present in a message if and only if the entire field-value for that
		// header field is defined as a comma-separated list [i.e., #(values)].
		// It MUST be possible to combine the multiple header fields into one
		// "field-name: field-value" pair, without changing the semantics of the
		// message, by appending each subsequent field-value to the first, each
		// separated by a comma.
		//
		StringBuilder sb = new StringBuilder();
		String sep = "";
		for ( String s: strings )
		{
			sb.append(sep).append(s);
			sep = ",";
		}
		return sb.toString();
	}

	// Parse the "charset" parameter, if any, from a Content-Type header
	//
	private static String getContentTypeEncoding( String contentTypeHeader )
	{
		String charset = null;
		if ( null != contentTypeHeader )
		{
			String[] values = contentTypeHeader.split(";");

			for (String value : values)
			{
				value = value.trim();

				if (value.toLowerCase().startsWith("charset="))
				{
					charset = value.substring("charset=".length());
					debug("Explicit charset was found in content type, was: %s", charset);
				}
			}
		}
		return charset;
	}

	private static boolean isContentTypeXML ( String contentType )
	{
		Pattern regex = Pattern.compile("^application/(?:\\w+)[+]xml"); // application/rss+xml, many others
		Matcher applicationPlusXmlPatthernMatcher = regex.matcher(contentType);

		return (
			contentType.startsWith("text/xml") ||
			contentType.startsWith("application/xml") ||
			contentType.startsWith("application/xhtml") ||
			applicationPlusXmlPatthernMatcher.find()
		);
	}

	private static boolean isContentTypeHTML ( String contentType )
	{
		return (
			contentType.startsWith("text/html") ||
			contentType.startsWith("application/xhtml")
		);
	}

	private static boolean isContentTypeText ( String contentType )
	{
		// Text types, use utf-8 to decode if no encoding specified
		//
		return (
			NetworkRequest.isContentTypeXML(contentType) ||
			NetworkRequest.isContentTypeHTML(contentType) ||
			contentType.startsWith("text/") ||
			contentType.startsWith("application/json") ||
			contentType.startsWith("application/javascript") ||
			contentType.startsWith("application/x-javascript") ||
			contentType.startsWith("application/ecmascript") ||
			contentType.startsWith("application/x-www-form-urlencoded")
		);
	}

	// For structured text types (html or xml) look for embedded encoding.  Here is a good overview of the
	// state of this problem:
	//
	//     http://en.wikipedia.org/wiki/Character_encodings_in_HTML
	//
	private static String getEncodingFromContent ( String contentType, String content )
	{
		// Note: This logic accomodates the fact that application/xhtml (and the -xml variant) is both XML
		//       and HTML, and the rule is to use the XML encoding if present, else HTML.
		//
		String charset = null;

		debug("Looking for embedded encoding in content: <BODY>%s</BODY>", content);

		if (NetworkRequest.isContentTypeXML(contentType))
		{
			// Look in XML encoding meta header (ex: http://www.nasa.gov/rss/breaking_news.rss)
			//
			//   <?xml version="1.0" encoding="utf-8"?>
			//
			Pattern xmlMetaPattern = Pattern.compile("<?xml\\b[^>]*\\bencoding=[\'\"]([a-zA-Z0-9_\\-]+)[\'\"].*[^>]*?>", Pattern.CASE_INSENSITIVE);
			Matcher xmlMetaMatcher = xmlMetaPattern.matcher(content);
			if (xmlMetaMatcher.find())
			{
				charset = xmlMetaMatcher.group(1);
				debug("Found charset in XML meta header encoding attribute: %s", charset);
			}
		}

		if ((null == charset) && (NetworkRequest.isContentTypeHTML(contentType)))
		{
			// Look in HTML meta "charset" tag (ex: http://www.android.com)
			//
			//   <meta charset="utf-8">
			//
			Pattern metaCharsetPattern = Pattern.compile("<meta\\b[^>]*\\bcharset=[\'\"]([a-zA-Z0-9_\\-]+)[\'\"][^>]*>", Pattern.CASE_INSENSITIVE);
			Matcher metaCharsetMatcher = metaCharsetPattern.matcher(content);
			if (metaCharsetMatcher.find())
			{
				charset = metaCharsetMatcher.group(1);
				debug("Found charset in HTML meta charset header: %s", charset);
			}

			if ( null == charset )
			{
				// Look in HTML HTTP Content-Type meta header (ex: http://www.cnn.com)
				//
				//   <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
				//
				// First we need to find the full tag for this header.  Since the attributes may be in any order, we will have to do a
				// second pass to extract the charset out of the "content" attribute.
				//
				Pattern metaHttpCtHeaderPattern = Pattern.compile("<meta\\b[^>]*\\bhttp-equiv=[\'\"](?:Content-Type)[\'\"][^>]*>", Pattern.CASE_INSENSITIVE);
				Matcher metaHttpCtHeaderMatcher = metaHttpCtHeaderPattern.matcher(content);
				if ( metaHttpCtHeaderMatcher.find() )
				{
					String httpMetaCtHeader = metaHttpCtHeaderMatcher.group();
					debug("Found httpMetaCtHeader: %s", httpMetaCtHeader);

					Pattern metaHttpCtCharsetPattern = Pattern.compile("\\bcharset=([a-zA-Z0-9_\\-]+)\\b", Pattern.CASE_INSENSITIVE);
					Matcher metaHttpCtCharsetMatcher = metaHttpCtCharsetPattern.matcher(httpMetaCtHeader);
					if ( metaHttpCtCharsetMatcher.find() )
					{
						charset = metaHttpCtCharsetMatcher.group(1);
						debug("Found charset in meta Content-Type header: %s", charset);
					}
				}
			}
		}

		return charset;
	}

	private enum ProgressDirection
	{
		UPLOAD, DOWNLOAD, NONE;

		public static ProgressDirection fromString( String progress )
		{
			ProgressDirection direction = null;

			if ( null != progress )
			{
				if ( "upload".equalsIgnoreCase( progress ) )
				{
					direction = UPLOAD;
				}
				else if ( "download".equalsIgnoreCase( progress ) )
				{
					direction = DOWNLOAD;
				}
			}
			else
			{
				direction = NONE;
			}
			return direction;
		}

		public static String toString( ProgressDirection progress )
		{
			String direction = null;

			switch( progress )
			{
				case UPLOAD:
					direction = "upload";
					break;
				case DOWNLOAD:
					direction = "download";
					break;
				default:
					direction = "none";
					break;
			}

			return direction;
		}
	}

	private class CoronaFileSpec
	{
		public String 	filename = null;
		public Object 	baseDirectory = null;
		public String 	fullPath = null;
		public Boolean 	isResourceFile = false;

		public CoronaFileSpec( String filename, Object baseDirectory, String fullPath, Boolean isResourceFile )
		{
			this.filename = filename;
			this.baseDirectory = baseDirectory;
			this.fullPath = fullPath;
			this.isResourceFile = isResourceFile;
		}

		public InputStream getInputStream( ) throws IOException
		{
			InputStream is = null;

			if ( this.fullPath.startsWith("content://"))
			{
				Context context = CoronaEnvironment.getApplicationContext();
				is = context.getContentResolver().openInputStream(android.net.Uri.parse(this.fullPath));
			}
			else if ( ! this.isResourceFile )
			{
				is = new FileInputStream( this.fullPath );
			}
			else
			{
				// The big issue is that on-device, when using either null or the explicit system.ResourceDirectory,
				// system.pathForFile returns the filename as the path.  It appears that it is then up to the Android
				// programmer to understand that this is the case, and to use the Android resource APIs to access the
				// file contents.
				//
				Context context = CoronaEnvironment.getApplicationContext();
				is = context.getResources().getAssets().open(this.filename);
			}

			return is;
		}

		public long getFileSize( ) throws IOException
		{
			long length = -1;

			if ( ! this.isResourceFile )
			{
				length = new File( this.fullPath ).length();
			}
			else
			{
				InputStream is = this.getInputStream();
				length = is.available();
				is.close();
			}

			return length;
		}
	}

	private class NetworkRequestState
	{
		public Boolean 						isError 			= false;
		public String 						name 				= EVENT_NAME;
		public String 						phase 				= "began";
		public int 							status 				= -1;
		public String  						url 				= null;
		public Map<String, List<String>> 	responseHeaders 	= null;
		public String 						responseType 		= "text";
		public Object 						response 			= null; // String, byte[], or CoronaFileSpec
		public AtomicBoolean				isRequestCancelled 	= null;
		public long 						bytesTransferred 	= 0;
		public long 						bytesEstimated 		= 0;
		public Map<String, String> 			debugValues 		= null;

		public NetworkRequestState ( String url, Boolean isDebug )
		{
			this.url = url;
			this.isRequestCancelled = new AtomicBoolean( false );

			if ( isDebug )
			{
				this.debugValues = new HashMap<String, String>();
				this.debugValues.put("isDebug", "true");
			}
		}

		public NetworkRequestState ( NetworkRequestState baseRequest )
		{
			this.isError 			= baseRequest.isError;
			this.name 				= baseRequest.name;
			this.phase 				= baseRequest.phase;
			this.status 			= baseRequest.status;
			this.url     			= baseRequest.url;
			this.responseHeaders 	= baseRequest.responseHeaders;
			this.responseType 		= baseRequest.responseType;
			this.response 			= baseRequest.response;
			this.isRequestCancelled = baseRequest.isRequestCancelled;
			this.bytesTransferred 	= baseRequest.bytesTransferred;
			this.bytesEstimated 	= baseRequest.bytesEstimated;
			this.debugValues 		= baseRequest.debugValues;
		}

		public void setDebugValue( String key, String value )
		{
			if ( null != this.debugValues )
			{
				this.debugValues.put(key, value);
			}
		}

		public int push( LuaState luaState )
		{
			int luaTableStackIndex = luaState.getTop();

			luaState.pushBoolean(this.isError);
			luaState.setField(luaTableStackIndex, "isError");

			luaState.pushString(this.name);
			luaState.setField(luaTableStackIndex, "name");

			luaState.pushString(this.phase);
			luaState.setField(luaTableStackIndex, "phase");

			luaState.pushJavaObjectRaw(this.isRequestCancelled);
			luaState.setField(luaTableStackIndex, "requestId");

			luaState.pushNumber(this.status);
			luaState.setField(luaTableStackIndex, "status");

			luaState.pushString(this.url);
			luaState.setField(luaTableStackIndex, "url");

			if ( null != this.responseHeaders )
			{
				luaState.newTable(0, this.responseHeaders.size());
				int luaHeaderTableStackIndex = luaState.getTop();

				// Write response headers to table
				//
				for ( Map.Entry<String, List<String>> entry : this.responseHeaders.entrySet() )
				{
					String header = entry.getKey();
					if ( null == header )
					{
						// A null key is the status line
						header = "HTTP-STATUS-LINE";
					}
					debug("Processing header: %s", header);
					String value = NetworkRequest.concatHeaderValues(entry.getValue());

					luaState.pushString(value);
					luaState.setField(luaHeaderTableStackIndex, header);
				}

				luaState.setField(luaTableStackIndex, "responseHeaders");
			}

			if ( null != this.response )
			{
				luaState.pushString(this.responseType);
				luaState.setField(luaTableStackIndex, "responseType");

				// This can be string, byte array, or table of filename/baseDirectory
				//
				if ( this.response instanceof String )
				{
					luaState.pushString((String)this.response);
				}
				else if ( this.response instanceof byte[] )
				{
					luaState.pushString((byte[])this.response);
				}
				else if ( this.response instanceof CoronaFileSpec )
				{
					CoronaFileSpec fileSpec = (CoronaFileSpec)this.response;

					luaState.newTable(0, 3);
					int luaResponseTableStackIndex = luaState.getTop();

					luaState.pushString(fileSpec.filename);
					luaState.setField(luaResponseTableStackIndex, "filename");

					luaState.pushJavaObject(fileSpec.baseDirectory);
					luaState.setField(luaResponseTableStackIndex, "baseDirectory");

					luaState.pushString(fileSpec.fullPath);
					luaState.setField(luaResponseTableStackIndex, "fullPath");
				}

				luaState.setField(luaTableStackIndex, "response");
			}

			luaState.pushNumber(this.bytesTransferred);
			luaState.setField(luaTableStackIndex, "bytesTransferred");

			luaState.pushNumber(this.bytesEstimated);
			luaState.setField(luaTableStackIndex, "bytesEstimated");

			if ( null != this.debugValues )
			{
				luaState.newTable(0, this.debugValues.size());
				int luaHeaderTableStackIndex = luaState.getTop();

				// Write debug values to table
				//
				for ( Map.Entry<String, String> entry : this.debugValues.entrySet() )
				{
					String header = entry.getKey();
					String value = entry.getValue();

					debug("Writing debug value - %s: %s", header, value);

					luaState.pushString(value);
					luaState.setField(luaHeaderTableStackIndex, header);
				}

				luaState.setField(luaTableStackIndex, "debug");
			}

			return 1; // We left 1 item (the table) on the stack
		}
	}

	private class LuaCallback
	{
		private int luaFunctionReferenceKey = CoronaLua.REFNIL;
		private CoronaRuntimeTaskDispatcher taskDispatcher = null;
		private final long 	minNotificationIntervalMs 	= 1000;
		private String 		lastNotificationPhase 		= null;
		private long 		lastNotificationTime 		= 0;


		public LuaCallback( int luaFunctionReferenceKey, CoronaRuntimeTaskDispatcher taskDispatcher )
		{
			this.luaFunctionReferenceKey = luaFunctionReferenceKey;
			this.taskDispatcher = taskDispatcher;
		}

		public boolean call ( final NetworkRequestState baseNetworkRequest )
		{
			return this.call( baseNetworkRequest, false );
		}

		public boolean call ( final NetworkRequestState baseNetworkRequest, boolean shouldUnregister )
		{
			synchronized( this )
			{
				if ( this.luaFunctionReferenceKey == CoronaLua.REFNIL )
				{
					debug("Attempt to post call to callback after it was unregistered, ignoring");
					return false;
				}
				if ( this.taskDispatcher == null )
				{
					debug("Attempt to post call to callback without a CoronaRuntimeTaskDispatcher");
					return false;
				}

				final LuaCallback callback = this;
				final boolean willUnregister = shouldUnregister;

				// We call the callback conditionally based on the following:
				//
				// Rule 1: We don't send notifications if the request has been cancelled.
				//
				if (baseNetworkRequest.isRequestCancelled.get())
				{
					debug("Attempt to post call to callback after cancelling, ignoring");
					return false; // We did not post the callback
				}

				// Rule 2: We don't send multiple notifications of the same type (phase) within a certain interval, in
				//         order to avoid overrunning the listener.
				//
				long currentTime = System.currentTimeMillis();
				if ( baseNetworkRequest.phase.equals( this.lastNotificationPhase ) && ( ( this.lastNotificationTime + this.minNotificationIntervalMs ) > currentTime ) )
				{
					debug("Attempt to post call to callback for phase \"%s\" within notification interval, ignoring", baseNetworkRequest.phase);
					return false; // We did not post the callback
				}
				else
				{
					this.lastNotificationPhase = baseNetworkRequest.phase;
					this.lastNotificationTime = currentTime;
				}

				// We need to make a local copy of the NetworkRequest state, since this notification is getting posted to the Lua
				// thread and the IO thread will continue to update the base NetworkRequest object.
				//
				final NetworkRequestState networkRequest = new NetworkRequestState(baseNetworkRequest);

				// Create a task that will call the given Lua function.
				// This task's executeUsing() method will be called on the Corona runtime thread, just before rendering a frame.
				//
				com.ansca.corona.CoronaRuntimeTask task = new com.ansca.corona.CoronaRuntimeTask()
				{
					@Override
					public void executeUsing(CoronaRuntime runtime)
					{
						int luaFunctionReferenceKey = callback.luaFunctionReferenceKey;
						if (luaFunctionReferenceKey == CoronaLua.REFNIL)
						{
							debug("Attempt to post call to callback after it was unregistered, ignoring (Corona thread)");
							return;
						}

						if (networkRequest.isRequestCancelled.get())
						{
							debug("Attempt to call to callback posted before cancelling, after cancelling, ignoring");
							return;
						}

						debug("Executing callback - thread: %d", Thread.currentThread().getId());
						debug("Executing callback - runtime luaState: %s", Integer.toHexString(System.identityHashCode(runtime.getLuaState())));

						try
						{
							debug("Calling Lua callback");

							LuaState luaState = runtime.getLuaState();
							CoronaLua.newEvent( luaState, EVENT_NAME);
							networkRequest.push( luaState );
							try
							{
								CoronaLua.dispatchEvent( luaState, luaFunctionReferenceKey, 0 );
							}
							catch (LuaRuntimeException lre)
							{
								// Get the Lua stacktrace into a string so we can log it
								ByteArrayOutputStream baos = new ByteArrayOutputStream();
								PrintStream ps = new PrintStream(baos);
								lre.printLuaStackTrace(ps);
								String luaStackTrace = baos.toString("UTF8");
								luaStackTrace = luaStackTrace.replace("com.naef.jnlua.LuaRuntimeException: ", "");

								// The Lua stack is not arranged so that CoronaLuaErrorHandler can
								// get the error info from it so we have to rearrange it a bit
								CoronaLuaErrorHandler cleh = new CoronaLuaErrorHandler();
								luaState.pushString(luaStackTrace);
								luaState.insert(1);
								cleh.invoke(luaState);
							}

							if (willUnregister)
							{
								debug("Unregistering callback after call");
								CoronaLua.deleteRef( luaState, luaFunctionReferenceKey );
								luaFunctionReferenceKey = CoronaLua.REFNIL;
							}
						}
						catch (Exception ex)
						{
							// Write the stack trace to the log with Corona's tag
							StringWriter stacktraceWriter = new StringWriter();
							ex.printStackTrace(new PrintWriter(stacktraceWriter));
							android.util.Log.i("Corona", stacktraceWriter.toString());
						}
					}
				};

				// Send the above task to the Corona runtime asynchronously.
				// The send() method will do nothing if the Corona runtime is no longer available, which can
				// happen if the runtime was disposed/destroyed after the user has exited the Corona activity.
				//
				debug("Posting callback CoronaRuntimeTask to Corona thread");
				this.taskDispatcher.send(task);
			} // synchronized

			return true; // We posted the callback
		}

		// This must be called from the Corona thread (currently only used in onExiting handler)
		//
		public boolean unregister( CoronaRuntime runtime )
		{
			synchronized( this )
			{
				if ( this.luaFunctionReferenceKey == CoronaLua.REFNIL )
				{
					debug("Attempt to unregister callback after it was already unregistered, ignoring");
					return false;
				}

				try
				{
					debug("Unregistering Lua callback using runtime");

					LuaState luaState = runtime.getLuaState();
					CoronaLua.deleteRef( luaState, this.luaFunctionReferenceKey );
					this.luaFunctionReferenceKey = CoronaLua.REFNIL;
				}
				catch (Exception ex)
				{
					// Write the stack trace to the log with Corona's tag
					StringWriter stacktraceWriter = new StringWriter();
					ex.printStackTrace(new PrintWriter(stacktraceWriter));
					android.util.Log.i("Corona", stacktraceWriter.toString());
				}
			} // synchronized

			return true;
		}
	}

	private class NetworkRequestParameters
	{
		public URL 					requestURL			= null;
		public String 				method				= null;
		public Map<String, String>	requestHeaders		= null;
		public Object				requestBody			= null; // String, byte[], or CoronaFileSpec
		public ProgressDirection 	progressDirection 	= ProgressDirection.NONE;
		public Boolean              bBodyIsText			= true;
		public CoronaFileSpec 		response 			= null;
		public int					timeout				= 30;
		public Boolean 				isDebug 			= false;
		public LuaCallback 			callback			= null;
		public StatusBarNotificationSettings	successNotificationSettings = null;
		public Boolean              bHandleRedirects	= true;

		public boolean extractRequestParameters( LuaState luaState )
		{
			int arg = 1;

			Boolean isInvalid = false;

			// First argument - url (required)
			//
			if ( LuaType.STRING == luaState.type( arg ) )
			{
				String urlString = luaState.toString( arg );

				try
				{
					this.requestURL = new URL(urlString);
				}
				catch (MalformedURLException e)
				{
					paramValidationFailure( luaState, "Malformed URL: %s", urlString );
					isInvalid = true;
				}
			}
			else
			{
				paramValidationFailure( luaState, "First argument to network.request() should be a URL string" );
				isInvalid = true;
			}

			++arg;

			// Second argument - method (required)
			//
			if (!isInvalid)
			{
				if ( LuaType.STRING == luaState.type( arg ) )
				{
					// This is validated in the Lua class
					this.method = luaState.toString( arg );
					++arg;
				}
				else
				{
					this.method = "GET";
				}
			}

			// Third argument - listener (required)
			//
			if (!isInvalid)
			{
				if ( CoronaLua.isListener( luaState, arg, EVENT_NAME ) )
				{
					// Fetch the given Lua state's associated Corona runtime dispatcher.
					CoronaRuntimeTaskDispatcher runtimeTaskDispatcher;
					runtimeTaskDispatcher = new CoronaRuntimeTaskDispatcher(luaState);
					if (runtimeTaskDispatcher.isRuntimeUnavailable()) {
						// The resulting task dispatcher does not have an associated runtime.
						// This means the Lua state's runtime was destroyed or it is using a Lua coroutine.
						// So, attempt to use the last Corona runtime's dispatcher that loaded the "network" library.
						runtimeTaskDispatcher = fLoader.getRuntimeTaskDispatcher();
					}

					// Create the Lua callback object to be invoked when the network request finishes.
					if (runtimeTaskDispatcher != null) {
						int luaFunctionReferenceKey = CoronaLua.newRef( luaState, arg );
						this.callback = new LuaCallback( luaFunctionReferenceKey, runtimeTaskDispatcher );
					}
					++arg;
				}
			}

			// Fourth argument - params table (optional)
			//
			int paramsTableStackIndex = arg;

			if (!isInvalid && !luaState.isNoneOrNil( arg ))
			{
				if ( LuaType.TABLE == luaState.type( arg ) )
				{
					Boolean wasRequestContentTypePresent = false;

					luaState.getField(paramsTableStackIndex, "headers");
					if (!luaState.isNil(-1))
					{
						// If we got something, make sure it's a table
						if ( LuaType.TABLE == luaState.type( -1 ) )
						{
							for (luaState.pushNil(); luaState.next(-2); luaState.pop(1))
							{
								// Fetch the table entry's string key.
								// An index of -2 accesses the key that was pushed into the Lua stack by luaState.next() up above.
								String keyName = luaState.toString(-2);
								if (keyName == null)
								{
									// A valid key was not found. Skip this table entry.
									continue;
								}

								if ( "Content-Length".equalsIgnoreCase( keyName ) )
								{
									// You just don't worry your pretty little head about the Content-Length, we'll handle that...
									continue;
								}

								// Fetch the table entry's value in string form.
								// An index of -1 accesses the entry's value that was pushed into the Lua stack by luaState.next() above.
								String valueString = null;
								switch (luaState.type(-1))
								{
									case STRING:
										valueString = luaState.toString(-1);
										break;
									case NUMBER:
										double numericValue = luaState.toNumber(-1);
										if ( Math.floor( numericValue ) == numericValue )
										{
											valueString = Long.toString(Math.round(numericValue));
										}
										else
										{
											valueString = Double.toString(numericValue);
										}
										break;
									case BOOLEAN:
										valueString = Boolean.toString(luaState.toBoolean(-1));
										break;
								}

								if (valueString != null)
								{
									if (this.requestHeaders == null)
									{
										this.requestHeaders = new HashMap<String, String>();
									}

									if ( "Content-Type".equalsIgnoreCase( keyName ) )
									{
										wasRequestContentTypePresent = true;

										String ctCharset = getContentTypeEncoding( valueString );
										if ( null != ctCharset )
										{
											try
											{
												Charset.forName(ctCharset);
											}
											catch (Exception e)
											{
												paramValidationFailure( luaState, "'header' value for Content-Type header contained an unsupported character encoding: %s", ctCharset );
												isInvalid = true;
											}
										}
									}

									this.requestHeaders.put(keyName, valueString);
									debug("Header - %s: %s", keyName, valueString);
								}
							}
						}
						else
						{
							paramValidationFailure( luaState, "'headers' value of params table, if provided, should be a table (got %s)", luaState.type(-1).toString() );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					//If this is a POST request and the user hasn't filled in the content-type
					//we make an assumption (to preserve existing functionality)
					if (null == this.requestHeaders &&
						this.method.equals("POST") &&
						!wasRequestContentTypePresent)
					{
						this.requestHeaders = new HashMap<String, String>();
						this.requestHeaders.put("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
						wasRequestContentTypePresent = true;
					}

					// If this is a HEAD request and we're on an older version of Android,
					// set Accept-Encoding so we don't get compressed responses because
					// that throws exceptions in the GZIP handler
					if (this.method.equals("HEAD") &&
						android.os.Build.VERSION.SDK_INT < 21)
					{
						if (this.requestHeaders == null)
						{
							this.requestHeaders = new HashMap<String, String>();
						}

						this.requestHeaders.put("Accept-Encoding", "identity");
					}

					this.bHandleRedirects = true;
					luaState.getField(paramsTableStackIndex, "handleRedirects");
					if (!luaState.isNil(-1))
					{
						if ( LuaType.BOOLEAN == luaState.type( -1 ) )
						{
							this.bHandleRedirects = luaState.toBoolean(-1);
							debug("Redirect option provided, was: %s", (this.bHandleRedirects?"true":"false"));
						}
						else
						{
							paramValidationFailure( luaState, "'handleRedirects' value of params table, if provided, should be a boolean value (got %s)", luaState.type(-1).toString() );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					this.bBodyIsText = true;
					luaState.getField(paramsTableStackIndex, "bodyType");
					if (!luaState.isNil(-1))
					{
						// If we got something, make sure it's a string
						if ( LuaType.STRING == luaState.type( -1 ) )
						{
							String bodyType = luaState.toString(-1);
							if ( bodyType.matches("\\b(text)|(binary)\\b") )
							{
								this.bBodyIsText = bodyType.matches("\\b(text)\\b");
								debug("Request body is text: %b", this.bBodyIsText);
							}
							else
							{
								paramValidationFailure( luaState, "'bodyType' value of params table was invalid, must be either \"text\" or \"binary\", but was: \"%s\"", bodyType );
								isInvalid = true;
							}
						}
						else
						{
							paramValidationFailure( luaState, "'bodyType' value of params table, if provided, should be a string value (got %s)", luaState.type(-1).toString() );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					luaState.getField(paramsTableStackIndex, "body");
					if (!luaState.isNil(-1))
					{
						// This can be either a Lua string containing the body, or a table with filename/baseDirectory that points to a body file.
						// If it's a string, it can either be "text" (Java String) or "binary" (byte[]), based on bodyType (above).
						//
						switch (luaState.type(-1))
						{
							case STRING:
								if (this.bBodyIsText)
								{
									debug("Request body from String (text)");
									this.requestBody = luaState.toString(-1);

									if (!wasRequestContentTypePresent)
									{
										if (this.requestHeaders == null)
										{
											this.requestHeaders = new HashMap<String, String>();
										}

										String contentKey = "Content-Type";
										String contentType = "text/plain; charset=UTF-8";
										this.requestHeaders.put(contentKey, contentType);
										wasRequestContentTypePresent = true;
									}
								}
								else
								{
									debug("Request body from String (binary)");
									this.requestBody = luaState.toByteArray(-1);

									if (!wasRequestContentTypePresent)
									{
										if (this.requestHeaders == null)
										{
											this.requestHeaders = new HashMap<String, String>();
										}

										String contentKey = "Content-Type";
										String contentType = "application/octet-stream";
										this.requestHeaders.put(contentKey, contentType);
										wasRequestContentTypePresent = true;
									}
								}
								break;

							case TABLE:
								// Body type for body from file is always binary
								//
								this.bBodyIsText = false;

								// Extract filename/baseDirectory
								//
								luaState.getField(-1, "filename"); // required
								if ( LuaType.STRING == luaState.type( -1 ) )
								{
									int 	numParams = 1;
									String 	filename = null;
									Object 	baseDirectory = null;

									filename = luaState.toString(-1);
									luaState.pop(1);

									luaState.getField(-1, "baseDirectory"); // optional
									if (!luaState.isNoneOrNil(-1))
									{
										baseDirectory = luaState.toJavaObject(-1, Object.class);
									}
									luaState.pop(1);

									// Prepare and call Lua function
									luaState.getGlobal("_network_pathForFile"); // Push the function on the stack
									luaState.pushString(filename); // Push argument #1
									if (null != baseDirectory)
									{
										luaState.pushJavaObject(baseDirectory); // Push argument #2
										numParams++;
									}

									luaState.call(numParams, 2); // 1/2 arguments, 2 returns
									Boolean isResourceFile = luaState.toBoolean(-1);
									String path = luaState.toString(-2);
									luaState.pop(2); // Pop results

									debug("body filename: %s, baseDirectory: ", filename, baseDirectory);
									debug("pathForFile from LUA: %s - isResourceFile: %b", path, isResourceFile);
									this.requestBody = new CoronaFileSpec( filename, baseDirectory, path, isResourceFile );
								}
								else
								{
									paramValidationFailure( luaState, "body 'filename' value is required and must be a string value" );
									isInvalid = true;
								}
								break;

							default:
								paramValidationFailure( luaState, "Either body string or table specifying body file is required if 'body' is specified (got %s)", luaState.type(-1).toString() );
								isInvalid = true;
								break;
						}

						if ( ( null != this.requestBody ) && !wasRequestContentTypePresent )
						{
							paramValidationFailure( luaState, "Request Content-Type header is required when request 'body' is specified" );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					luaState.getField(paramsTableStackIndex, "progress");
					if (!luaState.isNil(-1))
					{
						if ( LuaType.STRING == luaState.type( -1 ) )
						{
							String progress = luaState.toString(-1);

							this.progressDirection = ProgressDirection.fromString(progress);
							debug("Progress was: %s", ProgressDirection.toString(this.progressDirection));

							if ( null == this.progressDirection )
							{
								// Error, value provided did not map to valid progress direction
								//
								paramValidationFailure( luaState, "'progress' value of params table was invalid, if provided, must be either \"upload\" or \"download\", but was: \"%s\"", progress );
								isInvalid = true;
							}
						}
						else
						{
							paramValidationFailure( luaState, "'progress' value of params table, if provided, should be a string value (got %s)", luaState.type(-1).toString() );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					luaState.getField(paramsTableStackIndex, "response");
					if (!luaState.isNil(-1))
					{
						// If we got something, make sure it's a table
						if ( LuaType.TABLE == luaState.type( -1 ) )
						{
							// Extract filename/baseDirectory
							//
							luaState.getField(-1, "filename"); // required
							if ( LuaType.STRING == luaState.type( -1 ) )
							{
								int 	numParams = 1;
								String 	filename = null;
								Object 	baseDirectory = null;


								filename = luaState.toString(-1);
								luaState.pop(1);

								luaState.getField(-1, "baseDirectory"); // optional
								if (!luaState.isNoneOrNil(-1))
								{
									baseDirectory = luaState.toJavaObject(-1, Object.class);
								}
								luaState.pop(1);

								// Prepare and call Lua function
								luaState.getGlobal("_network_pathForFile"); // Push the function on the stack
								luaState.pushString(filename); // Push argument #1
								if (null != baseDirectory)
								{
									luaState.pushJavaObject(baseDirectory); // Push argument #2
									numParams++;
								}

								luaState.call(numParams, 2); // 1/2 arguments, 2 returns
								Boolean isResourceFile = luaState.toBoolean(-1);
								String path = luaState.toString(-2);
								luaState.pop(2); // Pop results
								debug("response filename: %s, baseDirectory: %s", filename, baseDirectory);
								debug("pathForFile from LUA: %s - isResourceFile: %b", path, isResourceFile);

								// Check if we are downloading a file to external storage.
								boolean isDownloadingToExternalStorage = false;
								try {
									java.io.File externalStorageDirectory =
													android.os.Environment.getExternalStorageDirectory();
									if ((externalStorageDirectory != null) && (path != null)) {
										isDownloadingToExternalStorage =
												path.startsWith(externalStorageDirectory.getAbsolutePath());
									}
								}
								catch (Exception ex) { }
								if (isDownloadingToExternalStorage) {
									// Throw an exception if this application does not have permission.
									android.content.Context context;
									context = com.ansca.corona.CoronaEnvironment.getApplicationContext();
									if (context != null) {
										String permissionName = android.Manifest.permission.WRITE_EXTERNAL_STORAGE;
										context.enforceCallingOrSelfPermission(permissionName, null);
									}
								}

								this.response = new CoronaFileSpec( filename, baseDirectory, path, isResourceFile );
							}
							else
							{
								paramValidationFailure( luaState, "response 'filename' value is required and must be a string value (got %s)", luaState.type(-1).toString() );
								isInvalid = true;
							}
						}
						else
						{
							paramValidationFailure( luaState, "'response' value of params table, if provided, should be a table specifying response location values (got %s)", luaState.type(-1).toString() );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					luaState.getField(paramsTableStackIndex, "timeout");
					if (!luaState.isNil(-1))
					{
						if ( LuaType.NUMBER == luaState.type( -1 ) )
						{
							this.timeout = luaState.toInteger(-1);
							debug("Request timeout provided, was: %d", this.timeout);
						}
						else
						{
							paramValidationFailure( luaState, "'timeout' value of params table, if provided, should be a numeric value (got %s)", luaState.type(-1).toString() );
							isInvalid = true;
						}
					}
					luaState.pop(1);

					luaState.getField(paramsTableStackIndex, "debug");
					if (!luaState.isNil(-1))
					{
						if ( LuaType.BOOLEAN == luaState.type( -1 ) )
						{
							this.isDebug = luaState.toBoolean(-1);
							debug("debug setting provided, was: %b", this.isDebug);
						}
					}
					luaState.pop(1);

					luaState.getField(paramsTableStackIndex, "successNotification");
					if (luaState.isTable(-1))
					{
						this.successNotificationSettings = StatusBarNotificationSettings.from(
								CoronaEnvironment.getApplicationContext(), luaState, -1);
					}
					luaState.pop(1);
				}
				else
				{
					paramValidationFailure( luaState, "Fourth argument to network.request(), if provided, should be a params table (got %s)", luaState.type(-1).toString() );
					isInvalid = true;
				}
			}

			return !isInvalid;
		}

	}

	private class AsyncNetworkRequestRunnable implements Runnable
	{
		private 	NetworkRequestParameters			requestParameters;
		private		CopyOnWriteArrayList<AsyncNetworkRequestRunnable> connectionList;
		public 		NetworkRequestState					requestState;

		public AsyncNetworkRequestRunnable( NetworkRequestParameters requestParameters, CopyOnWriteArrayList<AsyncNetworkRequestRunnable> connectionList )
		{
			this.requestParameters = requestParameters;
			this.connectionList = connectionList;
			// Check for null here instead of in extractRequestParameters is so that we can return an isError to the callback function.
			String url = "";
			if(this.requestParameters.requestURL != null) {
				url = this.requestParameters.requestURL.toString();
			}
			this.requestState = new NetworkRequestState(url, this.requestParameters.isDebug );
		}

		public boolean abort( CoronaRuntime runtime )
		{
			// For now, all we can really do is flag the request as cancelled (so we won't get any more callbacks) and
			// unregister the callback now while we have the chance (when the connection finally completes, the Lua state
			// might be gone)...
			//
			this.requestState.isRequestCancelled.set( true );
			if (this.requestParameters.callback != null)
			{
				this.requestParameters.callback.unregister( runtime );
			}
			return false;
		}

		public void run()
		{

			this.connectionList.add(this);

			long bytesToUpload = 0; // 0 == Definitively no upload body
			long bytesUploaded = 0;
			long bytesToDownload = 0; // 0 == Definitively no download body
			long bytesDownloaded = 0;

			try
			{
				HttpURLConnection urlConnection = null;

				try
				{
					urlConnection = (HttpURLConnection) this.requestParameters.requestURL.openConnection();
					urlConnection.setConnectTimeout( this.requestParameters.timeout * 1000 );
					urlConnection.setReadTimeout( this.requestParameters.timeout * 1000 );

					// fix 'PATCH' request Error
					if (this.requestParameters.method.equals("PATCH") && android.os.Build.VERSION.SDK_INT <= 19) // 19=android.os.Build.VERSION_CODES.KITKAT
					{
						urlConnection.setRequestProperty("X-HTTP-Method-Override", "PATCH");
						urlConnection.setRequestMethod("POST");
						debug("Network request: PATCH replaced on POST", this.requestParameters.method);
					}
					else
					{
						urlConnection.setRequestMethod( this.requestParameters.method ); // GET, POST, PUT, HEAD, DELETE
						debug("Network request: ", this.requestParameters.method);
					}


					// See 301 and 302 handling below
					urlConnection.setInstanceFollowRedirects( false );

					if (this.requestParameters.requestURL.getUserInfo() != null)
					{
						debug("Adding basic auth header");
						String basicAuth = "Basic " + new String(Base64.encodeToString(this.requestParameters.requestURL.getUserInfo().getBytes(), Base64.DEFAULT));
						urlConnection.setRequestProperty("Authorization", basicAuth);
					}

					debug("Opening connection to: %s", this.requestParameters.requestURL);

					String requestBodyCharset = "UTF-8";
					Boolean wasCtHeaderSpecified = false;

					// Set any request headers
					if (this.requestParameters.requestHeaders != null)
					{
						for (Map.Entry<String, String> entry : this.requestParameters.requestHeaders.entrySet())
						{
							String key = entry.getKey();
							String value = entry.getValue();

							if ( "Content-Type".equalsIgnoreCase( key ) )
							{
								debug("Content-Type header value for POST/PUT is: %s", value);

								wasCtHeaderSpecified = true;

								if ( this.requestParameters.bBodyIsText )
								{
									// If the request body is text, we need to record the specified character encoding (if any) from
									// the supplied Content-Type header so we can apply it to the request body later.  If there is no
									// character encoding specified on the Content-Type header, then we need to set it's charset attribute
									// explicitly here (to the default).
									//
									String ctCharset = getContentTypeEncoding( value );
									if ( null != ctCharset )
									{
										try
										{
											Charset.forName(ctCharset);
											requestBodyCharset = ctCharset;
										}
										catch (Exception e)
										{
											// This is bad, caller supplied a charset in the Content-Type header that we don't support - error out.
											//
											throw new Exception("Caller specified an unsupported character encoding in Content-Type charset, was: " + ctCharset);
										}
									}
									else
									{
										value += "; charset=" + requestBodyCharset;
										debug("Adding default charset to Content-Type header: %s", value);
									}
								}
							}

							urlConnection.setRequestProperty(key, value);
						}
					}

					// Write the request body
					//
					if (this.requestParameters.method.equals("POST") || this.requestParameters.method.equals("PUT") || this.requestParameters.method.equals("PATCH"))
					{
						if ( ( null != this.requestParameters.requestBody ) && ( ! wasCtHeaderSpecified ))
						{
							// We check for the presence of a Content-Type request header on param validation whenever a request body
							// is specified, so we should never encounter this case.
							//
							debug("No Content-Type request header was provided for the POST/PUT");
						}

						OutputStream out = null;
						InputStream in = null;

						try
						{
							if ( this.requestParameters.requestBody instanceof String )
							{
								debug("Request body is text from Lua string (requestBodyCharset: %s)", requestBodyCharset);
								String bodyString = (String)this.requestParameters.requestBody;
								in = new ByteArrayInputStream(bodyString.getBytes(requestBodyCharset));
								// Content-Length is in bytes (not characters)
								bytesToUpload = in.available();
							}
							else if ( this.requestParameters.requestBody instanceof byte[] )
							{
								debug("Request body is binary from Lua string");
								byte[] bodyBytes = (byte[])this.requestParameters.requestBody;
								in = new ByteArrayInputStream(bodyBytes);
								bytesToUpload = bodyBytes.length;
							}
							else if ( this.requestParameters.requestBody instanceof CoronaFileSpec )
							{
								debug("Request body is from a file");
								CoronaFileSpec fileSpec = (CoronaFileSpec)this.requestParameters.requestBody;
								in = fileSpec.getInputStream();
								bytesToUpload = fileSpec.getFileSize();
							}

							debug("Request body size: %d", bytesToUpload);

							if ( this.requestParameters.progressDirection == ProgressDirection.UPLOAD )
							{
								this.requestState.phase = "began";
								this.requestState.bytesEstimated = bytesToUpload;
								if (this.requestParameters.callback != null)
								{
									this.requestParameters.callback.call( this.requestState );
								}
							}

							this.requestState.phase = "progress";

							if ( null != in )
							{
								urlConnection.setDoOutput(true);

		 						// Call either setFixedLengthStreamingMode if size is known, or setChunkedStreamingMode
		 						// if size is not known, otherwise the urlconnection will buffer the complete request
		 						// body in memory.
								//
								if ((bytesToUpload > 0) && (bytesToUpload < Integer.MAX_VALUE))
								{
									urlConnection.setFixedLengthStreamingMode((int)bytesToUpload);
								}
								else
								{
									// Unknown or greater than max int.
									//
									// Use the default chunk size (recommended)
									//
									urlConnection.setChunkedStreamingMode(0);
								}

								in = new BufferedInputStream( in );
								out = new BufferedOutputStream(urlConnection.getOutputStream());

								final int sendBufferSize = 1024;

								byte[] buffer = new byte[sendBufferSize];
								int bufferLength = 0; //used to store a temporary size of the buffer

								// Stream the request body (input) to the server (output)
								//
								while (!this.requestState.isRequestCancelled.get() && ((bufferLength = in.read(buffer)) > 0))
								{
									out.write(buffer, 0, bufferLength);

									//add up the size so we know how much is uploaded
									bytesUploaded += bufferLength;

									if ( this.requestParameters.progressDirection == ProgressDirection.UPLOAD )
									{
										this.requestState.bytesTransferred = bytesUploaded;
										if (this.requestParameters.callback != null)
										{
											this.requestParameters.callback.call( this.requestState );
										}
									}
								}
							}
							else
							{
								debug("No request body supplied");
							}
						}
						finally
						{
							if ( null != in )
							{
								in.close();
							}
							if ( null != out )
							{
								out.close();
							}
						}
					}
					else
					{
						// No request body to upload, but upload status requested.  We will send this anyway, since this way
						// you will at least see that the bytesToUpload is zero, and know that no more progress is coming...
						//
						if ( this.requestParameters.progressDirection == ProgressDirection.UPLOAD )
						{
							this.requestState.phase = "began";
							if (this.requestParameters.callback != null)
							{
								this.requestParameters.callback.call( this.requestState );
							}
						}
					}

					// Now we need to inspect the response code and response body (via either input or error streams) to determine if there
					// actually is a response body...

					debug("Waiting for response code (%s)", urlConnection.getURL().toString());
					int responseCode = urlConnection.getResponseCode();
					debug("Got response code %d (%s)", responseCode, urlConnection.getURL().toString());

					// Handle redirects ourselves (unless the caller opts to handle them theirselves)
					// since the HttpURLConnection class doesn't do so properly on some
					// Android forks (e.g. Galaxy S3, HTC One)
					// (it messes up content-lengths, cookies and probably other things)
					// It also gives us the opportunity to go ahead and redirect to URLs with a
					// different method (e.g. POST -> GET) which is against the RFC rules but
					// is the behavior the ordinary programmer is going to expect
					// See: https://tools.ietf.org/html/rfc2616#section-10.3.3
					int numRedirects = 0;
					final int maxRedirects = 10;

					while (this.requestParameters.bHandleRedirects &&
							(responseCode == 301 || responseCode == 302 || responseCode == 303 || responseCode == 307))
					{
						String locHeader = urlConnection.getHeaderField("Location"); // get the location
						String origURL = urlConnection.getURL().toString();
						List<String> cookies = urlConnection.getHeaderFields().get("Set-Cookie");

						if (locHeader == null || locHeader.equals(""))
						{
							throw new RuntimeException( String.format( "no Location: header in %d redirect response from %s", responseCode, origURL ) );
						}

						++numRedirects;

						if (numRedirects == maxRedirects)
						{
							throw new RuntimeException( String.format( "more than maximum number of redirects attempted (%d) (%s -> %s)", maxRedirects, origURL, locHeader ) );
						}

						if (origURL.substring(5).equalsIgnoreCase("https") && locHeader.substring(5).equalsIgnoreCase("http:"))
						{
							System.out.println("WARNING: " + String.format("redirecting from HTTPS to HTTP (%s -> %s)", origURL, locHeader));
						}

						URL url = new URL(urlConnection.getURL(), locHeader);

						debug("Handling %d redirect to: %s", responseCode, locHeader);

						// Make a new HttpURLConnection instance
						// As the cURL guys say: Violate RFC 2616/10.3.3 and switch from POST to GET
						urlConnection = (HttpURLConnection) url.openConnection();
						urlConnection.setConnectTimeout( this.requestParameters.timeout * 1000 );
						urlConnection.setReadTimeout( this.requestParameters.timeout * 1000 );
						urlConnection.setInstanceFollowRedirects( false );

						// Reflect all cookies
						// Must be done as a single header or sadness ensues
						if (cookies != null)
						{
							String cookieHeader = "";

							for (String cookie : cookies)
							{
								debug("=== set cookie: %s ('%s')", cookie, cookie.split(";")[0]);
								// the split is there to get rid of cookie attributes which
								// are not needed by the server side like expires, path, etc.
								if (cookieHeader.length() > 0)
								{
									cookieHeader += "; ";
								}
								cookieHeader += cookie.split(";")[0];
							}
							debug("=== set Cookie: %s", cookieHeader);
							urlConnection.addRequestProperty("Cookie", cookieHeader);
						}

						// TODO: we need to change all 4 platforms at the same time do this
						// Put the new location in the networkRequest event
						// this.requestState.url = urlConnection.getURL().toString();

						debug("Waiting for response code (%s)", urlConnection.getURL().toString());
						responseCode = urlConnection.getResponseCode();
						debug("Got response code %d (%s)", responseCode, urlConnection.getURL().toString());
					}

					Boolean isErrorStream = false;
					InputStream is = null;

					if ( responseCode >= 200 && responseCode < 300 )
					{
						is = urlConnection.getInputStream();
						if ( null != is )
						{
							// If we have an input stream, lets make sure that there's some data on it (some "success" responses do not
							// actually have any response body, and so they provide an "empty" input stream).
							//
							is = new PushbackInputStream(is);
							int b = is.read();

							if ( -1 != b )
							{
								((PushbackInputStream)is).unread( b );
							}
							else
							{
								is.close();
								is = null;
							}
						}
					}
					else
					{
						is = urlConnection.getErrorStream();
						if ( null != is )
						{
							isErrorStream = true;
						}
					}

					requestState.status = responseCode;
					requestState.responseHeaders = urlConnection.getHeaderFields();

					if ( null != is )
					{
						bytesToDownload = urlConnection.getContentLength(); // Fairly unreliable, will often be -1 (length unknown, usually chunked transfer)
					}

					if ( this.requestParameters.progressDirection == ProgressDirection.DOWNLOAD )
					{
						// We're going to send this whether or not there is a download, since you will at least see that the bytesToDownload
						// is zero, and know that no more progress is coming...
						//
						this.requestState.phase = "began";
						this.requestState.bytesEstimated = bytesToDownload;
						if (this.requestParameters.callback != null)
						{
							this.requestParameters.callback.call( this.requestState );
						}
					}

					// If we have a valid response body (even if it's an error response), then we process the "download"...
					//
					if ( null != is )
					{
						this.requestState.phase = "progress";

						// If we get an encoding from the Content-Type header, then we will treat the content as text and
						// decode it using that encoding.  If not, then we need to determine whether the Content-Type itself
						// implies text, and if so, assign a default encoding.  In that case, we will also attempt to
						// determine whether the encoding is contained in the content (as an xml or html meta tag).
						//
						String contentType = urlConnection.getContentType();
						if (contentType == null) {
							contentType = "";
						}

						String charset = NetworkRequest.getContentTypeEncoding(contentType);
						if ( null != charset )
						{
							debug("Writing protocol charset debug");
							this.requestState.setDebugValue("charset", charset);
							this.requestState.setDebugValue("charsetSource", "protocol");

							// Validate the character encoding we got from the Content-Type header...
							//
							try
							{
								Charset.forName(charset);
							}
							catch (Exception e)
							{
								debug("The character encoding found in the Content-Type header was not supported, was: " + charset);
								charset = null;
							}
						}

						if ( null == charset )
						{
							if (NetworkRequest.isContentTypeText(contentType))
							{
								// We need to get a "preview" of the content (enough to get the headers) in order to scan it for
								// indicators of encoding.  We need to then push this back so the readers below will get the full
								// stream to decode.
								//
								final byte[] previewBuffer = new byte[1024];

								is = new PushbackInputStream(is, previewBuffer.length);

								int previewLength = is.read(previewBuffer);
								if (previewLength > 0)
								{
									String preview = new String(previewBuffer, 0, previewLength, "usascii");

									String contentCharset = NetworkRequest.getEncodingFromContent(contentType, preview);
									if (contentCharset != null)
									{
										// A content encoding was found in the content itself!
										//
										debug("Writing content charset debug");
										this.requestState.setDebugValue("charset", contentCharset);
										this.requestState.setDebugValue("charsetSource", "content");

										// Validate the character encoding we got from the content...
										//
										try
										{
											Charset.forName(contentCharset);
											charset = contentCharset;
										}
										catch (Exception e)
										{
											charset = "UTF-8";
											debug("The character encoding found in the content itself was not supported, was: " + contentCharset + ", content will be decoded using UTF-8");
										}
									}
									((PushbackInputStream)is).unread(previewBuffer, 0, previewLength);
								}

								if (charset == null)
								{
									// If it is a "text" Content-Type and no implicit charset was found, then we're going to
									// assume a default encoding of UTF-8.  Most text types have an implied default encoding
									// of UTF-8, and the others tend to default to USASCII (which will decode properly with
									// UTF-8), so we just default to UTF-8.
									//
									charset = "UTF-8";

									debug("Writing implicit charset debug");
									this.requestState.setDebugValue("charset", charset);
									this.requestState.setDebugValue("charsetSource", "implicit");
								}
							}

							if ( isErrorStream && ( null == charset ))
							{
								// Error stream should always be text of some kind, and should never overwrite a file, so we'll
								// force an encoding here if one wasn't already found...
								//
								charset = "UTF-8";
							}
						}

						final int bufferSize = 1024;

						// If requestParameters.response then response should go to indicated file, otherwise to string/byte[].
						//
						// However, if requestParameters.response was indicated and isErrorStream, then go ahead and just write the response
						// to response, as opposed to the file indicated (to avoid writing/overwriting file with error response).
						//

						if (( null != charset ) && (( null == this.requestParameters.response )  || isErrorStream))
						{
							// If the content is text and we're NOT writing it to a file...
							//
							debug("Response content was text, to be decoded with: %s", charset);
							requestState.responseType = "text";

							Reader in = null;
							Writer out = null;

							try
							{
								in = new BufferedReader(new InputStreamReader(is, charset));
								out = new StringWriter();

								// Create a buffer...
								final char[] buffer = new char[bufferSize];
								int bufferLength = 0;

								// Now, read through the input buffer and write the contents to the output
								while (!this.requestState.isRequestCancelled.get() && ((bufferLength = in.read(buffer)) > 0))
								{
									out.write(buffer, 0, bufferLength);
									bytesDownloaded += bufferLength;

									if ( this.requestParameters.progressDirection == ProgressDirection.DOWNLOAD )
									{
										this.requestState.bytesTransferred = bytesDownloaded;
										if (this.requestParameters.callback != null)
										{
											this.requestParameters.callback.call( this.requestState );
										}
									}
								}

								if ( out instanceof StringWriter )
								{
									this.requestState.response = ((StringWriter)out).toString();
								}
							}
							finally
							{
								if (in != null)
								{
									in.close();
								}
								if (out != null)
								{
									out.close();
								}
							}
						}
						else
						{
							debug("Response content was binary");
							requestState.responseType = "binary";

							InputStream in = null;
							OutputStream out = null;

							File tempFile = null;

							if ( ( this.requestParameters.response == null) || isErrorStream )
							{
								out = new ByteArrayOutputStream();
							}
							else
							{
								CoronaFileSpec fileSpec = (CoronaFileSpec)this.requestParameters.response;


								// We will stream to a temp file, and upon successfull completion, rename (with overwrite)
								// the temp file to the specified file, and only then set response to the provided fileSpec.
								//

								//Let's feed the same volume/folder in for the temp path to be sure it's in the final
								//destination folder


								File dstFolder = null;
								if (null != fileSpec.fullPath)
								{

									int lastIndex = fileSpec.fullPath.lastIndexOf(File.separator);
									if (lastIndex > 0)
									{
										String destPath = fileSpec.fullPath.substring(0,lastIndex);
										if (null != destPath)
										{
											dstFolder = new File(destPath);
											dstFolder.mkdirs();
										}
									}
								}

								tempFile = File.createTempFile("download", ".tmp", dstFolder); // Create uniquely named temp file (in cache directory on Android)
								debug("Temp file path: %s", tempFile.getPath());
								out = new FileOutputStream( tempFile );

							}

							try
							{
								in = new BufferedInputStream(is);

								// Create a buffer...
								//
								byte[] buffer = new byte[bufferSize];
								int bufferLength = 0; //used to store a temporary size of the buffer

								// This is our poor-mans request cancelling - just continue to wait on the read and bail
								// when it returns.

								// Now, read through the input buffer and write the contents to the output
								//
								while (!this.requestState.isRequestCancelled.get() && ((bufferLength = in.read(buffer)) > 0))
								{
									out.write(buffer, 0, bufferLength);
									bytesDownloaded += bufferLength;

									if ( this.requestParameters.progressDirection == ProgressDirection.DOWNLOAD )
									{
										this.requestState.bytesTransferred = bytesDownloaded;
										if (this.requestParameters.callback != null)
										{
											this.requestParameters.callback.call( this.requestState );
										}
									}
								}

								if ( out instanceof ByteArrayOutputStream )
								{
									out.close();
									this.requestState.response = ((ByteArrayOutputStream)out).toByteArray();
									out = null;
								}
								else if ( out instanceof FileOutputStream )
								{
									out.close();
									out = null;

									if (!this.requestState.isRequestCancelled.get())
									{
										// Successfull completion.  Now rename (with overwrite) the temp file to the specified file, and
										// only then set response to the provided fileSpec.
										//
										CoronaFileSpec fileSpec = (CoronaFileSpec)this.requestParameters.response;

										if ( null != tempFile )
										{
											File destFile = new File( fileSpec.fullPath );
											if ( destFile.exists() )
											{
												destFile.delete();
											}
											boolean bRenamed = tempFile.renameTo( destFile );
											if (bRenamed)
											{
												debug("Temp file successfully renamed");
											}
											else
											{
												// Failure to rename temp file to desired output file.  Error out...
												//
												throw new Exception("Failed to renamed temporary download file at path " + tempFile.getPath() + " to final file at path " + destFile.getPath());
											}

											this.requestState.response = fileSpec;
										}
									}
									else
									{
										// Cancelled, let's just delete the temp file...
										//
										if ( null != tempFile )
										{
											tempFile.delete();
										}
									}

									tempFile = null;
								}
							}
							finally
							{
								if ( null != in )
								{
									in.close();
								}
								if ( null != out )
								{
									out.close();
								}
								if ( null != tempFile )
								{
									tempFile.delete();
								}
							}
						}
					}
				}
				finally
				{
					if ( null != urlConnection )
					{
						urlConnection.disconnect();
					}
				}
			}
			catch (Exception e)
			{
				String message = e.getMessage();
				if ( null == message )
				{
					if (e instanceof java.net.SocketTimeoutException)
					{
						message = "Request timed out";
					}
					else if (e.getCause() != null)
					{
						message = e.getCause().toString();
					}
					else
					{
						message = "";
					}
				}
				boolean bReportError = true;
				if (this.requestParameters != null)
				{
					if (this.requestParameters.requestURL != null)
					{
						String urlRequest = this.requestParameters.requestURL.toString();
						if (urlRequest.startsWith("https://stats.coronalabs.com") ||
							urlRequest.startsWith("https://monetize-api.coronalabs.com") ||
							urlRequest.startsWith("https://api.intercom.io"))
						{
							// Don't report errors for our own analytics
							bReportError = false;
						}
						else if (message.startsWith(urlRequest))
						{
							// If we get an error message that just consists of the URL, elaborate
							message = "Invalid URL: " + message;
						}
						else
						{
							message += ": " + urlRequest;
						}
					}
				}
				if (bReportError)
				{
					error(message + " (" + e.getClass().getCanonicalName() + ")");
				}

				debug("Exception during request: %s", message);
				this.requestState.isError = true;
				this.requestState.response = message;
				this.requestState.setDebugValue("errorMessage", message);
			}

			// On an empty response body, we set it to a text response with an empty string to be consistent
			// with what is done on the other platforms.
			//
			if ( ( !this.requestState.isError ) && ( null == this.requestState.response ) )
			{
				this.requestState.response = "";
				this.requestState.responseType = "text";
			}

			// Post a success notification to the status bar, if configured.
			if ((this.requestState.isError == false) &&
			    ((this.requestState.status >= 200) && (this.requestState.status < 300)) &&
			    !this.requestState.isRequestCancelled.get()
			   )
			{
				if (this.requestParameters.successNotificationSettings != null)
				{
					NotificationServices notificationServices;
					notificationServices = new NotificationServices(CoronaEnvironment.getApplicationContext());
					StatusBarNotificationSettings statusBarSettings = this.requestParameters.successNotificationSettings;
					statusBarSettings.setId(notificationServices.reserveId());
					statusBarSettings.setTimestamp(new java.util.Date());
					notificationServices.post(statusBarSettings);
				}
			}

			this.requestState.phase = "ended";
			if ( this.requestParameters.progressDirection == ProgressDirection.NONE )
			{
				// If no progress was specified, set the bytesEstimated/bytesTransfered to values for the
				// downloaded content (if any).
				//
				this.requestState.bytesTransferred = bytesDownloaded;
				this.requestState.bytesEstimated = bytesToDownload;
			}
			if (this.requestParameters.callback != null)
			{
				this.requestParameters.callback.call( this.requestState, true );
			}

			this.connectionList.remove(this);
		}
	}

	/**
	 * Gets the name of the Lua function as it would appear in the Lua script.
	 * @return Returns the name of the custom Lua function.
	 */
	@Override
	public String getName()
	{
		return "request_native";
	}

	/**
	 * This method is called when the Lua function is called.
	 * <p>
	 * Warning! This method is not called on the main UI thread.
	 * @param luaState Reference to the Lua state.
	 *                 Needed to retrieve the Lua function's parameters and to return values back to Lua.
	 * @return Returns the number of values to be returned by the Lua function.
	 */
	@Override
	public int invoke ( LuaState luaState )
	{
		// Throw an exception if this application does not have the following permission.
		android.content.Context context = com.ansca.corona.CoronaEnvironment.getApplicationContext();
		if (context != null) {
			context.enforceCallingOrSelfPermission(android.Manifest.permission.INTERNET, null);
		}

		debug("network.request() - thread: %d", Thread.currentThread().getId());
		debug("network.request() - luaState %s", Integer.toHexString(System.identityHashCode(luaState)));
		//
		// network.request( url, method, listener [, params] )
		//
		//     params.headers
		//     params.body
		//     params.bodyType
		//     params.successNotification
		//     params.progress
		//     params.response
		//     params.timeout
		//
		//     listener( event )
		//
		//    	   event.name = "networkRequest"
		//         event.phase
		//    	   event.url
		//         event.requestId
		//    	   event.isError
		//    	   event.status
		//    	   event.responseHeaders
		//         event.responseType
		//         event.response
		//         event.bytesTransferred
		//         event.bytesEstimated
		//
		final NetworkRequestParameters requestParameters = new NetworkRequestParameters();
		if (requestParameters.extractRequestParameters(luaState))
		{
			AsyncNetworkRequestRunnable request;
			request = new AsyncNetworkRequestRunnable( requestParameters, this.fOpenRequests );
			AtomicBoolean isCancelled = request.requestState.isRequestCancelled;

			Thread t = new Thread(request);
			t.start();

			// Return the "requestId" object, which happens to be the AtomicBoolean to cancel the request
			//
			luaState.pushJavaObjectRaw(isCancelled);

			return 1;
		}
		else
		{
			// Parameter validation failure, return nothing to indicate failure....
			//
			return 0;
		}

	}
}
