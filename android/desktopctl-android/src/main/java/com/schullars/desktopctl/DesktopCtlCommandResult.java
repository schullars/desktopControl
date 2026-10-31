package com.schullars.desktopctl;

/**
 * Contains the process-compatible result of one DesktopCTL command.
 */
public final class DesktopCtlCommandResult {
    private final int exitCode;
    private final String standardOutput;
    private final String standardError;

    private DesktopCtlCommandResult(int exitCode, String standardOutput, String standardError) {
        this.exitCode = exitCode;
        this.standardOutput = standardOutput;
        this.standardError = standardError;
    }

    /**
     * Creates a successful result whose content belongs on standard output.
     *
     * @param output UTF-8 text without a required trailing newline.
     * @return a result with exit code zero.
     */
    public static DesktopCtlCommandResult success(String output) {
        return new DesktopCtlCommandResult(0, output, "");
    }

    /**
     * Creates an invalid-command result.
     *
     * @param message stable English error text.
     * @return a result with exit code two.
     */
    public static DesktopCtlCommandResult invalid(String message) {
        return new DesktopCtlCommandResult(2, "", message);
    }

    /**
     * Creates a platform-operation failure result.
     *
     * @param message stable English error text.
     * @return a result with exit code three.
     */
    public static DesktopCtlCommandResult failure(String message) {
        return new DesktopCtlCommandResult(3, "", message);
    }

    /**
     * Returns the process-compatible exit code.
     *
     * @return zero for success, two for invalid input, or three for operation failure.
     */
    public int getExitCode() {
        return exitCode;
    }

    /**
     * Returns UTF-8 standard output content.
     *
     * @return an empty string when the command did not write standard output.
     */
    public String getStandardOutput() {
        return standardOutput;
    }

    /**
     * Returns UTF-8 standard error content.
     *
     * @return an empty string when the command did not write standard error.
     */
    public String getStandardError() {
        return standardError;
    }
}
