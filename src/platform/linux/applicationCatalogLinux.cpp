#include "private/applicationCatalogPrivate.h"

namespace desktopctl {

ApplicationCatalogPrivate::ApplicationCatalogPrivate() = default;

ApplicationCatalogPrivate::~ApplicationCatalogPrivate() = default;

ApplicationListResult ApplicationCatalogPrivate::listApplications()
{
    return {{false, "Application discovery is not implemented on Linux yet."}, {}};
}

OperationResult ApplicationCatalogPrivate::openApplication(const std::string&)
{
    return {false, "Application launching is not implemented on Linux yet."};
}

} // namespace desktopctl
