package com.schullars.desktopctl;

/**
 * Describes the outcome of one Android host operation.
 */
public final class DesktopCtlOperationResult {
    private final boolean successful;
    private final String message;

    private DesktopCtlOperationResult(boolean successful, String message) {
        this.successful = successful;
        this.message = message;
    }

    /**
     * Creates a successful operation result.
     *
     * @param message optional stable English status text.
     * @return a successful result.
     */
    public static DesktopCtlOperationResult success(String message) {
        return new DesktopCtlOperationResult(true, message == null ? "" : message);
    }

    /**
     * Creates a failed operation result.
     *
     * @param message stable English failure text.
     * @return a failed result.
     */
    public static DesktopCtlOperationResult failure(String message) {
        String safeMessage = message == null || message.isEmpty()
                ? "The Android host operation failed."
                : message;
        return new DesktopCtlOperationResult(false, safeMessage);
    }

    /**
     * Returns whether the host accepted and completed the operation.
     *
     * @return true on success.
     */
    public boolean isSuccessful() {
        return successful;
    }

    /**
     * Returns stable English status or failure text.
     *
     * @return a non-null string.
     */
    public String getMessage() {
        return message;
    }
}
