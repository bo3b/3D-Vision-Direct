#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <vector>
#include <sstream>
#include "shutterGlasses.h"
#include <iostream>

#include "nvapi.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class NvidiaShutterGlasses : public ShutterGlasses
{
public:
	NvidiaShutterGlasses();
	~NvidiaShutterGlasses();

	void toggleEyes(int offset);
	void nextProfile();
	void refresh();
	NvAPI_Status getCurrentResolution_NVIDIA();

	float x_offset;
	float y_offset;
	float w_offset;
	int increment = 100;

private:
	std::vector<std::string> MonitorID;
	std::vector<std::string> EDID_ID;
	std::vector<float> validRefreshRates;
	std::vector<float> valid_x_us;
	std::vector<float> valid_y_us;
	std::vector<float> valid_z_us;
	std::vector<float> valid_w_us;
	int currentProfile;
	HANDLE pipe0, pipe1;

	std::ostringstream vs_out;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
