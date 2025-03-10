
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <string>
#include <fstream>
#include <cmath>
#include <iostream>
using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "nvidiaShutterGlasses.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <initguid.h>
#include <setupapi.h>
#include <usbdi.h>

// Output stream so we can redirect anything here to VS Output.
std::ostringstream vs_out;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
string getDeviceName(HDEVINFO hardwareDeviceInfo, PSP_DEVICE_INTERFACE_DATA deviceInfoData)
{
    ULONG predictedLength = 0;
	ULONG requiredLength = 0;

    //allocate a function class device data structure to receive the goods about this particular device.
    SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, deviceInfoData, NULL, 0, &requiredLength, NULL);
    PSP_DEVICE_INTERFACE_DETAIL_DATA functionClassDeviceData = new SP_DEVICE_INTERFACE_DETAIL_DATA[requiredLength];
	functionClassDeviceData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    predictedLength = requiredLength;

    if (SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, deviceInfoData, functionClassDeviceData, predictedLength, &requiredLength, NULL))
	{
		char name[256];
		//strncpy_s(name, functionClassDeviceData->DevicePath, 256);
		strncpy_s(name, functionClassDeviceData->DevicePath, 256);
		delete [] functionClassDeviceData;
		return name;
    }

	delete [] functionClassDeviceData;
	return "";
}

string findUsbDevice()
{
	HDEVINFO hardwareDeviceInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hardwareDeviceInfo == INVALID_HANDLE_VALUE)
		return "";

	//Enumerate through all devices in Set.
	SP_DEVICE_INTERFACE_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	string HardwareIDs[11] = {	"usb#vid_0955&pid_0007",
								"usb#vid_0955&pid_7001",
								"usb#vid_0955&pid_7002",
								"usb#vid_0955&pid_7003",
								"usb#vid_0955&pid_7004",
								"usb#vid_0955&pid_7008",
								"usb#vid_0955&pid_7009",
								"usb#vid_0955&pid_700A",
								"usb#vid_0955&pid_700C",
								"usb#vid_0955&pid_700D&mi_00",
								"usb#vid_0955&pid_700E&mi_00"
	};
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), { (SHORT)0, (SHORT)25 });

	for (int i = 0; SetupDiEnumDeviceInterfaces(hardwareDeviceInfo, 0, &GUID_DEVINTERFACE_USB_DEVICE, i, &deviceInfoData); ++i)
	{
		string usbName = getDeviceName(hardwareDeviceInfo, &deviceInfoData);
		
		for (int a = 0; a < (sizeof(HardwareIDs)/sizeof(string)); a++)
		{ 
			if (usbName.find(HardwareIDs[a]) != std::string::npos)
			{
					SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
					vs_out << "USB Device ID: " << HardwareIDs[a] << "       " 
						   << "  USB Device Name: " << usbName << "       " << std::endl;
					OutputDebugStringA(vs_out.str().c_str());
					return usbName;
				}
		}
	}

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	return "";
}

