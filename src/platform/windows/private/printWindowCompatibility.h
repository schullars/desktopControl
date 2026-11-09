#pragma once
#include <Windows.h>
#include <string>
namespace desktopctl {
template<class Print, class Clear>
bool printWindowCompatible(Print print, Clear clear, std::string& error)
{
    SetLastError(ERROR_SUCCESS);
    if (print(PW_RENDERFULLCONTENT)) return true;
    const DWORD first = GetLastError();
    if (first != ERROR_INVALID_PARAMETER) {
        error = "PrintWindow failed with Win32 error " + std::to_string(first) + ".";
        return false;
    }
    if (!clear()) {
        error = "PrintWindow failed with Win32 error 87; unable to clear fallback bitmap.";
        return false;
    }
    SetLastError(ERROR_SUCCESS);
    if (print(0)) return true;
    const DWORD second = GetLastError();
    error = "PrintWindow failed with Win32 error 87; flags=0 retry failed with Win32 error "
        + std::to_string(second) + ".";
    return false;
}
}
