//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PlatformReachability_H__
#define _PlatformReachability_H__

#import <Foundation/Foundation.h>

#import "CoronaRuntime.h"
#import "CoronaLua.h"

// ----------------------------------------------------------------------------

struct lua_State;
	
// ----------------------------------------------------------------------------
	
class PlatformReachability
{
	public:

		enum PlatformReachabilityType
		{
			kReachabilityTypeUndefined = 0,
			kHostName,
			kAddress,
			kInternet,
			kLocalWiFi,
			kNumReachabilityTypes
		};
		static const char kReachabilityListenerEvent[];
		
		explicit PlatformReachability( lua_State* L, PlatformReachabilityType type, const char* address );

		virtual ~PlatformReachability();
		
		// Intended for internal use to decide if the constructor failed since we don't do exception handling.
		virtual bool IsValid() const;
	
		virtual bool IsReachable() const;
		
		virtual bool IsConnectionRequired() const;
		
		virtual bool IsConnectionOnDemand() const;
		
		virtual bool IsInteractionRequired() const;

		virtual bool IsReachableViaCellular() const;

		virtual bool IsReachableViaWiFi() const;
    
		void SetListener( lua_State* L, CoronaLuaRef ref );

		virtual void InvokeCallback() const;

	protected:
	
		lua_State *	 fLuaState;
		CoronaLuaRef fListenerRef;
		NSString *   fAddress;
};

// ----------------------------------------------------------------------------
	
#endif // _PlatformReachability_H__
