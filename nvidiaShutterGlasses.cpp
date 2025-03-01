
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
					cout << "USB Device: " << HardwareIDs[a] << "       \n";
					//cout << "USB Device: " << usbName << "       \n";
					return usbName;
				}
		}
		/*
		if (usbName.find("usb#vid_0955&pid_0007") != string::npos)
		{
			cout << "USB Device: " << usbName << "       \n";
			SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
			return usbName;
		}
		*/
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
	WriteFile(pipe, (char*)buffer, bytes, &bytesWritten, NULL);
	return bytesWritten;
}

template <typename T>
unsigned long readFromPipe(HANDLE pipe, T buffer, int bytes)
{
	unsigned long bytesRead;
	ReadFile(pipe, buffer, bytes, &bytesRead, NULL);
	return bytesRead;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NvidiaShutterGlasses::NvidiaShutterGlasses()
	: currentProfile(0)
	, x_offset(0.0f)
	, y_offset(0.0f)
	, w_offset(0.0f)
{
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

	pipe0 = openUsbDeviceFile("PIPE02");
	pipe1 = openUsbDeviceFile("PIPE00");
	refresh();
}

NvidiaShutterGlasses::~NvidiaShutterGlasses()
{
	CloseHandle(pipe0);
	CloseHandle(pipe1);
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
	vs_out << "\n----- From MonitorTimings.ini ------\n"
		 << "Monitor: " << MonitorID[currentProfile] << "       \n"
		 << "EDID ID: " << EDID_ID[currentProfile] << "       \n"
		 << "ScreenRefresh: " << rate << " Hz      \n"
		 << "x: " << x_us << "us                   \n" //<< x << "       \n"
		 << "y: " << y_us << "us                   \n" //<< y << "       \n"
		 << "z: " << z_us << "us                   \n" //<< z << "       \n"
		 << "w: " << w_us << "us                   \n\n" //<< w << "       \n"
		 << "Timing Increment: " << increment << "us                   \n"
		 << "--------------------------------------\n" 
		 << "                                        \n"
		 << "                                        \n";
	OutputDebugStringA(vs_out.str().c_str());

	int sequence[] = {	0x00031842,
						0x00180001, w, x, y, 0x22242830, 0x0405080a, z,
						0x00021c01, 0x00000002,	//note only 6 bytes are actually sent here
						0x00021e01, timeout,	//note only 6 bytes are actually sent here
						0x00011b01, 0x00000007,	//note only 5 bytes are actually sent here
						0x00031840 };

	HANDLE readPipe = openUsbDeviceFile("PIPE03");
	char readBuffer[7];

	writeToPipe(pipe0, sequence, 4);    // 42 18 03 00
	// Here start the problems with the sleep mode of the IR emitter !!!
	// readFromPipe fails after sleep mode
	readFromPipe(readPipe, readBuffer, 7);
	writeToPipe(pipe0, sequence+1, 28); // 01 00 18 00,ww ww ww ww,xx xx xx xx,yy yy yy yy,30 28 24 22,0a 08 05 04,zz zz zz zz
	writeToPipe(pipe0, sequence+8, 6);  // 01 1c 02 00,02 00
	writeToPipe(pipe0, sequence+10, 6); // 01 1e 02 00,timeout
	writeToPipe(pipe0, sequence+12, 5); // 01 1b 01 00,07
	writeToPipe(pipe0, sequence+13, 4); // 40 18 03 00

	CloseHandle(readPipe);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::toggleEyes(int offset)
{
	int sequence[]  = { isLeftEye() ? 0x0000feaa : 0x0000ffaa, offset };
	writeToPipe(pipe1, sequence, 8);
	ShutterGlasses::toggleEyes();
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

//#include "NvAPI.h"
//#include <adl_sdk.h>

// Get current display format and timing for an NVidia display.
// Output details to VS Output for sanity checks.
NvAPI_Status NvidiaShutterGlasses::getCurrentResolution_NVIDIA()
{
	NvAPI_Status status;

	status = NvAPI_Initialize();
	if (status != NVAPI_OK) 
	{
		vs_out << "!!! NvAPI Initialization failed" << std::endl;
		OutputDebugStringA(vs_out.str().c_str());
		return status;
	}

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
	vs_out << "Refresh Rate: " << timing.etc.rr << " Hz" << "  Physical: " << timing.etc.rrx1k / 1000.00f << std::endl;
	vs_out << "Timing standard: " << timing.etc.status << "  Name: \"" << timing.etc.name << "\"" << std::endl;
	vs_out << "Resolution: " << timing.HVisible << " x " << timing.VVisible << std::endl;
	vs_out << "Vertical total pixels: " << timing.VTotal << std::endl;

	//if (timing.TimingFlags & NV_TIMING_FLAGS_INTERLACED)
	//	vs_out << "Scan Type: Interlaced" << std::endl;
	//else
	//	vs_out << "Scan Type: Progressive" << std::endl;

	//if (timing.TimingFlags & NV_TIMING_FLAGS_PREFERRED)
	//	vs_out << "Status: Preferred Timing" << std::endl;
	OutputDebugStringA(vs_out.str().c_str());

	NvAPI_Unload();

	return NVAPI_OK;
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
