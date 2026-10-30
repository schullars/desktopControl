#pragma once

#include <string>

namespace desktopctl {

class WindowsText final {
public:
    static std::string utf8FromWide(const std::wstring& value);
    static std::wstring wideFromUtf8(const std::string& value);
};

} // namespace desktopctl
