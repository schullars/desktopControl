package com.schullars.desktopctl;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

final class ScreenshotStore {
    private static final int JPEG_QUALITY = 95;
    private final DesktopCtlHost host;
    private final File screenshotDirectory;

    ScreenshotStore(DesktopCtlHost host, File cacheRoot) {
        this.host = host;
        this.screenshotDirectory = new File(cacheRoot, "DesktopCTL/screenshots");
    }

    DesktopCtlCommandResult capture(String outputArgument) {
        File outputFile;
        try {
            outputFile = resolveOutput(outputArgument);
        } catch (IOException exception) {
            return DesktopCtlCommandResult.invalid(exception.getMessage());
        }

        if (!screenshotDirectory.isDirectory() && !screenshotDirectory.mkdirs()) {
            return DesktopCtlCommandResult.failure(
                    "Unable to create the Android screenshot directory.");
        }

        DesktopCtlHost.ScreenshotFormat format = formatFor(outputFile.getName());
        if (format == null) {
            return DesktopCtlCommandResult.invalid(
                    "Android screenshots require a .jpg, .jpeg, or .bmp extension.");
        }

        DesktopCtlScreenshotResult captureResult = host.captureScreen(
                outputFile,
                format,
                JPEG_QUALITY);
        if (captureResult == null || captureResult.getOperation() == null) {
            return failureJson("The Android host returned no screenshot result.");
        }
        if (!captureResult.getOperation().isSuccessful()) {
            return failureJson(captureResult.getOperation().getMessage());
        }
        if (!outputFile.isFile()) {
            return failureJson("The Android host did not create the screenshot file.");
        }

        Long imageCount = countImages();
        boolean cleanupRecommended = imageCount != null && imageCount > 50;
        String message = createMessage(imageCount, cleanupRecommended);
        String output = "{"
                + "\"success\":true,"
                + "\"path\":" + JsonCodec.quote(outputFile.getAbsolutePath()) + ","
                + "\"directory\":" + JsonCodec.quote(screenshotDirectory.getAbsolutePath()) + ","
                + "\"imageCount\":" + (imageCount == null ? "null" : imageCount) + ","
                + "\"cleanupRecommended\":" + cleanupRecommended + ","
                + "\"width\":" + captureResult.getWidth() + ","
                + "\"height\":" + captureResult.getHeight() + ","
                + "\"scale\":" + Float.toString(captureResult.getScale()) + ","
                + "\"rotation\":" + captureResult.getRotation() + ","
                + "\"message\":" + JsonCodec.quote(message)
                + "}";
        return DesktopCtlCommandResult.success(output);
    }

    File getScreenshotDirectory() {
        return screenshotDirectory;
    }

    private File resolveOutput(String outputArgument) throws IOException {
        File canonicalDirectory = screenshotDirectory.getCanonicalFile();
        if (outputArgument == null || outputArgument.isEmpty()) {
            String timestamp = new SimpleDateFormat(
                    "yyyyMMdd_HHmmss_SSS",
                    Locale.US).format(new Date());
            return new File(canonicalDirectory, "screenshot_" + timestamp + ".jpg")
                    .getCanonicalFile();
        }

        File requested = new File(outputArgument);
        File resolved = requested.isAbsolute()
                ? requested.getCanonicalFile()
                : new File(canonicalDirectory, outputArgument).getCanonicalFile();
        String directoryPath = canonicalDirectory.getPath() + File.separator;
        if (!resolved.getPath().startsWith(directoryPath)) {
            throw new IOException(
                    "Android screenshot output must remain inside the app screenshot directory.");
        }
        if (resolved.getParentFile() == null
                || !resolved.getParentFile().equals(canonicalDirectory)) {
            throw new IOException(
                    "Android screenshot output cannot create nested directories.");
        }
        return resolved;
    }

    private Long countImages() {
        File[] entries = screenshotDirectory.listFiles(file ->
                file.isFile() && formatFor(file.getName()) != null);
        return entries == null ? null : (long) entries.length;
    }

    private static DesktopCtlHost.ScreenshotFormat formatFor(String name) {
        String lowerName = name.toLowerCase(Locale.ROOT);
        if (lowerName.endsWith(".jpg") || lowerName.endsWith(".jpeg")) {
            return DesktopCtlHost.ScreenshotFormat.JPEG;
        }
        if (lowerName.endsWith(".bmp")) {
            return DesktopCtlHost.ScreenshotFormat.BMP;
        }
        return null;
    }

    private static String createMessage(Long imageCount, boolean cleanupRecommended) {
        if (imageCount == null) {
            return "Screenshot saved. The output directory image count is unavailable.";
        }
        String message = "Screenshot saved. The output directory contains "
                + imageCount
                + " image file(s).";
        if (cleanupRecommended) {
            message += " More than 50 images are present; remove screenshots you no longer need.";
        }
        return message;
    }

    private static DesktopCtlCommandResult failureJson(String message) {
        String output = "{"
                + "\"success\":false,"
                + "\"path\":null,"
                + "\"directory\":null,"
                + "\"imageCount\":null,"
                + "\"cleanupRecommended\":false,"
                + "\"message\":" + JsonCodec.quote(message)
                + "}";
        return DesktopCtlCommandResult.failure(output);
    }
}
