package com.schullars.desktopctl;

/**
 * Contains Android host capture status and the coordinate-space metadata of the saved image.
 */
public final class DesktopCtlScreenshotResult {
    private final DesktopCtlOperationResult operation;
    private final int width;
    private final int height;
    private final float scale;
    private final int rotation;

    /**
     * Creates a screenshot result.
     *
     * @param operation capture status.
     * @param width saved image width in pixels.
     * @param height saved image height in pixels.
     * @param scale multiplier from command coordinates to injected screen coordinates.
     * @param rotation clockwise display rotation in degrees.
     */
    public DesktopCtlScreenshotResult(
            DesktopCtlOperationResult operation,
            int width,
            int height,
            float scale,
            int rotation) {
        this.operation = operation;
        this.width = width;
        this.height = height;
        this.scale = scale;
        this.rotation = rotation;
    }

    /**
     * Returns the platform capture status.
     *
     * @return a non-null operation result.
     */
    public DesktopCtlOperationResult getOperation() {
        return operation;
    }

    /**
     * Returns the saved image width.
     *
     * @return width in screenshot-coordinate pixels.
     */
    public int getWidth() {
        return width;
    }

    /**
     * Returns the saved image height.
     *
     * @return height in screenshot-coordinate pixels.
     */
    public int getHeight() {
        return height;
    }

    /**
     * Returns the multiplier from screenshot coordinates to physical display coordinates.
     *
     * @return a positive coordinate scale.
     */
    public float getScale() {
        return scale;
    }

    /**
     * Returns the clockwise physical display rotation.
     *
     * @return zero, 90, 180, or 270 degrees.
     */
    public int getRotation() {
        return rotation;
    }
}
