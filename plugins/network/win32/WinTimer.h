//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __WinTimer_H__
#define __WinTimer_H__

#include <windows.h>
#include <map>

// ----------------------------------------------------------------------------

class WinTimer
{
	public:
		WinTimer( );
		virtual ~WinTimer();

	public:
		virtual void Start();
		virtual void Stop();
		virtual void SetInterval( ULONG milliseconds );
		virtual bool IsRunning() const;
		virtual void Evaluate();
		virtual void OnTimer() = 0; // Pure virtual, derived class must implement

	public:
		static long GetTickDelta(DWORD x, DWORD y);
		static int CompareTicks(DWORD x, DWORD y);

	private:
		UINT_PTR	fTimer;
		DWORD		fIntervalInMilliseconds;
		DWORD		fNextIntervalTimeInTicks;

		// Map allowing us to delegate scheduled tasks to object instances
		static std::map<UINT_PTR, WinTimer*> fTimerMap;

		// Static timer proc, used to call object instances
		static void CALLBACK OnTimerElapsed(HWND aHwnd, UINT aMessage, UINT_PTR aTimerId, DWORD aTime); 
};

// ----------------------------------------------------------------------------

#endif // __WinTimer_H__
