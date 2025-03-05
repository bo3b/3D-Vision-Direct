#pragma once

// https://users.csc.calpoly.edu/~zwood/teaching/csc572/final11/rsomers/
// https://github.com/bobsomers/3dvgl/tree/master
// https://sourceforge.net/p/libnvstusb/code/HEAD/tree/
// https://github.com/FlintEastwood/3DVisionActivator
// http://www.mtbs3d.com/phpBB/viewtopic.php?f=26&t=3130
// 
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
	void WakeEmitter();
	~NvidiaShutterGlasses();

	void toggleEyes(int offset);
	void setLeftEye(int offset);
	void setRightEye(int offset);
	void nextProfile();
	void refresh();
	void clear();

	NvAPI_Status getCurrentResolution_NVIDIA();
	NvAPI_Status enable_LightBoost_NVIDIA();
	NvAPI_Status disable_LightBoost_NVIDIA();

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

	NvU32 PrimaryDisplayID = 0xDEADBEEF;

	std::ostringstream vs_out;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
