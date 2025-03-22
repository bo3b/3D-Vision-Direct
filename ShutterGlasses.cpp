////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ShutterGlasses.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#include <setupapi.h>
#include <initguid.h>
#include <usbiodef.h>

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

using std::endl;
using std::getline;
using std::wifstream;
using std::wstring;
using std::wstringstream;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static wstring get_device_name(HDEVINFO hardwareDeviceInfo, PSP_DEVICE_INTERFACE_DATA deviceInfoData)
{
    ULONG predicted_length = 0;
    ULONG required_length  = 0;

    //allocate a function class device data structure to receive the goods about this particular device.
    SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, deviceInfoData, nullptr, 0, &required_length, nullptr);

    PSP_DEVICE_INTERFACE_DETAIL_DATA function_device_data = new SP_DEVICE_INTERFACE_DETAIL_DATA[required_length];
    function_device_data->cbSize                          = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    predicted_length                                      = required_length;

    if (SetupDiGetDeviceInterfaceDetail(hardwareDeviceInfo, deviceInfoData, function_device_data, predicted_length, &required_length, nullptr))
    {
        wstring name(function_device_data->DevicePath);

        delete[] function_device_data;
        return name;
    }

    // Error result
    delete[] function_device_data;
    return L"";
}

static wstring find_usb_device()
{
    HDEVINFO device_info = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (device_info == INVALID_HANDLE_VALUE)
        return L"";

    //Enumerate through all devices in Set.
    SP_DEVICE_INTERFACE_DATA device_info_data;
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    wstring hardware_IDs[11] = { L"usb#vid_0955&pid_0007",
                                 L"usb#vid_0955&pid_7001",
                                 L"usb#vid_0955&pid_7002",
                                 L"usb#vid_0955&pid_7003",
                                 L"usb#vid_0955&pid_7004",
                                 L"usb#vid_0955&pid_7008",
                                 L"usb#vid_0955&pid_7009",
                                 L"usb#vid_0955&pid_700A",
                                 L"usb#vid_0955&pid_700C",
                                 L"usb#vid_0955&pid_700D&mi_00",
                                 L"usb#vid_0955&pid_700E&mi_00" };

    for (int i = 0; SetupDiEnumDeviceInterfaces(device_info, nullptr, &GUID_DEVINTERFACE_USB_DEVICE, i, &device_info_data); ++i)
    {
        wstring usb_name = get_device_name(device_info, &device_info_data);

        for (int a = 0; a < (sizeof(hardware_IDs) / sizeof(wstring)); a++)
        {
            if (usb_name.find(hardware_IDs[a]) != std::wstring::npos)
            {
                SetupDiDestroyDeviceInfoList(device_info);
                wstringstream log_s;
                log_s << "USB Device ID: " << hardware_IDs[a] << "       "
                      << "  USB Device Name: " << usb_name << "       " << endl;
                OutputDebugString(log_s.str().c_str());
                return usb_name;
            }
        }
    }

    // Error out
    SetupDiDestroyDeviceInfoList(device_info);
    return L"";
}

