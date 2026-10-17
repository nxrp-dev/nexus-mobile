//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "WinTimer.h"

#include "WindowsNetworkSupport.h"

// Initialized static map...
std::map<UINT_PTR, WinTimer*> WinTimer::fTimerMap;

// ----------------------------------------------------------------------------

// Called by Windows when the system timer has elapsed.
// Calls WinTimer's Evaluate() function to see if it is time to invoke its callback.
//
VOID CALLBACK WinTimer::OnTimerElapsed(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	WinTimer *timer = fTimerMap[idEvent];
    if (!timer)
        return; // FAIL!

	timer->Evaluate();
}

WinTimer::WinTimer( ) 
{
	debug("WinTimer::WinTimer - thread ID: %d", GetCurrentThreadId());

	fIntervalInMilliseconds = 10;
	fNextIntervalTimeInTicks = 0;
	fTimer = NULL;
}

WinTimer::~WinTimer()
{
	Stop();
}

// Starts the timer.
void
WinTimer::Start()
{
	// Do not continue if the timer is already running.
	if (IsRunning())
	{
		return;
	}

	// Start the timer, but with an interval faster than the configured interval.
	// We do this because Windows timers can invoke later than expected.
	// To compensate, we'll schedule when to invoke the timer's callback using "fIntervalEndTimeInTicks".
	fNextIntervalTimeInTicks = ::GetTickCount() + fIntervalInMilliseconds;
	fTimer = ::SetTimer(NULL, 0, 10, OnTimerElapsed);
    fTimerMap[fTimer] = this;
}

// Stops the timer.
void
WinTimer::Stop()
{
	// Do not continue if the timer has already been stopped.
	if (IsRunning() == false)
	{
		return;
	}

	// Stop the timer.
	if (fTimer != 0)
    {
        ::KillTimer(NULL, fTimer);
 
        if (fTimerMap.size() > 0)
            fTimerMap[fTimer] = NULL;
 
        fTimer = 0;
    }
}

// Sets the timer's interval in milliseconds. This can be applied while the timer is running.
// The interval cannot be set less than 10 milliseconds.
void
WinTimer::SetInterval( DWORD milliseconds )
{
	fIntervalInMilliseconds = milliseconds;
}

// Returns true if the timer is currently running.
bool
WinTimer::IsRunning() const
{
	return fTimer != NULL;
}

// Checks if the running timer's interval has elapsed, and if it has, invokes its callback.
// This function is provided because a Windows system timer can trigger late.
// This function will not do anything if the timer is not running.
void
WinTimer::Evaluate()
{
	// Do not continue if the if we haven't reached the scheduled time yet.
	if (CompareTicks(::GetTickCount(), fNextIntervalTimeInTicks) < 0)
	{
		return;
	}

	// Schedule the next interval time.
	for (; CompareTicks(::GetTickCount(), fNextIntervalTimeInTicks) > 0; fNextIntervalTimeInTicks += fIntervalInMilliseconds);

	// Invoke this timer's callback.
	OnTimer();
}


// Computes the delta in milliseconds between two times in milliseconds as returned by
// ::GetTickCount() and represented as DWORD values.  The delta will be negative if x is
// before y, or positive if x is after y.
//
// This logic allows for the time values to wrap, such that you may increment or decrement 
// any such time value, wrapping in either direction, or you may pass in a ::GetTickCount()
// value that has wrapped, and this logic will accommodate that, so long as the time interval
// between x and y is not greater than 2^30 milliseconds (about 12.5 days).
// 
long
WinTimer::GetTickDelta(DWORD x, DWORD y)
{
	// Inspired by http://stackoverflow.com/questions/727918/what-happens-when-gettickcount-wraps
	//
	// Test vectors used:
	//
	// x = 13487231,   y = 13492843,   delta = -5612
	// x = 13492843,   y = 13487231,   delta =  5612
	// x = 4294967173, y = 1111,       delta = -1234
	// x = 1111,       y = 4294967173, delta =  1234
	// x = 0x7fffffff, y = 0x80000000, delta =    -1
	// x = 0x80000000, y = 0x7fffffff, delta =     1
	// x = 0x80000000, y = 0x80000001, delta =    -1
	// x = 0x80000001, y = 0x80000000, delta =     1
	// x = 0x7fffffff, y = 0x80000001, delta =    -2
	// x = 0xffffffff, y = 0x00000001, delta =    -2
	// x = 0x00000000, y = 0xffffffff, delta =     1
	//

	// Compare the given tick values via subtraction (relies on specified behavior of DWORD 
	// and long wrapping, including wrapping on subraction).
	//
	long deltaTime = (long)x - (long)y;
	return deltaTime;
}

// Compares the given tick values returned by ::GetTickCount() using GetTickDelta() (above).
//
// Returns a positive value if "x" is greater than "y".
// Returns zero if "x" is equal to "y".
// Returns a negative value if "x" is less than "y".
//
int
WinTimer::CompareTicks(DWORD x, DWORD y)
{
	long deltaTime = WinTimer::GetTickDelta(x, y);
	if (deltaTime < 0)
	{
		return -1;
	}
	else if (0 == deltaTime)
	{
		return 0;
	}
	return 1;
}

// ----------------------------------------------------------------------------

