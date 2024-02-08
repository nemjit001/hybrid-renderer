#pragma once

#include <chrono>

typedef std::chrono::time_point<std::chrono::steady_clock> time_point;

/// @brief Simple timer structure.
struct Timer
{
	/// @brief Delta time in seconds
	float deltaTime = 0.0f;
	time_point last = time_point();

	Timer()
		:
		deltaTime(0.0f),
		last()
	{
		tick();
	}

	/// @brief Tick the timer, updating the frame delta.
	inline void tick()
	{
		time_point current = std::chrono::steady_clock::now();
		std::chrono::duration<float> diff = current - last;
		deltaTime = diff.count();
		last = current;
	}
};
