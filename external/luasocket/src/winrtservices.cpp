
#include "winrtservices.h"
#include <thread>


void Sleep(int milliseconds)
{
	if (milliseconds > 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	}
}
