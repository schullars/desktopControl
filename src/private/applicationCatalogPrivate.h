#pragma once

#include "applicationCatalog.h"

#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace desktopctl {

class ApplicationCatalogPrivate final {
public:
    ApplicationCatalogPrivate();
    ~ApplicationCatalogPrivate();

    ApplicationListResult listApplications();
    OperationResult openApplication(const std::string& idOrName);

private:
#if defined(_WIN32)
    struct ApplicationRecord {
        ApplicationInfo info;
        std::wstring applicationUserModelId;
        std::wstring launchTarget;
    };

    static std::string lowerAscii(const std::string& value);
    static std::wstring shellItemName(PCIDLIST_ABSOLUTE item, SIGDN form);
    static std::wstring applicationUserModelId(PCIDLIST_ABSOLUTE item);
    OperationResult discover(std::vector<ApplicationRecord>& applications);

    bool comAvailable_ = false;
    bool uninitializeCom_ = false;
#elif defined(__linux__)
    struct ApplicationRecord {
        ApplicationInfo info;
        std::string desktopId;
    };

    static std::string lowerAscii(const std::string& value);
    OperationResult discover(std::vector<ApplicationRecord>& applications);
#endif
};

} // namespace desktopctl