static HANDLE open_usb_device_filename(const wstring& filename)
{
    wstring emitter_name = find_usb_device();
    if (emitter_name == L"")
        return INVALID_HANDLE_VALUE;

    return CreateFile((emitter_name + L"\\" + filename).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD write_to_pipe(HANDLE pipe, uint32_t* buffer, DWORD count)
{
    DWORD bytes_written;
    BOOL  result = WriteFile(pipe, buffer, count, &bytes_written, nullptr);
    if (!result)
    {
        DWORD         errorCode = GetLastError();
        wstringstream log_s;
        log_s << "!!! WriteFile failed. Error Code: " << errorCode
              << " | Bytes attempted: " << count
              << " | Bytes written: " << bytes_written << endl;
        OutputDebugString(log_s.str().c_str());
        DebugBreak();
    }
    return bytes_written;
}

// Unused
DWORD read_from_pipe(HANDLE pipe, uint32_t* buffer, DWORD count)
{
    DWORD bytes_read;
    BOOL  result = ReadFile(pipe, buffer, count, &bytes_read, nullptr);
    if (!result)
    {
        DWORD         errorCode = GetLastError();
        wstringstream log_s;
        log_s << "!!! ReadFile failed. Error Code: " << errorCode
              << " | Bytes attempted: " << count
              << " | Bytes written: " << bytes_read << endl;
        OutputDebugString(log_s.str().c_str());
        DebugBreak();
    }
    return bytes_read;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NvidiaShutterGlasses::NvidiaShutterGlasses()
{
    wstringstream log_s;
    log_s << "NvidiaShutterGlasses::NvidiaShutterGlasses(" << typeid(*this).name() << "@" << this << ")" << endl;
    OutputDebugString(log_s.str().c_str());

    NvAPI_Status status = NvAPI_Initialize();
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "!!! NvAPI Initialization failed" << endl;
        OutputDebugString(log_s.str().c_str());
        // TODO: force exception, save bool, something?
    }

    // Only reading the single monitor setup here.
    // Will need to use IniHandler in 3Dmigoto for multiples.

    monitor_info ini;

    wifstream fin("MonitorTimings.ini");
    if (fin.is_open())
    {
        while (!fin.eof())
        {
            wstring line;
            getline(fin, line);

            wstring searchstr = L"Monitor: ";
            size_t  found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.monitor_name = (line.substr(found + searchstr.length()));

            searchstr = L"EDID_ID: ";
            found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.monitor_EDID = (line.substr(found + searchstr.length()));

            searchstr = L"RefreshRateHz: ";
            found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.refresh_rate = (stof(line.substr(found + searchstr.length())));

            searchstr = L"X_us:";
            found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.timer_x_us = (stof(line.substr(found + searchstr.length())));

            searchstr = L"Y_us:";
            found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.timer_y_us = (stof(line.substr(found + searchstr.length())));

            searchstr = L"Z_us:";
            found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.timer_z_us = (stof(line.substr(found + searchstr.length())));

            searchstr = L"W_us:";
            found     = line.find(searchstr);
            if (found != std::string::npos)
                ini.timer_w_us = (stof(line.substr(found + searchstr.length())));
        }
        fin.close();
    }
    else
    {
        // Not found, default to PG278QR known good timings
        ini.monitor_name = (L"No MonitorTimings.ini !!!");
        ini.monitor_EDID = (L"DummyID");
        ini.refresh_rate = (119.997f);
        ini.timer_x_us   = (0.5f);
        ini.timer_y_us   = (7334.00f);
        ini.timer_z_us   = (8333.50f);
        ini.timer_w_us   = (4735.58f);
    }

    // Single add to the vector for now.
    monitors.push_back(ini);
}

// Push this into a specific routine, so we can specify the actual timing, instead
// of being in the constructor.

void NvidiaShutterGlasses::WakeEmitter()
{
    wstringstream log_s;
    log_s << "NvidiaShutterGlasses::WakeEmitter(" << typeid(*this).name() << "@" << this << ")" << endl;
    OutputDebugString(log_s.str().c_str());

    // This sequence is here to wake up a sleeping emitter. If we immediately jump
    // in and start hitting the emitter, we get a BSOD, apparently because the USB
    // pipe is not running correctly.  The delay itself is not sufficient, the pipe
    // is somehow broken.  So rather than do anything heroic, we'll just close it.
    HANDLE wake;
    wake = open_usb_device_filename(L"PIPE02");
    if (wake == INVALID_HANDLE_VALUE)
    {
        wstringstream log_s;
        log_s << "!!! Failed to open usb Wake pipe? Handle: " << wake << endl;
        OutputDebugString(log_s.str().c_str());
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

    pipe_usb_init = open_usb_device_filename(L"PIPE02");
    if (pipe_usb_init == INVALID_HANDLE_VALUE)
    {
        wstringstream log_s;
        log_s << "!!! Failed to open usb pipe_usb_init pipe? Handle: " << pipe_usb_init << endl;
        OutputDebugString(log_s.str().c_str());
        DebugBreak();
    }
    pipe_usb_swaps = open_usb_device_filename(L"PIPE00");
    if (pipe_usb_swaps == INVALID_HANDLE_VALUE)
    {
        wstringstream log_s;
        log_s << "!!! Failed to open usb pipe_usb_swaps pipe? Handle: " << pipe_usb_swaps << endl;
        OutputDebugString(log_s.str().c_str());
        DebugBreak();
    }
}

// Because this object is standalone, and not wrapping a DX11 object, we will
// go ahead and use the ctor/dtor c++ style.
//
// Note, anything derived from IUnknown should follow DX11 model.

NvidiaShutterGlasses::~NvidiaShutterGlasses()
{
    if (pipe_usb_init != INVALID_HANDLE_VALUE)
        CloseHandle(pipe_usb_init);
    if (pipe_usb_swaps != INVALID_HANDLE_VALUE)
        CloseHandle(pipe_usb_swaps);

    DisableLightBoost();

    NvAPI_Unload();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//refresh variables and initialize usb device
void NvidiaShutterGlasses::InitEmitter()
{
    wstringstream log_s;
    log_s << "NvidiaShutterGlasses::InitEmitter(" << typeid(*this).name() << "@" << this << ")" << endl;
    OutputDebugString(log_s.str().c_str());

    monitor_info main = monitors.front();

    float rate = main.refresh_rate;
    float x_us = main.timer_x_us;  //us
    float y_us = main.timer_y_us;  //us(activeTime)
    float z_us = main.timer_z_us;  //us(frameTime) (Can be calculated from 1000ms/rate ???)
    float w_us = main.timer_w_us;  //us

    //Asus PG248Q (AUS24B1) Values from NvTimingsEd 120Hz
    //float rate = 119.983 ;
    //float x_us =    0.50 ;	//us
    //float y_us = 7334.00 ;	//us(activeTime)
    //float z_us = 8334.50 ;	//us(frameTime) (Can be calculated from 1000ms/rate ???)
    //float w_us = 4735.58 ;	//us

    //int NVSTUSB_CLOCK = 48000000; // CPU clock of IR emitter
    //int NVSTUSB_T0_CLOCK = NVSTUSB_CLOCK / 12 / 1000000; // T0 runs at  4MHz
    //int NVSTUSB_T2_CLOCK = NVSTUSB_CLOCK /  4 / 1000000; // T2 runs at 12MHz
    uint32_t x       = (int)(-x_us * 4 + 1);   // T0 runs at  4MHz
    uint32_t y       = (int)(-y_us * 4 + 1);   // T0 runs at  4MHz
    uint32_t z       = (int)(-z_us * 12 + 1);  // T2 runs at 12MHz
    uint32_t w       = (int)(-w_us * 12 + 1);  // T2 runs at 12MHz
    uint32_t timeout = (int)(rate * 4);        // idle timeout(number of frames)

    {
        wstringstream log_s;
        log_s << endl
              << "----- From monitors.ini ------" << endl
              << "Monitor: " << main.monitor_name << "       " << endl
              << "EDID ID: " << main.monitor_EDID << "       " << endl
              << "ScreenRefresh: " << rate << " Hz      " << endl
              << "x: " << x_us << "us                   " << endl
              << "y: " << y_us << "us                   " << endl
              << "z: " << z_us << "us                   " << endl
              << "w: " << w_us << "us                   " << endl
              << "--------------------------------------" << endl
              << "                                        " << endl;
        OutputDebugString(log_s.str().c_str());
    }

    // USB emitter Init sequence to endpoint 2
    //
    // These hex constants are written backwards from the original libnvstusb code.

    uint32_t sequence[] = { 0x00031840,  // set 40 from 42 to skip read, just ClearEmitter
                            0x00180001, w, x, y, 0x22242830, 0x0405080a, z,
                            0x00021c01, 0x00000002,  //note only 6 bytes are actually sent here
                            0x00021e01, timeout,     //note only 6 bytes are actually sent here
                            0x00011b01, 0x00000007,  //note only 5 bytes are actually sent here
                            0x00031840 };

    write_to_pipe(pipe_usb_init, sequence, 4);       // 40 18 03 00
    write_to_pipe(pipe_usb_init, sequence + 1, 28);  // 01 00 18 00,ww ww ww ww,xx xx xx xx,yy yy yy yy,30 28 24 22,0a 08 05 04,zz zz zz zz
    write_to_pipe(pipe_usb_init, sequence + 8, 6);   // 01 1c 02 00,02 00
    write_to_pipe(pipe_usb_init, sequence + 10, 6);  // 01 1e 02 00,timeout
    write_to_pipe(pipe_usb_init, sequence + 12, 5);  // 01 1b 01 00,07
    write_to_pipe(pipe_usb_init, sequence + 13, 4);  // 40 18 03 00
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Just send the ClearEmitter command by itself.
// It is the last sequence before the example goes into alternating eye pings.

void NvidiaShutterGlasses::ClearEmitter()
{
    uint32_t sequence[] = { 0x00031840 };
    write_to_pipe(pipe_usb_init, sequence, 4);  // 40 18 03 00
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::ToggleEyes()
{
    uint32_t eye        = IsLeftEye() ? 0x0000feaa : 0x0000ffaa;
    uint32_t sequence[] = { eye, 0xffff0000 };  // aa ff/fe 00 00  00 00 ff ff
    write_to_pipe(pipe_usb_swaps, sequence, 8);
    ShutterGlasses::ToggleEyes();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::SetLeftEye()
{
    uint32_t sequence[] = { 0x0000feaa, 0xffff0000 };
    write_to_pipe(pipe_usb_swaps, sequence, 8);
    ShutterGlasses::SetLeftEye();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void NvidiaShutterGlasses::SetRightEye()
{
    uint32_t sequence[] = { 0x0000ffaa, 0xffff0000 };
    write_to_pipe(pipe_usb_swaps, sequence, 8);
    ShutterGlasses::SetRightEye();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This will change the monitor timings, so that it will enable LightBoost.
//
// After long and tedious research (4 years!), it turns out simple. LightBoost turns
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
// This is tested as working correctly on a PG278QR monitor.

// Fetch the EDID from the NV_EDID struct. This is a giant nasty hairball of incredibly
// bad design decisions, so requires bit twiddling I fetched from ChatGPT.

static wstring parse_monitor_EDID(NvPhysicalGpuHandle main_gpu)
{
    NvAPI_Status status;
    NV_EDID      raw_edid = {};

    // Get NvDisplayHandle for the 0 based, Main Display.
    NvDisplayHandle hDisplay;
    status = NvAPI_EnumNvidiaDisplayHandle(0, &hDisplay);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "Failed to get NvDisplayHandle for Main Display. Status: " << status << endl;
        OutputDebugString(log_s.str().c_str());
        return L"";
    }

    // Use the Main Display NvDisplayHandle to fetch the outputID bit array.
    NvU32 outputId;
    status = NvAPI_GetAssociatedDisplayOutputId(hDisplay, &outputId);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "Failed to get display output ID. Status: " << status << endl;
        OutputDebugString(log_s.str().c_str());
        return L"";
    }

    // Now we can use the outputID array to fetch the EDID data.
    raw_edid.version    = NV_EDID_VER3;
    raw_edid.sizeofEDID = sizeof(raw_edid);
    status              = NvAPI_GPU_GetEDID(main_gpu, outputId, &raw_edid);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "Failed to retrieve monitor EDID string. Status: " << status << endl;
        OutputDebugString(log_s.str().c_str());
        return L"";
    }

    // Ensure EDID has the standard 128-byte block (256 in current variant)
    if (raw_edid.sizeofEDID < 128)
    {
        wstringstream log_s;
        log_s << "Failed to retrieve monitor EDID string. Bad sizeofEDID: " << raw_edid.sizeofEDID << endl;
        OutputDebugString(log_s.str().c_str());
        return L"";
    }

    // Use Manufacturer ID (Bytes 8–9) to create the 3-letter vendor code
    // This is some BCD encoded madness with a 5 bit alphabet.
    uint16_t vendor_id      = (raw_edid.EDID_Data[8] << 8) | raw_edid.EDID_Data[9];
    char     vendor_code[4] = {};
    vendor_code[0]          = ((vendor_id >> 10) & 0x1F) + 'A' - 1;
    vendor_code[1]          = ((vendor_id >> 5) & 0x1F) + 'A' - 1;
    vendor_code[2]          = (vendor_id & 0x1F) + 'A' - 1;
    vendor_code[3]          = '\0';
    wstring vendor_string(vendor_code, vendor_code + 3);

    // Use Product/Model ID (Bytes 10–11) to create a Hexadecimal string (little endian swap)
    uint16_t           product_id = (raw_edid.EDID_Data[11] << 8) | raw_edid.EDID_Data[10];
    std::wstringstream product_stream;
    product_stream << std::uppercase << std::hex << std::setw(4) << std::setfill(L'0');  // Set to print HEX
    product_stream << product_id;
    wstring product_code = product_stream.str();

    return vendor_string + L"_" + product_code;
}

// Get current display format and timing for an NVidia display.
// Output details to VS Output for sanity checks.
// If we determine it's a PG278QR running at 120Hz, we'll set flag as OK.

NvAPI_Status NvidiaShutterGlasses::GetCurrentResolution()
{
    wstringstream log_s;
    NvAPI_Status  status;
    wstring       monitor_EDID = L"";

    log_s << "NvidiaShutterGlasses::GetCurrentResolution(" << typeid(*this).name() << "@" << this << ")" << endl;
    OutputDebugString(log_s.str().c_str());

    // TODO: Find GPU in system, from Windows, decide to use AMD or NVidia

    // We only can expect to use primary display?
    // TODO: seems like we could allow syncing on an alternate display.
    //status = NvAPI_EnumNvidiaDisplayHandle(0, &hNvDisplay);
    //if (status != NVAPI_OK)
    //{
    //	log_s << "!!! Failed to get primary display handle" << endl;
    //	OutputDebugString(log_s.str().c_str());
    //	return false;
    //}

    NvPhysicalGpuHandle gpu_handles[NVAPI_MAX_PHYSICAL_GPUS] = {};
    NvU32               gpu_count                            = 0;

    status = NvAPI_EnumPhysicalGPUs(gpu_handles, &gpu_count);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "!!! Failed to enumerate NVidia GPUs - none in system?" << endl;
        NvAPI_Unload();
        OutputDebugString(log_s.str().c_str());
        return status;
    }

    // Get all display IDs connected to the first GPU
    NV_GPU_DISPLAYIDS display_ids[NVAPI_MAX_DISPLAYS] = {};
    display_ids->version                              = NV_GPU_DISPLAYIDS_VER;
    NvU32 display_count                               = 1;  // Only do first one for now.

    status = NvAPI_GPU_GetConnectedDisplayIds(gpu_handles[0], display_ids, &display_count, 0);
    if (status != NVAPI_OK || display_count == 0)
    {
        wstringstream log_s;
        log_s << "!!! Failed to get connected display IDs. Count: " << display_count << endl;
        NvAPI_Unload();
        OutputDebugString(log_s.str().c_str());
        return status;
    }

    // Get the EDID as a wstring, so we can determine if it is a monitor on our whitelist.
    // Only doing Main Display for now.

    wstring monitor_edid = parse_monitor_EDID(gpu_handles[0]);

    // This seems to work for zeroed out NV_TIMING_INPUT.
    // This is what we want- current timings that are active, not hypothetical variants
    // that might be enabled someday.
    // However, the resolution does not change from the native spec, when changing to
    // other resolutions.

    NV_TIMING       timing  = {};
    NV_TIMING_INPUT current = {};
    current.version         = NV_TIMING_INPUT_VER;

    status = NvAPI_DISP_GetTiming(display_ids[0].displayId, &current, &timing);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "Failed to retrieve current timing parameters" << endl;
        OutputDebugString(log_s.str().c_str());
        return status;
    }

    {
        wstringstream log_s;
        log_s << endl;
        log_s << "Display Timing Details - EDID: " << monitor_edid << endl;
        log_s << "----------------------" << endl;
        log_s << "Timing standard: " << timing.etc.status << "  Name: \"" << reinterpret_cast<char*>(timing.etc.name) << "\"" << endl;
        log_s << "Refresh Rate: " << timing.etc.rr << " Hz" << "  Physical: " << timing.etc.rrx1k / 1000.00f << endl;
        log_s << "** Pixel Clock: " << timing.pclk << endl;
        log_s << "Resolution: " << timing.HVisible << " x " << timing.VVisible << endl;
        log_s << "** Vertical total pixels: " << timing.VTotal << "  Horizontal total pixels: " << timing.HTotal << endl;
        log_s << "----------------------" << endl;
    }

    //if (timing.TimingFlags & NV_TIMING_FLAGS_INTERLACED)
    //	log_s << "Scan Type: Interlaced" << endl;
    //else
    //	log_s << "Scan Type: Progressive" << endl;

    //if (timing.TimingFlags & NV_TIMING_FLAGS_PREFERRED)
    //	log_s << "Status: Preferred Timing" << endl;
    OutputDebugString(log_s.str().c_str());

    // For the moment, this will just verify the first and only record from MonitorTimings.ini to check EDID
    // If we are running a known good monitor, let's mark it valid and thus enable the
    // EnableLightBoost call. It needs to be the known monitor, and refresh rate must
    // match. We match against the rounded to decimal and logical version, because there
    // is a lot of slop in these numbers like 119.998 vs. 119.997 in monitortimings.ini
    bool refresh_matches = round(monitors.front().refresh_rate) == timing.etc.rr;
    if (monitors.front().monitor_EDID == monitor_edid && refresh_matches)
    {
        PrimaryDisplayID = display_ids[0].displayId;
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

NvAPI_Status NvidiaShutterGlasses::EnableLightBoost()
{
    NvAPI_Status status;

    wstringstream log_s;
    log_s << "NvidiaShutterGlasses::EnableLightBoost(" << typeid(*this).name() << "@" << this << ")" << endl;
    OutputDebugString(log_s.str().c_str());

    if (PrimaryDisplayID == 0xDEADBEEF)
        return NVAPI_NVIDIA_DISPLAY_NOT_FOUND;

    // We already know it's the proper hardware, but let's fetch 'timing' again
    // so that we can be certain all parameters are initialized properly.
    NV_TIMING       timing  = {};
    NV_TIMING_INPUT current = {};
    current.version         = NV_TIMING_INPUT_VER;
    status                  = NvAPI_DISP_GetTiming(PrimaryDisplayID, &current, &timing);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "Failed to retrieve current timing parameters for ID: " << PrimaryDisplayID << endl;
        OutputDebugString(log_s.str().c_str());
        return status;
    }

    // Try to set a new timing for the monitor so that LightBoost will turn on.
    NV_CUSTOM_DISPLAY lightboost = {};
    lightboost.version           = NV_CUSTOM_DISPLAY_VER;
    lightboost.timing            = timing;  // Copy everything from current for safety.

    lightboost.srcPartition = { 0.0f, 0.0f, 1.0f, 1.0f };
    lightboost.width        = 2560;
    lightboost.height       = 1440;
    lightboost.colorFormat  = NV_FORMAT_A8R8G8B8;  // means 32 bit color
    lightboost.xRatio       = 1.0f;
    lightboost.yRatio       = 1.0f;
    lightboost.depth        = 32;

    // The magic trick. Bump the VTotal by 5, and voila- LightBoost is on.
    // Also key is bumping the pixel clock to match the longer frame. Without
    // this the glasses were ever slightly out of sync with monitor.
    //
    // Sanity checks were done earlier for whether this is acceptable to dirk
    // around the timings of the monitor. If it's in MonitorTimings.ini and
    // the EDID matches, we consider it OK.

    NvU16 standard_vtotal = lightboost.timing.VTotal;
    NvU32 standard_pclk   = lightboost.timing.pclk;

    lightboost.timing.VTotal = standard_vtotal + 5;
    lightboost.timing.pclk   = standard_pclk * lightboost.timing.VTotal / standard_vtotal;  // deliberately no floats

    {
        wstringstream log_s;
        log_s << "** Switch VTotal from: " << standard_vtotal << " to: " << lightboost.timing.VTotal << endl;
        log_s << "** Switch pclk from: " << standard_pclk << " to: " << lightboost.timing.pclk << endl;
        log_s << "----------------------" << endl;
        OutputDebugString(log_s.str().c_str());
    }

    // Enable LightBoost timing. If this fails for some reason and returns an error, that is OK,
    // we won't error out.
    status = NvAPI_DISP_TryCustomDisplay(&PrimaryDisplayID, 1, &lightboost);
    if (status != NVAPI_OK)
    {
        wstringstream log_s;
        log_s << "Failed to retrieve current timing parameters for ID: " << PrimaryDisplayID << endl;
        OutputDebugString(log_s.str().c_str());
        return status;
    }

    OutputDebugString(L"->EnableLightBoost via NvAPI_DISP_TryCustomDisplay successfully.\n");

    return status;
}

// Restore the previous setting upon exit.

NvAPI_Status NvidiaShutterGlasses::DisableLightBoost()
{
    NvAPI_Status status;

    wstringstream log_s;
    log_s << "NvidiaShutterGlasses::DisableLightBoost(" << typeid(*this).name() << "@" << this << ")" << endl;
    OutputDebugString(log_s.str().c_str());

    if (PrimaryDisplayID == 0xDEADBEEF)
        return NVAPI_NVIDIA_DISPLAY_NOT_FOUND;

    status = NvAPI_DISP_RevertCustomDisplayTrial(&PrimaryDisplayID, 1);

    log_s << "-> DisableLightBoost by NvAPI_DISP_RevertCustomDisplayTrial: " << status << endl;
    OutputDebugString(log_s.str().c_str());

    return status;
}

// Prototype setup for AMD support, untested.
//
//#include <adl_sdk.h>
//
//void GetCurrentResolution_AMD()
//{
//    int            iAdapterIndex = 0;
//    ADLDisplayMode displayMode;
//    if (ADL_Display_Modes_Get(iAdapterIndex, -1, &displayMode) == ADL_OK)
//    {
//        std::cout << "AMD Current Resolution: " << displayMode.iXRes << "x" << displayMode.iYRes
//                  << " @ " << displayMode.iRefreshRate << "Hz" << endl;
//    }
//    else
//    {
//        std::cerr << "Failed to retrieve AMD display settings." << endl;
//    }
//}
//
//bool SetCustomResolution_AMD(int width, int height, int refreshRate)
//{
//    int            iAdapterIndex = 0;
//    ADLDisplayMode displayMode   = { width, height, refreshRate };
//
//    displayMode.iXRes           = width;
//    displayMode.iYRes           = height;
//    displayMode.iRefreshRate    = refreshRate;
//    displayMode.iTimingStandard = ADL_DL_TIMING_STANDARD_CVT;
//    displayMode.iHTotal += 10;  // Adjust horizontal total pixels
//    displayMode.iVTotal += 5;   // Adjust vertical total pixels
//
//    if (ADL_Display_Modes_Set(iAdapterIndex, -1, &displayMode) == ADL_OK)
//    {
//        std::cout << "Custom resolution set successfully on AMD." << endl;
//        return true;
//    }
//    else
//    {
//        std::cerr << "Failed to set custom resolution on AMD." << endl;
//        return false;
//    }
//}