HANDLE openUsbDeviceFile(const string& filename)
{
	string deviceName = findUsbDevice();
    if (deviceName == "")
        return INVALID_HANDLE_VALUE;

	return	CreateFile(	(deviceName + "\\" + filename).c_str(),
						GENERIC_WRITE | GENERIC_READ,
						FILE_SHARE_WRITE | FILE_SHARE_READ,
						NULL,
						OPEN_EXISTING,
						0,
						NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
unsigned long writeToPipe(HANDLE pipe, T buffer, int bytes)
{
	unsigned long bytesWritten;
	BOOL result = WriteFile(pipe, (char*)buffer, bytes, &bytesWritten, NULL);
	if (!result)
	{
		DWORD errorCode = GetLastError();
		vs_out << "!!! WriteFile failed. Error Code: " << errorCode
			<< " | Bytes attempted: " << bytes
			<< " | Bytes written: " << bytesWritten << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		DebugBreak();
	}
	return bytesWritten;
}

template <typename T>
unsigned long readFromPipe(HANDLE pipe, T buffer, int bytes)
{
	unsigned long bytesRead;
	BOOL result = ReadFile(pipe, buffer, bytes, &bytesRead, NULL);
	if (!result)
	{
		DWORD errorCode = GetLastError();
		vs_out << "!!! ReadFile failed. Error Code: " << errorCode
			<< " | Bytes attempted: " << bytes
			<< " | Bytes written: " << bytesRead << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		DebugBreak();
	}
	return bytesRead;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NvidiaShutterGlasses::NvidiaShutterGlasses()
	: currentProfile(0)
	, x_offset(0.0f)
	, y_offset(0.0f)
	, w_offset(0.0f)
{
	NvAPI_Status status = NvAPI_Initialize();
	if (status != NVAPI_OK)
	{
		vs_out << "!!! NvAPI Initialization failed" << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		// TODO: force exception, save bool, something?
	}

	/*
	ifstream fin("validRefreshRates.ini");
	if (fin.is_open())
	{
		while (!fin.eof())
		{
			string line;
			getline(fin, line);
			//validRefreshRates.push_back(atoi(line.c_str()));
			validRefreshRates.push_back(stof(line));
		}
		fin.close();
	}
	else { validRefreshRates.push_back(99.999f); }
	*/

	ifstream fin("MonitorTimings.ini");
	if (fin.is_open())
	{
		while (!fin.eof())
		{
			string line;
			getline(fin, line);

			string searchstr = "Monitor:";
			size_t found = line.find(searchstr);
			if (found != std::string::npos)
				MonitorID.push_back(line.substr(found + searchstr.length()));

			searchstr = "EDID_ID:";
			found = line.find(searchstr);
			if (found != std::string::npos)
				EDID_ID.push_back(line.substr(found + searchstr.length()));
			
			searchstr = "RefreshRateHz:";
			found = line.find(searchstr);
			if (found != std::string::npos) 
			validRefreshRates.push_back( stof( line.substr(found+searchstr.length()) ) );

			searchstr = "X_us:";
			found = line.find(searchstr);
			if (found != std::string::npos)
				valid_x_us.push_back(stof(line.substr(found + searchstr.length())));

			searchstr = "Y_us:";
			found = line.find(searchstr);
			if (found != std::string::npos)
				valid_y_us.push_back(stof(line.substr(found + searchstr.length())));

			searchstr = "Z_us:";
			found = line.find(searchstr);
			if (found != std::string::npos)
				valid_z_us.push_back(stof(line.substr(found + searchstr.length())));

			searchstr = "W_us:";
			found = line.find(searchstr);
			if (found != std::string::npos)
				valid_w_us.push_back(stof(line.substr(found + searchstr.length())));
		}
		fin.close();
	}
	else { 
		MonitorID.push_back("No MonitorTimings.ini !!!");
		EDID_ID.push_back("DummyID");
		validRefreshRates.push_back(120.0f);
		valid_x_us.push_back(1.0f);
		valid_y_us.push_back(7333.0f);
		valid_z_us.push_back(8333.34f);
		valid_w_us.push_back(4735.0f);
	}
}

// Push this into a specific routine, so we can specify the actual timing, instead
// of being in the constructor.

void NvidiaShutterGlasses::WakeEmitter()
{
	// This sequence is here to wake up a sleeping emitter. If we immediately jump
	// in and start hitting the emitter, we get a BSOD, apparently because the USB
	// pipe is not running correctly.  The delay itself is not sufficient, the pipe
	// is somehow broken.  So rather than do anything heroic, we'll just close it.
	HANDLE wake;
	wake = openUsbDeviceFile("PIPE02");
	if (wake == INVALID_HANDLE_VALUE)
	{
		vs_out << "!!! Failed to open usb Wake pipe? Handle: " << wake << std::endl;
		OutputDebugString(vs_out.str().c_str());
		DebugBreak();
	}
	Sleep(100);
	CloseHandle(wake);
	Sleep(100);

	// Actual pipes that will be used to set up and run the emitter.
	// These are directed to the Emitter USB endpoints.
	// The PIPE02 is endpoint 2 for the usb emitter interface, used for setup.
	// The PIPE00 is endpoint 1 for the usb emitter, used for eye swap commands.
	// The PIPE03 for the usb emitter, used for read commands. (unused here)

	pipe_usb_init = openUsbDeviceFile("PIPE02");
	if (pipe_usb_init == INVALID_HANDLE_VALUE)
	{
		vs_out << "!!! Failed to open usb pipe_usb_init pipe? Handle: " << pipe_usb_init << std::endl;
		OutputDebugString(vs_out.str().c_str());
		DebugBreak();
	}
	pipe_usb_swaps = openUsbDeviceFile("PIPE00");
	if (pipe_usb_swaps == INVALID_HANDLE_VALUE)
	{
		vs_out << "!!! Failed to open usb pipe_usb_swaps pipe? Handle: " << pipe_usb_swaps << std::endl;
		OutputDebugString(vs_out.str().c_str());
		DebugBreak();
	}
}

NvidiaShutterGlasses::~NvidiaShutterGlasses()
{
	if (pipe_usb_init != INVALID_HANDLE_VALUE)
		CloseHandle(pipe_usb_init);
	if (pipe_usb_swaps != INVALID_HANDLE_VALUE)
		CloseHandle(pipe_usb_swaps);

	disable_LightBoost_NVIDIA();

	NvAPI_Unload();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//refresh variables and initialize usb device
void NvidiaShutterGlasses::refresh()
{
	float rate = validRefreshRates[currentProfile];
	float x_us = valid_x_us[currentProfile];	//us
	float y_us = valid_y_us[currentProfile];	//us(activeTime)
	float z_us = valid_z_us[currentProfile];	//us(frameTime) (Can be calculated from 1000ms/rate ???)
	float w_us = valid_w_us[currentProfile];	//us
	
	//Asus PG248Q (AUS24B1) Values from NvTimingsEd 120Hz
	//float rate = 119.983 ;
	//float x_us =    0.50 ;	//us
	//float y_us = 7334.00 ;	//us(activeTime)
	//float z_us = 8334.50 ;	//us(frameTime) (Can be calculated from 1000ms/rate ???)
	//float w_us = 4735.58 ;	//us

	// prevent negative timing values
	if ((x_us + x_offset) < 0.0) { x_offset = 0 - x_us; }
	if ((y_us + y_offset) < 0.0) { y_offset = 0 - y_us; }
	if ((w_us + w_offset) < 0.0) { w_offset = 0 - w_us; }

	x_us += x_offset;	
	y_us += y_offset;	
	w_us += w_offset;

	//int NVSTUSB_CLOCK = 48000000; // CPU clock of IR emitter
	//int NVSTUSB_T0_CLOCK = NVSTUSB_CLOCK / 12 / 1000000; // T0 runs at  4MHz
	//int NVSTUSB_T2_CLOCK = NVSTUSB_CLOCK /  4 / 1000000; // T2 runs at 12MHz
	int x = (int)(-x_us * 4 + 1); // T0 runs at  4MHz
	int y = (int)(-y_us * 4 + 1); // T0 runs at  4MHz
	int z = (int)(-z_us *12 + 1); // T2 runs at 12MHz
	int w = (int)(-w_us *12 + 1); // T2 runs at 12MHz
	int timeout = (int)(rate * 4); // idle timeout(number of frames)

	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), { (SHORT)0, (SHORT)15 });
	vs_out << std::endl 
		 << "----- From MonitorTimings.ini ------" << std::endl
		 << "Monitor: " << MonitorID[currentProfile] << "       " << std::endl
		 << "EDID ID: " << EDID_ID[currentProfile] << "       " << std::endl
		 << "ScreenRefresh: " << rate << " Hz      " << std::endl
		 << "x: " << x_us << "us                   " << std::endl	//<< x << "       " << std::endl
		 << "y: " << y_us << "us                   " << std::endl	//<< y << "       " << std::endl
		 << "z: " << z_us << "us                   " << std::endl	//<< z << "       " << std::endl
		 << "w: " << w_us << "us                   " << std::endl 
		 << std::endl												//<< w << "       " << std::endl
		 << "Timing Increment: " << increment << "us                   " << std::endl
		 << "--------------------------------------" << std::endl 
		 << "                                        " << std::endl
		 << "                                        " << std::endl;
	OutputDebugStringA(vs_out.str().c_str());

	// USB emitter Init sequence to endpoint 2
	//
	// These hex constants are written backwards from the original libnvstusb code.

	int sequence[] = {	0x00031840,				// set 40 from 42 to skip read, just clear
						0x00180001, w, x, y, 0x22242830, 0x0405080a, z,
						0x00021c01, 0x00000002,	//note only 6 bytes are actually sent here
						0x00021e01, timeout,	//note only 6 bytes are actually sent here
						0x00011b01, 0x00000007,	//note only 5 bytes are actually sent here
						0x00031840 };


	writeToPipe(pipe_usb_init, sequence, 4);    // 40 18 03 00
	writeToPipe(pipe_usb_init, sequence+1, 28); // 01 00 18 00,ww ww ww ww,xx xx xx xx,yy yy yy yy,30 28 24 22,0a 08 05 04,zz zz zz zz
	writeToPipe(pipe_usb_init, sequence+8, 6);  // 01 1c 02 00,02 00
	writeToPipe(pipe_usb_init, sequence+10, 6); // 01 1e 02 00,timeout
	writeToPipe(pipe_usb_init, sequence+12, 5); // 01 1b 01 00,07
	writeToPipe(pipe_usb_init, sequence+13, 4); // 40 18 03 00

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Just send the clear command by itself. 
// It is the last sequence before the example goes into alternating eye pings.

void NvidiaShutterGlasses::clear()
{
	int sequence[] = { 0x00031840 };
	writeToPipe(pipe_usb_init, sequence, 4); // 40 18 03 00
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::toggleEyes()
{
	uint32_t sequence[]  = { isLeftEye() ? 0x0000feaa : 0x0000ffaa, 0xffff0000 };	// aa ff/fe 00 00  00 00 ff ff 
	writeToPipe(pipe_usb_swaps, sequence, 8);
	ShutterGlasses::toggleEyes();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::setLeftEye()
{
	int sequence[] = { 0x0000feaa, offset };
	writeToPipe(pipe_usb_swaps, sequence, 8);
	ShutterGlasses::setLeftEye();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::setRightEye()
{
	int sequence[] = { 0x0000ffaa, offset };
	writeToPipe(pipe_usb_swaps, sequence, 8);
	ShutterGlasses::setRightEye();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::nextProfile()
{
	currentProfile++;
	if (currentProfile >= validRefreshRates.size())
		currentProfile = 0;
	x_offset = 0.0f;
	y_offset = 0.0f;
	w_offset = 0.0f;
	refresh();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This will change the monitor timings, so that it will enable LightBoost.
// 
// After long and tedious research, it turns out simple. LightBoost turns
// on if we bump the back porch of the timing signal. This can be done in CreateCustomResolution
// in NVidia control panel, but we want to do it programmatically to avoid external tools.
//
// The trick is to set the TotalPixels to 5 higher than it normally runs.
// 5 extra lines in the vertical blank will tell the PG278QR to go into LightBoost mode.
//
// In theory this is not dangerous in terms of damaging hardware, as monitors like a
// PG278QR will go black screen and avoid breaking. I think using the NvAPI itself adds
// a level of safety as well, since they are pretty serious about their debugging.
// As opposed to running external tools like CRU where you can do anything.
// 

//#include "NvAPI.h"
//#include <adl_sdk.h>

// Get current display format and timing for an NVidia display.
// Output details to VS Output for sanity checks.
// If we determine it's a PG278QR running at 120Hz, we'll set flag as OK.

NvAPI_Status NvidiaShutterGlasses::getCurrentResolution_NVIDIA()
{
	NvAPI_Status status;

	// We only can expect to use primary display?
	// TODO: seems like we could allow syncing on an alternate display.
	//status = NvAPI_EnumNvidiaDisplayHandle(0, &hNvDisplay);
	//if (status != NVAPI_OK)
	//{
	//	vs_out << "!!! Failed to get primary display handle" << std::endl;
	//	OutputDebugStringA(vs_out.str().c_str());
	//	return false;
	//}

	NvPhysicalGpuHandle gpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
	NvU32 gpuCount = 0;
	status = NvAPI_EnumPhysicalGPUs(gpuHandles, &gpuCount);
	if (status != NVAPI_OK) 
	{
		vs_out << "!!! Failed to enumerate NVidia GPUs - none in system?" << std::endl;
		NvAPI_Unload();
		OutputDebugString(vs_out.str().c_str());
		return status;
	}

	// Get all display IDs connected to the first GPU
	NV_GPU_DISPLAYIDS displayIds[NVAPI_MAX_DISPLAYS] = { 0 };
	displayIds->version = NV_GPU_DISPLAYIDS_VER2;
	NvU32 displayCount = 1;		// Only do first one for now.
	status = NvAPI_GPU_GetConnectedDisplayIds(gpuHandles[0], displayIds, &displayCount, 0);
	if (status != NVAPI_OK || displayCount == 0) 
	{
		vs_out << "!!! Failed to get connected display IDs. Count: " << displayCount << std::endl;
		NvAPI_Unload();
		OutputDebugString(vs_out.str().c_str());
		return status;
	}


	// This seems to work for zeroed out NV_TIMING_INPUT.
	// This is what we want- current timings that are active, not hypothetical variants
	// that might be enabled someday. 
	// However, the resolution does not change from the native spec, when changing to 
	// other resolutions.
	
	NV_TIMING timing = {};
	NV_TIMING_INPUT current = { 0 };
	current.version = NV_TIMING_INPUT_VER;
	status = NvAPI_DISP_GetTiming(displayIds[0].displayId, &current, &timing);
	if (status != NVAPI_OK) 
	{
		vs_out << "Failed to retrieve current timing parameters" << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		return status;
	}

	vs_out << "Display Timing Details:" << std::endl;
	vs_out << "----------------------" << std::endl;
	vs_out << "Timing standard: " << timing.etc.status << "  Name: \"" << timing.etc.name << "\"" << std::endl;
	vs_out << "Refresh Rate: " << timing.etc.rr << " Hz" << "  Physical: " << timing.etc.rrx1k / 1000.00f << std::endl;
	vs_out << "** Pixel Clock: " << timing.pclk << std::endl;
	vs_out << "Resolution: " << timing.HVisible << " x " << timing.VVisible << std::endl;
	vs_out << "** Vertical total pixels: " << timing.VTotal << "  Horizontal total pixels: "<< timing.HTotal << std::endl;
	vs_out << "----------------------" << std::endl;

	//if (timing.TimingFlags & NV_TIMING_FLAGS_INTERLACED)
	//	vs_out << "Scan Type: Interlaced" << std::endl;
	//else
	//	vs_out << "Scan Type: Progressive" << std::endl;

	//if (timing.TimingFlags & NV_TIMING_FLAGS_PREFERRED)
	//	vs_out << "Status: Preferred Timing" << std::endl;
	OutputDebugStringA(vs_out.str().c_str());


	// If we are running a known good monitor, let's mark it valid and thus
	// enable the enable_LightBoost_NVIDIA call.
	if (timing.etc.rrx1k == 119998 && timing.VTotal == 1525 && timing.HVisible == 2560 && timing.VVisible == 1440)
	{
		PrimaryDisplayID = displayIds[0].displayId;
	}

	return NVAPI_OK;
}

// The goal here will be to enable LightBoost upon request, and disable it when the
// object is destroyed. Currently only going to enable for a PG278QR, a monitor I can
// personally test. And only for 120Hz mode. If the monitor is not already running
// in that state, we won't try.
//
// This might not be the right way to do this. We could for example add a custom
// display resolution that is persistent, and activate it. It's just notable that
// using this approach gives the same monitor flash/setup as when 3D Vision is 
// activated in a game.

NvAPI_Status NvidiaShutterGlasses::enable_LightBoost_NVIDIA()
{
	NvAPI_Status status;

	if (PrimaryDisplayID == 0xDEADBEEF)
		return NVAPI_NVIDIA_DISPLAY_NOT_FOUND;

	NV_TIMING timing = { 0 };
	NV_TIMING_INPUT current = { 0 };
	current.version = NV_TIMING_INPUT_VER;
	status = NvAPI_DISP_GetTiming(PrimaryDisplayID, &current, &timing);
	if (status != NVAPI_OK)
	{
		vs_out << "Failed to retrieve current timing parameters for ID: " << PrimaryDisplayID << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		return status;
	}

	// Try to set a new timing for the monitor so that LightBoost will turn on.
	NV_CUSTOM_DISPLAY lightboost = { 0 };
	lightboost.version = NV_CUSTOM_DISPLAY_VER;
	lightboost.timing = timing;						// Copy everything from current for safety.

	lightboost.srcPartition = { 0.0f, 0.0f, 1.0f, 1.0f };
	lightboost.width = 2560;
	lightboost.height = 1440;
	lightboost.colorFormat = NV_FORMAT_A8R8G8B8;	// means 32 bit color
	lightboost.xRatio = 1.0f;
	lightboost.yRatio = 1.0f;
	lightboost.depth = 32;

	// The magic trick. Bump the VTotal by 5, and voila- LightBoost is on.
	// Also key is bumping the pixel clock to match the longer frame. Without
	// this the glasses were ever slightly out of sync with monitor. 
	if (lightboost.timing.VTotal == 1525)
	{
		NvU16 standard_vtotal = lightboost.timing.VTotal;
		NvU32 standard_pclk = lightboost.timing.pclk;

		lightboost.timing.VTotal = standard_vtotal + 5;
		lightboost.timing.pclk = standard_pclk * lightboost.timing.VTotal / standard_vtotal;	// deliberately no floats

		vs_out << "** Switch VTotal from: " << standard_vtotal << " to: " << lightboost.timing.VTotal << std::endl;
		vs_out << "** Switch pclk from: " << standard_pclk << " to: " << lightboost.timing.pclk << std::endl;
		vs_out << "----------------------" << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
	}

	status = NvAPI_DISP_TryCustomDisplay(&PrimaryDisplayID, 1, &lightboost);
	if (status != NVAPI_OK)
	{
		vs_out << "Failed to retrieve current timing parameters for ID: " << PrimaryDisplayID << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		return status;
	}

	return status;
}

// Restore the previous setting upon exit.

NvAPI_Status NvidiaShutterGlasses::disable_LightBoost_NVIDIA()
{
	NvAPI_Status status;

	if (PrimaryDisplayID == 0xDEADBEEF)
		return NVAPI_NVIDIA_DISPLAY_NOT_FOUND;

	status = NvAPI_DISP_RevertCustomDisplayTrial(&PrimaryDisplayID, 1);

	return status;
}

//void GetCurrentResolution_AMD()
//{
//	int iAdapterIndex = 0;
//	ADLDisplayMode displayMode;
//	if (ADL_Display_Modes_Get(iAdapterIndex, -1, &displayMode) == ADL_OK) {
//		std::cout << "AMD Current Resolution: " << displayMode.iXRes << "x" << displayMode.iYRes
//			<< " @ " << displayMode.iRefreshRate << "Hz" << std::endl;
//	}
//	else {
//		std::cerr << "Failed to retrieve AMD display settings." << std::endl;
//	}
//}

//bool SetCustomResolution_NVIDIA(int width, int height, int refreshRate)
//{
//	NvAPI_Status status;
//	NvDisplayHandle hNvDisplay = NULL;
//	NV_TIMING timing = {};
//
//	status = NvAPI_Initialize();
//	if (status != NVAPI_OK) {
//		std::cerr << "NvAPI Initialization failed" << std::endl;
//		return false;
//	}
//
//	status = NvAPI_EnumNvidiaDisplayHandle(0, &hNvDisplay);
//	if (status != NVAPI_OK) {
//		std::cerr << "Failed to get display handle" << std::endl;
//		return false;
//	}
//
//	status = NvAPI_DISP_GetTiming(hNvDisplay, &timing);
//	if (status != NVAPI_OK) {
//		std::cerr << "Failed to retrieve current timing parameters" << std::endl;
//		return false;
//	}
//
//	timing.horizontalTotal += 10;
//	timing.verticalTotal += 5;
//
//	status = NvAPI_DISP_TryCustomDisplay(hNvDisplay, &timing);
//	if (status != NVAPI_OK) {
//		std::cerr << "Failed to set custom resolution" << std::endl;
//		return false;
//	}
//
//	std::cout << "Custom resolution set successfully on NVIDIA." << std::endl;
//	return true;
//}

//bool SetCustomResolution_AMD(int width, int height, int refreshRate)
//{
//	int iAdapterIndex = 0;
//	ADLDisplayMode displayMode = { width, height, refreshRate };
//
//	displayMode.iXRes = width;
//	displayMode.iYRes = height;
//	displayMode.iRefreshRate = refreshRate;
//	displayMode.iTimingStandard = ADL_DL_TIMING_STANDARD_CVT;
//	displayMode.iHTotal += 10; // Adjust horizontal total pixels
//	displayMode.iVTotal += 5;  // Adjust vertical total pixels
//
//	if (ADL_Display_Modes_Set(iAdapterIndex, -1, &displayMode) == ADL_OK) {
//		std::cout << "Custom resolution set successfully on AMD." << std::endl;
//		return true;
//	}
//	else {
//		std::cerr << "Failed to set custom resolution on AMD." << std::endl;
//		return false;
//	}
//}

//int main()
//{
//	GetCurrentResolution_NVIDIA();
//	GetCurrentResolution_AMD();
//	SetCustomResolution_NVIDIA(1920, 1080, 110);
//	SetCustomResolution_AMD(1920, 1080, 110);
//	return 0;
//}
