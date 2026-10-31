#pragma once

#include "operationResult.h"

#include <memory>

namespace desktopctl {

class ActivityIndicatorPrivate;

class ActivityIndicator final {
public:
    /// <summary>
    /// Creates the desktop activity indicator for the current platform.
    /// </summary>
    ActivityIndicator();
    ~ActivityIndicator();

    ActivityIndicator(const ActivityIndicator&) = delete;
    ActivityIndicator& operator=(const ActivityIndicator&) = delete;

    /// <summary>
    /// Starts the indicator in a detached background process and waits until it is ready.
    /// </summary>
    /// <param name="keepAlive">True disables the inactivity timeout until an explicit stop,
    /// logout, process exit, or operating-system shutdown.</param>
    /// <returns>Success when the indicator is running or was already running.</returns>
    OperationResult startBackground(bool keepAlive = false);

    /// <summary>
    /// Reports whether a fully initialized indicator owns the current login-session state.
    /// </summary>
    /// <returns>True only while the background indicator is ready and alive.</returns>
    bool isRunning() const;

    /// <summary>
    /// Refreshes the inactivity deadline owned by the running background session.
    /// </summary>
    /// <returns>Success when the activity signal reaches the current session.</returns>
    OperationResult recordActivity();

    /// <summary>
    /// Runs the green screen border and pointer halo until a stop request or Ctrl+C.
    /// </summary>
    /// <param name="keepAlive">True disables the three-minute inactivity timeout.</param>
    /// <returns>The shutdown result. This call blocks while the indicator is running.</returns>
    OperationResult run(bool keepAlive = false);

    /// <summary>
    /// Requests that the indicator running in the current login session stop.
    /// </summary>
    /// <returns>Success when the stop request is sent; failure when no instance is active.</returns>
    OperationResult requestStop();

private:
    std::unique_ptr<ActivityIndicatorPrivate> _d;
};

} // namespace desktopctl
