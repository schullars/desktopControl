#include "activityIndicator.h"

#include "private/activityIndicatorPrivate.h"

#include <memory>

namespace desktopctl {

ActivityIndicator::ActivityIndicator()
    : _d(std::make_unique<ActivityIndicatorPrivate>())
{
}

ActivityIndicator::~ActivityIndicator() = default;

OperationResult ActivityIndicator::startBackground(bool keepAlive)
{
    return _d->startBackground(keepAlive);
}

bool ActivityIndicator::isRunning() const
{
    return _d->isRunning();
}

OperationResult ActivityIndicator::recordActivity()
{
    return _d->recordActivity();
}

OperationResult ActivityIndicator::run(bool keepAlive)
{
    return _d->run(keepAlive);
}

OperationResult ActivityIndicator::requestStop()
{
    return _d->requestStop();
}

} // namespace desktopctl
