#pragma once

// Utils that are common.

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <iomanip>

inline std::ostringstream g_out;

//--------------------------------------------------------------------------------------
// Frank Luna style error checking for stuff that should never fail.
//--------------------------------------------------------------------------------------
inline void HR(
    HRESULT hresult)
{
    if (FAILED(hresult))
    {
        std::ostringstream error_log;

        error_log << __FILE__ << ", " << __LINE__ << ", HR: " << std::hex << hresult << std::dec << std::endl;
        OutputDebugStringA(error_log.str().c_str());

        DebugBreak();
        exit(hresult);
    }
}

//--------------------------------------------------------------------------------------
// Clean usage of ostringstream for output logging.
// So that the log is always cleared.
//--------------------------------------------------------------------------------------

inline void log()
{
    OutputDebugStringA(g_out.str().c_str());
    g_out.str("");
}
