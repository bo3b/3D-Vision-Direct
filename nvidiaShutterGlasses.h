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
#include <iostream>

#include "nvapi.h"
#include "shutterGlasses.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class NvidiaShutterGlasses : public ShutterGlasses
{
public:
    NvidiaShutterGlasses();
    ~NvidiaShutterGlasses();

    void WakeEmitter();
    void InitEmitter();
    void ClearEmitter();

    void ToggleEyes();
    void SetLeftEye();
    void SetRightEye();

    NvAPI_Status GetCurrentResolution_NVIDIA();
    NvAPI_Status EnableLightBoost_NVIDIA();
    NvAPI_Status DisableLightBoost_NVIDIA();

private:
    struct monitor_info
    {
        std::string monitor_name;
        std::string monitor_EDID;
        float       refresh_rate;
        float       timer_x_us;
        float       timer_y_us;
        float       timer_z_us;
        float       timer_w_us;
    };
    std::vector<monitor_info> monitors;

    HANDLE                   pipe_usb_init  = INVALID_HANDLE_VALUE;
    HANDLE                   pipe_usb_swaps = INVALID_HANDLE_VALUE;

    NvU32 PrimaryDisplayID = 0xDEADBEEF;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
