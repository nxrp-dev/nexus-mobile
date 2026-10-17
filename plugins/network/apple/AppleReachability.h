//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _AppleReachability_H__
#define _AppleReachability_H__

#include "PlatformReachability.h"


// ----------------------------------------------------------------------------

struct lua_State;

@class DdgReachability;
@class AppleReachabilityCallbackDelegate;

class Runtime;

// ----------------------------------------------------------------------------


class AppleReachability : public PlatformReachability
{
public:
	explicit AppleReachability( lua_State* L, PlatformReachabilityType type, const char* address );
	
	virtual ~AppleReachability();

	virtual bool IsValid() const;

	virtual bool IsReachable() const;
	
	virtual bool IsConnectionRequired() const;
	
	virtual bool IsConnectionOnDemand() const;
	
	virtual bool IsInteractionRequired() const;
	
	virtual bool IsReachableViaCellular() const;
	
	virtual bool IsReachableViaWiFi() const;

protected:

	DdgReachability* networkReachability;
	AppleReachabilityCallbackDelegate* reachabilityCallbackDelegate;
};

// ----------------------------------------------------------------------------

#endif // _AppleReachability_H__
