package com.schullars.desktopctl;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public final class DesktopCtlAndroidTest {
    private File testRoot;

    @Before
    public void setUp() {
        testRoot = new File(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir(),
                "desktopctl-android-test");
        assertTrue(testRoot.isDirectory() || testRoot.mkdirs());
    }

    @Test
    public void testEngineSessionGateAndUnicodeText() {
        RecordingHost host = new RecordingHost();
        try (DesktopCtlEngine engine = new DesktopCtlEngine(host, testRoot, "android-test")) {
            DesktopCtlCommandResult gated = engine.execute(
                    "desktopctl keyboard type-unicode admin@123#");
            assertEquals(3, gated.getExitCode());

            DesktopCtlCommandResult started = engine.execute("desktopctl session start");
            assertEquals(0, started.getExitCode());

            DesktopCtlCommandResult typed = engine.execute(
                    "desktopctl keyboard type-unicode \"admin@123#\"");
            assertEquals(0, typed.getExitCode());
            assertEquals("admin@123#", host.typedText);
            assertEquals(DesktopCtlHost.TextInputMode.UNICODE, host.textInputMode);
        }
    }

    @Test
    public void testAndroidApplicationCatalogReturnsLauncherApps() {
        AndroidApplicationCatalog catalog = new AndroidApplicationCatalog(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        DesktopCtlApplicationListResult result = catalog.listApplications();
        assertTrue(result.getOperation().isSuccessful());
        assertFalse(result.getApplications().isEmpty());
        for (DesktopCtlApplication application : result.getApplications()) {
            assertNotNull(application.getId());
            assertFalse(application.getId().isEmpty());
        }
    }

    @Test
    public void testAndroidImageWriterCreatesJpegAndBmp() throws Exception {
        Bitmap bitmap = Bitmap.createBitmap(4, 3, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.rgb(12, 160, 72));
        try {
            File jpeg = new File(testRoot, "sample.jpg");
            DesktopCtlOperationResult jpegResult = AndroidImageWriter.write(
                    bitmap,
                    jpeg,
                    DesktopCtlHost.ScreenshotFormat.JPEG,
                    95);
            assertTrue(jpegResult.getMessage(), jpegResult.isSuccessful());
            assertFilePrefix(jpeg, 0xff, 0xd8);

            File bmp = new File(testRoot, "sample.bmp");
            DesktopCtlOperationResult bmpResult = AndroidImageWriter.write(
                    bitmap,
                    bmp,
                    DesktopCtlHost.ScreenshotFormat.BMP,
                    95);
            assertTrue(bmpResult.getMessage(), bmpResult.isSuccessful());
            assertFilePrefix(bmp, 'B', 'M');
        } finally {
            bitmap.recycle();
        }
    }

    private void assertFilePrefix(File file, int first, int second) throws Exception {
        assertTrue(file.isFile());
        try (FileInputStream input = new FileInputStream(file)) {
            assertEquals(first, input.read());
            assertEquals(second, input.read());
        }
    }

    private static final class RecordingHost implements DesktopCtlHost {
        private boolean ready;
        private String typedText = "";
        private TextInputMode textInputMode = TextInputMode.HYBRID;

        @Override
        public DesktopCtlOperationResult startSession(boolean keepAlive) {
            ready = true;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult stopSession() {
            ready = false;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public boolean isSessionReady() {
            return ready;
        }

        @Override
        public DesktopCtlOperationResult movePointer(int x, int y) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult movePointerRelative(int deltaX, int deltaY) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult clickMouse(MouseButton button, int count) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult setMouseButton(
                MouseButton button,
                ButtonAction action) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult scroll(int delta, ScrollAxis axis) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult sendKey(String key, KeyAction action) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult sendHotkey(List<String> keys) {
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult typeText(String text, TextInputMode mode) {
            typedText = text;
            textInputMode = mode;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlScreenshotResult captureScreen(
                File outputFile,
                ScreenshotFormat format,
                int jpegQuality) {
            return new DesktopCtlScreenshotResult(
                    DesktopCtlOperationResult.failure("Not used by this test."),
                    0,
                    0,
                    1.0f,
                    0);
        }

        @Override
        public DesktopCtlApplicationListResult listApplications() {
            return new DesktopCtlApplicationListResult(
                    DesktopCtlOperationResult.success(""),
                    new ArrayList<>());
        }

        @Override
        public DesktopCtlOperationResult openApplication(String idOrName) {
            return DesktopCtlOperationResult.success("");
        }
    }
}
