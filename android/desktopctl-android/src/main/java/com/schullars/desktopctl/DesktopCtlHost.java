package com.schullars.desktopctl;

import java.io.File;
import java.util.List;

/**
 * Supplies the Android platform operations required by the isolated command engine.
 * Implementations must marshal accessibility and UI work to the appropriate Android thread.
 */
public interface DesktopCtlHost {
    enum MouseButton {
        LEFT,
        RIGHT,
        MIDDLE
    }

    enum ButtonAction {
        DOWN,
        UP
    }

    enum ScrollAxis {
        VERTICAL,
        HORIZONTAL
    }

    enum KeyAction {
        PRESS,
        DOWN,
        UP
    }

    enum TextInputMode {
        HYBRID,
        PHYSICAL,
        UNICODE
    }

    enum ScreenshotFormat {
        JPEG,
        BMP
    }

    /**
     * Starts the capture, input, and visible-indicator resources required by a command session.
     *
     * @param keepAlive true when automatic inactivity shutdown is disabled.
     * @return success only after the green indicator and required services are ready.
     */
    DesktopCtlOperationResult startSession(boolean keepAlive);

    /**
     * Stops platform session resources and removes the visible indicator.
     *
     * @return the platform shutdown result.
     */
    DesktopCtlOperationResult stopSession();

    /**
     * Reports whether capture, input, and the visible indicator remain ready.
     *
     * @return true only while the platform session is usable.
     */
    boolean isSessionReady();

    DesktopCtlOperationResult movePointer(int x, int y);

    DesktopCtlOperationResult movePointerRelative(int deltaX, int deltaY);

    DesktopCtlOperationResult clickMouse(MouseButton button, int count);

    DesktopCtlOperationResult setMouseButton(MouseButton button, ButtonAction action);

    DesktopCtlOperationResult scroll(int delta, ScrollAxis axis);

    DesktopCtlOperationResult sendKey(String key, KeyAction action);

    DesktopCtlOperationResult sendHotkey(List<String> keys);

    DesktopCtlOperationResult typeText(String text, TextInputMode mode);

    /**
     * Saves the next current RustDesk screen frame to the requested app-private file.
     *
     * @param outputFile canonical output file selected by the command engine.
     * @param format encoder selected by the file extension.
     * @param jpegQuality JPEG quality from zero through one hundred; ignored for BMP.
     * @return capture status and the exact coordinate metadata of the saved frame.
     */
    DesktopCtlScreenshotResult captureScreen(
            File outputFile,
            ScreenshotFormat format,
            int jpegQuality);

    DesktopCtlApplicationListResult listApplications();

    DesktopCtlOperationResult openApplication(String idOrName);
}
