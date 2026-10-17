//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#import <Foundation/Foundation.h>

#import "CoronaRuntime.h"

#import "AppleNetworkSupport.h"

// ----------------------------------------------------------------------------

@interface CoronaURLRequest : NSMutableURLRequest

-(id)initWithNetworkRequestParameters:(NetworkRequestParameters*) requestParams error:(NSError**) error;

@end

// ----------------------------------------------------------------------------

@interface CoronaURLConnection : NSURLConnection

@property (nonatomic, retain) NetworkRequestState* fNetworkRequestState;
@property (nonatomic, retain) ConnectionManager* fConnectionManager;
@property (nonatomic, retain) id fDelegate;

-(id)initWithRequest:(CoronaURLRequest *)request networkRequestParameters:(NetworkRequestParameters*)requestParams connectionManager:(ConnectionManager*) connectionManager;

-(void)invalidate;
-(void)cancel;
-(void)end;
-(bool)reportErrors;

@end

// ----------------------------------------------------------------------------
