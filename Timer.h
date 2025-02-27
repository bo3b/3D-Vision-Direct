#pragma once
// High-resolution timer class
class Timer {
public:
	Timer() {
		QueryPerformanceFrequency(&frequency);
	}

	void Start() {
		QueryPerformanceCounter(&startTime);
	}

	double GetElapsedMicroseconds() {
		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		return (double)(currentTime.QuadPart - startTime.QuadPart) * 1'000'000.0 / frequency.QuadPart;
	}

private:
	LARGE_INTEGER frequency;  // Ticks per second
	LARGE_INTEGER startTime;  // Start timestamp
};
