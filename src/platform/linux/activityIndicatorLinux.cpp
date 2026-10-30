#include "private/activityIndicatorPrivate.h"

namespace desktopctl {

ActivityIndicatorPrivate::ActivityIndicatorPrivate() = default;

ActivityIndicatorPrivate::~ActivityIndicatorPrivate() = default;

OperationResult ActivityIndicatorPrivate::startBackground()
{
    return {false, "The activity indicator is not implemented on Linux yet."};
}

bool ActivityIndicatorPrivate::isRunning() const
{
    return false;
}

OperationResult ActivityIndicatorPrivate::recordActivity()
{
    return {false, "The activity indicator is not implemented on Linux yet."};
}

OperationResult ActivityIndicatorPrivate::run()
{
    return {false, "The activity indicator is not implemented on Linux yet."};
}

OperationResult ActivityIndicatorPrivate::requestStop()
{
    return {false, "The activity indicator is not implemented on Linux yet."};
}

} // namespace desktopctl
