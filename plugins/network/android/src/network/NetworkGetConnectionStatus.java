//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////
package network;

import com.ansca.corona.CoronaEnvironment;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

/**
 * Implements the network cancel() function in Lua.
 */
public class NetworkGetConnectionStatus implements com.naef.jnlua.NamedJavaFunction 
{
	private LuaLoader fLoader;

	public NetworkGetConnectionStatus(LuaLoader loader)
	{
		this.fLoader = loader;
	}

	/**
	 * Gets the name of the Lua function as it would appear in the Lua script.
	 * @return Returns the name of the custom Lua function.
	 */
	@Override
	public String getName() 
	{
		return "getConnectionStatus";
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
	public int invoke(com.naef.jnlua.LuaState luaState) 
	{
		boolean isConnected = false;
		boolean isMobile = false;

		// Fetch the application context.
		android.content.Context context = com.ansca.corona.CoronaEnvironment.getApplicationContext();

		// Throw an exception if this application does not have the following permission.
		context.enforceCallingOrSelfPermission(android.Manifest.permission.ACCESS_NETWORK_STATE, null);

		// Fetch the device's network status.
		ConnectivityManager cm = (ConnectivityManager)context.getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
		if ( null != activeNetwork )
		{
			isConnected = activeNetwork.isConnectedOrConnecting();
			if ( isConnected )
			{
				isMobile = activeNetwork.getType() != ConnectivityManager.TYPE_WIFI;
			}
		}
		NetworkRequest.debug("Is connected: %b, is mobile: %b", isConnected, isMobile);

		// Push the network status results back to Lua.
		luaState.newTable(0, 2);
		int luaTableStackIndex = luaState.getTop();
		luaState.pushBoolean(isConnected);
		luaState.setField(luaTableStackIndex, "isConnected");
		luaState.pushBoolean(isMobile);
		luaState.setField(luaTableStackIndex, "isMobile");
		return 1;
	}
}