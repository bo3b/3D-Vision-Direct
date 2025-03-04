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
		bool test = QueryPerformanceCounter(&currentTime);

		if (test == 0)
			DebugBreak();
		
		if (currentTime.QuadPart < lastCallTime.QuadPart)
			DebugBreak();
		lastCallTime = currentTime;

		LARGE_INTEGER testFrequency;
		QueryPerformanceFrequency(&testFrequency);
		if (testFrequency.QuadPart != frequency.QuadPart)
			DebugBreak();
		
		LONGLONG out = (currentTime.QuadPart - startTime.QuadPart) * 1'000'000.0 / frequency.QuadPart;
		//if (out < lastOut)
		//	DebugBreak();
		lastOut = out;

		return (double)out;
	}

private:
	std::ostringstream					g_out;

	LARGE_INTEGER frequency;  // Ticks per second
	LARGE_INTEGER startTime;  // Start timestamp
	LARGE_INTEGER lastCallTime = { 0 };
	LONGLONG lastOut = 0;
};
