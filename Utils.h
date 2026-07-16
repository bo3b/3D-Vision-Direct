#pragma once

// Utils that are common.

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <iomanip>

inline std::ostringstream g_out;

enum eye
{
    left  = 0,
    right = 1
};

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

//--------------------------------------------------------------------------------------
// Stream manipulator that ends a log line: appends a newline, writes the
// accumulated text to the debugger (VS Output window), and clears the buffer.
// Collapses the "g_out << ... << std::endl; log();" two-liner into a single
// "g_out << ... << endlog;". Preserves g_out's formatting state, since it is
// the same persistent stream. Valid only on g_out (an ostringstream).
//--------------------------------------------------------------------------------------
inline std::ostream& endlog(std::ostream& os)
{
    os << '\n';
    auto& buffer = static_cast<std::ostringstream&>(os);
    OutputDebugStringA(buffer.str().c_str());
    buffer.str("");
    return os;
}
