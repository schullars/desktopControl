#pragma once

#include "operationResult.h"

#include <memory>
#include <string>
#include <vector>

namespace desktopctl {

struct ApplicationInfo {
    std::string id;
    std::string name;
};

struct ApplicationListResult {
    OperationResult operation;
    std::vector<ApplicationInfo> applications;
};

class ApplicationCatalogPrivate;

class ApplicationCatalog final {
public:
    /// <summary>
    /// Creates the application catalog for the current platform.
    /// </summary>
    ApplicationCatalog();
    ~ApplicationCatalog();

    ApplicationCatalog(const ApplicationCatalog&) = delete;
    ApplicationCatalog& operator=(const ApplicationCatalog&) = delete;

    /// <summary>
    /// Lists applications that the current interactive user can launch from the system catalog.
    /// </summary>
    /// <returns>The operation status and stable application IDs and names sorted by name.</returns>
    ApplicationListResult listApplications();

    /// <summary>
    /// Opens an application by stable ID or unique display name.
    /// </summary>
    /// <param name="idOrName">An ID returned by listApplications or a unique full name.</param>
    /// <returns>Success when the shell accepts the request. The call does not wait for exit.</returns>
    OperationResult openApplication(const std::string& idOrName);

private:
    std::unique_ptr<ApplicationCatalogPrivate> _d;
};

} // namespace desktopctl
