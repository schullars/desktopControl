#include "activityIndicator.h"

#include "private/activityIndicatorPrivate.h"

#include <memory>

namespace desktopctl {

ActivityIndicator::ActivityIndicator()
    : _d(std::make_unique<ActivityIndicatorPrivate>())
{
}

ActivityIndicator::~ActivityIndicator() = default;

OperationResult ActivityIndicator::startBackground()
{
    return _d->startBackground();
}

bool ActivityIndicator::isRunning() const
{
    return _d->isRunning();
}

OperationResult ActivityIndicator::recordActivity()
{
    return _d->recordActivity();
}

OperationResult ActivityIndicator::run()
{
    return _d->run();
}

OperationResult ActivityIndicator::requestStop()
{
    return _d->requestStop();
}

} // namespace desktopctl
