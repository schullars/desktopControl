package com.schullars.desktopctl;

import java.io.File;
import java.util.Objects;

/**
 * Executes isolated Android DesktopCTL virtual commands against an injected host adapter.
 * The engine is thread-safe and serializes commands so platform operations retain command order.
 */
public final class DesktopCtlEngine implements AutoCloseable {
    public static final long DEFAULT_INACTIVITY_TIMEOUT_MILLISECONDS = 180_000L;
    private final SessionManager sessionManager;
    private final CommandDispatcher dispatcher;
    private boolean closed;

    /**
     * Creates an Android command engine with the standard three-minute inactivity timeout.
     *
     * @param host RustDesk or another Android host adapter.
     * @param cacheRoot writable app-private cache root used for stable screenshot storage.
     * @param version version reported by help and version commands.
     */
    public DesktopCtlEngine(DesktopCtlHost host, File cacheRoot, String version) {
        this(host, cacheRoot, version, DEFAULT_INACTIVITY_TIMEOUT_MILLISECONDS);
    }

    DesktopCtlEngine(
            DesktopCtlHost host,
            File cacheRoot,
            String version,
            long inactivityTimeoutMilliseconds) {
        Objects.requireNonNull(host, "host");
        Objects.requireNonNull(cacheRoot, "cacheRoot");
        Objects.requireNonNull(version, "version");
        if (inactivityTimeoutMilliseconds <= 0) {
            throw new IllegalArgumentException("The inactivity timeout must be positive.");
        }
        sessionManager = new SessionManager(host, inactivityTimeoutMilliseconds);
        dispatcher = new CommandDispatcher(
                host,
                sessionManager,
                new ScreenshotStore(host, cacheRoot),
                version);
    }

    /**
     * Parses and executes one DesktopCTL virtual command without invoking a system shell.
     *
     * @param commandLine command beginning with the literal executable name desktopctl.
     * @return process-compatible exit code, standard output, and standard error content.
     */
    public synchronized DesktopCtlCommandResult execute(String commandLine) {
        if (closed) {
            return DesktopCtlCommandResult.failure("The Android DesktopCTL engine is closed.");
        }
        CommandTokenizer.TokenizationResult tokenization = CommandTokenizer.tokenize(commandLine);
        if (!tokenization.isSuccessful()) {
            return DesktopCtlCommandResult.invalid(tokenization.getError());
        }
        return dispatcher.execute(tokenization.getTokens());
    }

    /**
     * Stops a running command session, removes platform resources, and rejects later commands.
     */
    @Override
    public synchronized void close() {
        if (closed) {
            return;
        }
        closed = true;
        sessionManager.close();
    }
}
