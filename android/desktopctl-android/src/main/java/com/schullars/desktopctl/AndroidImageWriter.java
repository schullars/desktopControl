package com.schullars.desktopctl;

import android.graphics.Bitmap;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * Writes RustDesk screen-frame bitmaps using DesktopCTL's Android image policy.
 */
public final class AndroidImageWriter {
    private AndroidImageWriter() {
    }

    /**
     * Writes a bitmap as quality-controlled JPEG or uncompressed 32-bit BMP.
     *
     * @param bitmap source bitmap in display orientation.
     * @param outputFile app-private destination file.
     * @param format requested encoder.
     * @param jpegQuality JPEG quality from zero through one hundred; ignored for BMP.
     * @return the encoder and filesystem result.
     */
    public static DesktopCtlOperationResult write(
            Bitmap bitmap,
            File outputFile,
            DesktopCtlHost.ScreenshotFormat format,
            int jpegQuality) {
        if (bitmap == null || bitmap.isRecycled()) {
            return DesktopCtlOperationResult.failure("The Android screenshot bitmap is unavailable.");
        }
        File parent = outputFile.getParentFile();
        if (parent == null || (!parent.isDirectory() && !parent.mkdirs())) {
            return DesktopCtlOperationResult.failure(
                    "Unable to create the Android screenshot directory.");
        }
        try (OutputStream output = new BufferedOutputStream(new FileOutputStream(outputFile))) {
            if (format == DesktopCtlHost.ScreenshotFormat.JPEG) {
                int boundedQuality = Math.max(0, Math.min(100, jpegQuality));
                if (!bitmap.compress(Bitmap.CompressFormat.JPEG, boundedQuality, output)) {
                    return DesktopCtlOperationResult.failure("Android JPEG encoding failed.");
                }
            } else {
                writeBitmap(bitmap, output);
            }
            output.flush();
            return DesktopCtlOperationResult.success("");
        } catch (IOException | RuntimeException exception) {
            return DesktopCtlOperationResult.failure(
                    "Unable to save the Android screenshot: " + exception.getMessage());
        }
    }

    private static void writeBitmap(Bitmap bitmap, OutputStream output) throws IOException {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        long pixelBytes = (long) width * height * 4L;
        long fileSize = 54L + pixelBytes;
        if (width <= 0 || height <= 0 || fileSize > Integer.MAX_VALUE) {
            throw new IOException("The Android bitmap is too large for BMP output.");
        }

        output.write('B');
        output.write('M');
        writeInt32(output, (int) fileSize);
        writeInt16(output, 0);
        writeInt16(output, 0);
        writeInt32(output, 54);
        writeInt32(output, 40);
        writeInt32(output, width);
        writeInt32(output, height);
        writeInt16(output, 1);
        writeInt16(output, 32);
        writeInt32(output, 0);
        writeInt32(output, (int) pixelBytes);
        writeInt32(output, 2835);
        writeInt32(output, 2835);
        writeInt32(output, 0);
        writeInt32(output, 0);

        int[] row = new int[width];
        byte[] encodedRow = new byte[width * 4];
        for (int y = height - 1; y >= 0; --y) {
            bitmap.getPixels(row, 0, width, 0, y, width, 1);
            for (int x = 0; x < width; ++x) {
                int color = row[x];
                int offset = x * 4;
                encodedRow[offset] = (byte) (color & 0xff);
                encodedRow[offset + 1] = (byte) ((color >>> 8) & 0xff);
                encodedRow[offset + 2] = (byte) ((color >>> 16) & 0xff);
                encodedRow[offset + 3] = 0;
            }
            output.write(encodedRow);
        }
    }

    private static void writeInt16(OutputStream output, int value) throws IOException {
        output.write(value & 0xff);
        output.write((value >>> 8) & 0xff);
    }

    private static void writeInt32(OutputStream output, int value) throws IOException {
        output.write(value & 0xff);
        output.write((value >>> 8) & 0xff);
        output.write((value >>> 16) & 0xff);
        output.write((value >>> 24) & 0xff);
    }
}
