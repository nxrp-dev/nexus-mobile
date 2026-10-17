//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#import "AppCoronaDelegate.h"

#import "CoronaRuntime.h"
#import "CoronaLua.h"

@implementation AppCoronaDelegate

- (void)willLoadMain:(id<CoronaRuntime>)runtime
{
	// Load our Lua helper support file
	//
//	NSString *fullPath = [[NSBundle mainBundle] pathForResource:@"helper.lua" ofType:nil inDirectory:nil];
//	Corona::Lua::DoFile( runtime.L, [fullPath UTF8String], 0, false);
}

- (void)didLoadMain:(id<CoronaRuntime>)runtime
{
}

@end
