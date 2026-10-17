//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// This corresponds to the name of the Lua library,
// e.g. [Lua] require "plugin.library"
package network;

import java.io.*;

import android.app.Activity;
import com.naef.jnlua.LuaState;
import com.naef.jnlua.JavaFunction;
import com.naef.jnlua.NamedJavaFunction;
import com.ansca.corona.CoronaActivity;
import com.ansca.corona.CoronaEnvironment;
import com.ansca.corona.CoronaLua;
import com.ansca.corona.CoronaRuntime;
import com.ansca.corona.CoronaRuntimeListener;
import com.ansca.corona.CoronaRuntimeTaskDispatcher;


public class LuaLoader implements JavaFunction, CoronaRuntimeListener {

	// This corresponds to the event name, e.g. [Lua] event.name
	private static final String EVENT_NAME = "networkLibraryEvent";

	// Performs an async network request when Lua functions network.request(), network.download(),
	// and network.upload() are called.
	private NetworkRequest fNetworkRequest = null;

	// Reference to the last active Corona runtime task dispatcher.
	private CoronaRuntimeTaskDispatcher fRuntimeTaskDispatcher = null;


	/**
	 * Creates a new object for displaying banner ads on the CoronaActivity
	 */
	public LuaLoader( ) 
	{
	}

	/**
	 * Warning! This method is not called on the main UI thread.
	 */
	@Override
	public int invoke( LuaState L ) 
	{
		NetworkRequest.debug("LuaLoader invoke - luaState: %s", Integer.toHexString(System.identityHashCode(L)));

		this.fNetworkRequest = new NetworkRequest(this);

		// Add functions to library
		NamedJavaFunction[] luaFunctions = new NamedJavaFunction[] {
			this.fNetworkRequest,
			new NetworkCancel(this),
			new NetworkGetConnectionStatus(this),
		};

		String libName = L.toString( 1 );
		L.register(libName, luaFunctions);

		// Load our Lua helper code (must be done after plugin loaded, since we are adding to the "network" module)
		//
		LuaHelper.loadLuaHelper(L);

		return 1;
	}

	// CoronaRuntimeListener
	@Override
	public void onLoaded( CoronaRuntime runtime ) 
	{
		// Create a new task dispatcher every time a new Corona runtime has been created.
		// New network operation will use this dispatcher to call Lua listeners.
		// Currently active network operations using the last runtime's dispatcher will no-op.
		fRuntimeTaskDispatcher = new CoronaRuntimeTaskDispatcher(runtime);

		LuaState luaState = runtime.getLuaState();
		NetworkRequest.debug("network plugin onLoaded - JNLUA version is: " + luaState.VERSION);
		NetworkRequest.debug("LuaLoader onLoaded - luaState: %s", Integer.toHexString(System.identityHashCode(luaState)));

		// Connection pooling was causing issues with timeouts.  If a request was made with given timeout, then disconnected,
		// releasing the connection back to the pool, then another request was made to the same host, it would re-use the
		// existing connection with the original timeouts.  Setting the connection or read timeouts on the HttpUrlConnection
		// has no effect in this case.  Turning off connection pooling as below remedies this issue.
		//
		// Also, connection pooling was very buggy prior to Froyo (connections got poisoned and returned to the pool in a
		// variety of cases).  Per the Android gods:
		//
		//    http://android-developers.blogspot.com/2011/09/androids-http-clients.html
		//
		// we would need the following workaround if we *did* want to turn on connection pooling:
		//
		//    private void disableConnectionReuseIfNecessary() {
		//        // HTTP connection reuse which was buggy pre-froyo
		//        if (Integer.parseInt(Build.VERSION.SDK) < Build.VERSION_CODES.FROYO) {
		//            System.setProperty("http.keepAlive", "false");
		//        }
		//    } 
		//
		// I did research whether there might be a way to turn off connection pooling for a given connection, so we could
		// parameterize that into our API for our unit tests and for people who cared more about timeout accuracy than
		// connection optimization (and that would also give us a run-time workaround if we ran in to the bug above).
		// Unfortunately, I could not find such a workaround.  You can indicate on a per-connection basis that you don't
		// want that connection reused when you are done with it, but there is no way to indicate on a per-connection
		// basis that you want the new connection not to come from the pool (AFAICT).
		//
		// There are also still some reports of people seeing the connection pooling bugs on later Android releases:
		//
		//    http://code.google.com/p/android/issues/detail?id=7786
		//
		// So I think any decision to turn connection pooling on should be considered carefully.
		//
		System.setProperty("http.keepAlive", "false");
	}

	// CoronaRuntimeListener
	@Override
	public void onStarted( CoronaRuntime runtime ) 
	{
		NetworkRequest.debug("onStarted");
	}

	// CoronaRuntimeListener
	@Override
	public void onSuspended( CoronaRuntime runtime )
	{
		NetworkRequest.debug("onSuspended");
	}

	// CoronaRuntimeListener
	@Override
	public void onResumed( CoronaRuntime runtime ) 
	{
		NetworkRequest.debug("onResumed");
	}

	// CoronaRuntimeListener
	@Override
	public void onExiting( CoronaRuntime runtime ) 
	{
		NetworkRequest.debug("onExiting");
		this.fNetworkRequest.abortOpenConnections( runtime );
	}

	/**
	 * Posts the given Runnable object to the main UI thread's message queue in a thread safe manner.
	 * @param runnable The Runnable object to be posted and executed on the main UI thread.
	 * @return Returns true if the Runnable object was successfully posted. Returns false if not.
	 */
	public boolean postOnUiThread(Runnable runnable) 
	{
		// Validate.
		if ( null == runnable ) 
		{
			return false;
		}
		
		// Fetch the activity's Handler needed to post the runnable object.
		CoronaActivity activity = CoronaEnvironment.getCoronaActivity();
		if ( null == activity )
		{
			// This happens during applicationExit (onExiting)
			return false;
		}

		android.os.Handler handler = activity.getHandler();
		if ( null == handler ) 
		{
			return false;
		}
		
		// Post the runnable object onto the UI thread's message queue.
		return handler.post(runnable);
	}

	/**
	 * Gets a task dispatcher for the last active Corona runtime.
	 * @return Returns a task dispatcher for the created Corona runtime.
	 *         <p>
	 *         Returns null if a Corona runtime and its activity has not been created yet.
	 */
	public CoronaRuntimeTaskDispatcher getRuntimeTaskDispatcher() {
		return fRuntimeTaskDispatcher;
	}
}