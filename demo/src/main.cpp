#include <iostream>

#include "demo.h"
#include "timer.h"

static Timer gFrameTimer = Timer();

int main()
{
	std::cout << "Startup complete\n";

	while (true)
	{
		gFrameTimer.tick();
	}

	std::cout << "Goodbye!\n";
	return 0;
}
