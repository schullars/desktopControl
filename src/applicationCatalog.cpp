#include "applicationCatalog.h"

#include "private/applicationCatalogPrivate.h"

#include <memory>

namespace desktopctl {

ApplicationCatalog::ApplicationCatalog()
    : _d(std::make_unique<ApplicationCatalogPrivate>())
{
}

ApplicationCatalog::~ApplicationCatalog() = default;

ApplicationListResult ApplicationCatalog::listApplications()
{
    return _d->listApplications();
}

OperationResult ApplicationCatalog::openApplication(const std::string& idOrName)
{
    return _d->openApplication(idOrName);
}

} // namespace desktopctl
