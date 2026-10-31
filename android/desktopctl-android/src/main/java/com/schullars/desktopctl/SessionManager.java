package com.schullars.desktopctl;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

final class SessionManager implements AutoCloseable {
    private final DesktopCtlHost host;
    private final long inactivityTimeoutMilliseconds;
    private final ScheduledExecutorService scheduler;
    private ScheduledFuture<?> timeoutTask;
    private boolean active;
    private boolean keepAlive;

    SessionManager(DesktopCtlHost host, long inactivityTimeoutMilliseconds) {
        this.host = host;
        this.inactivityTimeoutMilliseconds = inactivityTimeoutMilliseconds;
        this.scheduler = Executors.newSingleThreadScheduledExecutor(new DaemonThreadFactory());
    }

    synchronized DesktopCtlOperationResult start(boolean requestedKeepAlive) {
        if (active) {
            if (keepAlive != requestedKeepAlive) {
                return DesktopCtlOperationResult.failure(
                        "Stop the current session before changing keep-alive mode.");
            }
            recordActivityLocked();
            return DesktopCtlOperationResult.success("Session is already running.");
        }

        DesktopCtlOperationResult startResult = host.startSession(requestedKeepAlive);
        if (startResult == null) {
            return DesktopCtlOperationResult.failure(
                    "The Android host returned no session-start result.");
        }
        if (!startResult.isSuccessful()) {
            return startResult;
        }
        active = true;
        keepAlive = requestedKeepAlive;
        recordActivityLocked();
        String message = startResult.getMessage().isEmpty()
                ? "Android DesktopCTL session started."
                : startResult.getMessage();
        return DesktopCtlOperationResult.success(message);
    }

    synchronized DesktopCtlOperationResult stop() {
        if (!active) {
            return DesktopCtlOperationResult.failure("No Android DesktopCTL session is running.");
        }
        active = false;
        keepAlive = false;
        cancelTimeoutLocked();
        DesktopCtlOperationResult stopResult = host.stopSession();
        if (stopResult == null) {
            return DesktopCtlOperationResult.failure(
                    "The Android host returned no session-stop result.");
        }
        if (!stopResult.isSuccessful()) {
            return stopResult;
        }
        String message = stopResult.getMessage().isEmpty()
                ? "Android DesktopCTL session stopped."
                : stopResult.getMessage();
        return DesktopCtlOperationResult.success(message);
    }

    boolean recordActivity() {
        synchronized (this) {
            if (!active) {
                return false;
            }
            if (host.isSessionReady()) {
                recordActivityLocked();
                return true;
            }
            active = false;
            keepAlive = false;
            cancelTimeoutLocked();
        }
        host.stopSession();
        return false;
    }

    private void recordActivityLocked() {
        cancelTimeoutLocked();
        if (active && !keepAlive) {
            timeoutTask = scheduler.schedule(
                    this::expire,
                    inactivityTimeoutMilliseconds,
                    TimeUnit.MILLISECONDS);
        }
    }

    private void expire() {
        synchronized (this) {
            if (!active || keepAlive) {
                return;
            }
            active = false;
            timeoutTask = null;
        }
        host.stopSession();
    }

    private void cancelTimeoutLocked() {
        if (timeoutTask != null) {
            timeoutTask.cancel(false);
            timeoutTask = null;
        }
    }

    @Override
    public void close() {
        boolean mustStop;
        synchronized (this) {
            mustStop = active;
            active = false;
            keepAlive = false;
            cancelTimeoutLocked();
        }
        if (mustStop) {
            host.stopSession();
        }
        scheduler.shutdownNow();
    }

    private static final class DaemonThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(Runnable runnable) {
            Thread thread = new Thread(runnable, "desktopctl-android-session-timeout");
            thread.setDaemon(true);
            return thread;
        }
    }
}
