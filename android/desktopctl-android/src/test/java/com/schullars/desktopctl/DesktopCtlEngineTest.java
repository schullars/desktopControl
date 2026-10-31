package com.schullars.desktopctl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public final class DesktopCtlEngineTest {
    @Rule
    public final TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Test
    public void helpDoesNotRequireSessionAndContainsAndroidExamples() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = createEngine(host)) {
            DesktopCtlCommandResult result = engine.execute("desktopctl -H");

            assertEquals(0, result.getExitCode());
            assertTrue(result.getStandardOutput().contains("desktopctl session start"));
            assertTrue(result.getStandardOutput().contains("admin@123#"));
            assertTrue(result.getStandardOutput().contains("three minutes"));
            assertEquals(0, host.startCount);
        }
    }

    @Test
    public void nonHelpCommandRequiresStartedSession() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = createEngine(host)) {
            DesktopCtlCommandResult result = engine.execute("desktopctl screenshot");

            assertEquals(3, result.getExitCode());
            assertTrue(result.getStandardError().contains("session start"));
        }
    }

    @Test
    public void commandSequenceUsesInjectedHostWithoutShellExecution() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = createEngine(host)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());
            assertEquals(0, engine.execute("desktopctl mouse move 120 240").getExitCode());
            assertEquals(0, engine.execute("desktopctl mouse click left 2").getExitCode());
            assertEquals(0, engine.execute(
                    "desktopctl keyboard hotkey CTRL+SHIFT ESC").getExitCode());
            assertEquals(0, engine.execute(
                    "desktopctl keyboard type \"admin@123#\"").getExitCode());
            assertEquals(0, engine.execute("desktopctl session stop").getExitCode());

            assertEquals(1, host.startCount);
            assertEquals(120, host.pointerX);
            assertEquals(240, host.pointerY);
            assertEquals(2, host.clickCount);
            assertEquals(Arrays.asList("CTRL", "SHIFT", "ESC"), host.hotkey);
            assertEquals("admin@123#", host.text);
            assertEquals(DesktopCtlHost.TextInputMode.HYBRID, host.textInputMode);
            assertEquals(1, host.stopCount);
        }
    }

    @Test
    public void tokenizerRejectsShellOperatorsButPreservesQuotedSymbols() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = createEngine(host)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());

            DesktopCtlCommandResult unsafe = engine.execute(
                    "desktopctl keyboard type hello;whoami");
            DesktopCtlCommandResult quoted = engine.execute(
                    "desktopctl keyboard type \"mail@example.com #1\"");

            assertEquals(2, unsafe.getExitCode());
            assertTrue(unsafe.getStandardError().contains("Shell operators"));
            assertEquals(0, quoted.getExitCode());
            assertEquals("mail@example.com #1", host.text);
        }
    }

    @Test
    public void screenshotReturnsStableJsonMetadataAndCleanupNotice() throws Exception {
        FakeHost host = new FakeHost();
        File cacheRoot = temporaryFolder.newFolder("screen-cache");
        File screenshotDirectory = new File(cacheRoot, "DesktopCTL/screenshots");
        assertTrue(screenshotDirectory.mkdirs());
        for (int index = 0; index < 50; ++index) {
            writeBytes(new File(screenshotDirectory, "existing_" + index + ".jpg"));
        }

        try (DesktopCtlEngine engine = new DesktopCtlEngine(host, cacheRoot, "test", 2_000L)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());
            DesktopCtlCommandResult result = engine.execute(
                    "desktopctl screenshot full proof.jpg");

            assertEquals(0, result.getExitCode());
            assertTrue(result.getStandardOutput().contains("\"success\":true"));
            assertTrue(result.getStandardOutput().contains("\"imageCount\":51"));
            assertTrue(result.getStandardOutput().contains("\"cleanupRecommended\":true"));
            assertTrue(result.getStandardOutput().contains("\"width\":1080"));
            assertTrue(result.getStandardOutput().contains("\"height\":1920"));
            assertTrue(result.getStandardOutput().contains("\"scale\":1.0"));
            assertTrue(new File(screenshotDirectory, "proof.jpg").isFile());
        }
    }

    @Test
    public void applicationCommandsUsePackageIdentifiers() throws Exception {
        FakeHost host = new FakeHost();
        host.applications = Arrays.asList(
                new DesktopCtlApplication("com.android.settings", "Settings"),
                new DesktopCtlApplication("org.mozilla.firefox", "Firefox"));
        try (DesktopCtlEngine engine = createEngine(host)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());
            DesktopCtlCommandResult listResult = engine.execute("desktopctl app list --json");
            DesktopCtlCommandResult openResult = engine.execute(
                    "desktopctl app open org.mozilla.firefox");

            assertEquals(0, listResult.getExitCode());
            assertTrue(listResult.getStandardOutput().contains("org.mozilla.firefox"));
            assertEquals(0, openResult.getExitCode());
            assertEquals("org.mozilla.firefox", host.openedApplication);
        }
    }

    @Test
    public void inactiveSessionStopsHostAndRequiresRestart() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = new DesktopCtlEngine(
                host,
                temporaryFolder.newFolder("timeout-cache"),
                "test",
                30L)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());

            long deadline = System.currentTimeMillis() + 2_000L;
            while (host.stopCount == 0 && System.currentTimeMillis() < deadline) {
                Thread.sleep(10L);
            }

            assertEquals(1, host.stopCount);
            assertFalse(host.ready);
            assertEquals(3, engine.execute("desktopctl app list").getExitCode());
        }
    }

    @Test
    public void unavailableHostStopsSessionResourcesImmediately() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = createEngine(host)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());
            host.ready = false;

            DesktopCtlCommandResult result = engine.execute("desktopctl version");

            assertEquals(3, result.getExitCode());
            assertEquals(1, host.stopCount);
        }
    }

    @Test
    public void acceptedCommandRefreshesInactivityDeadline() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = new DesktopCtlEngine(
                host,
                temporaryFolder.newFolder("activity-cache"),
                "test",
                100L)) {
            assertEquals(0, engine.execute("desktopctl session start").getExitCode());
            Thread.sleep(70L);
            assertEquals(0, engine.execute("desktopctl version").getExitCode());
            Thread.sleep(70L);
            assertEquals(0, host.stopCount);

            long deadline = System.currentTimeMillis() + 2_000L;
            while (host.stopCount == 0 && System.currentTimeMillis() < deadline) {
                Thread.sleep(10L);
            }
            assertEquals(1, host.stopCount);
        }
    }

    @Test
    public void jsonCodecEscapesControlCharactersAndQuotes() {
        assertEquals(
                "\"line\\n\\\"quoted\\\"\\\\tail\"",
                JsonCodec.quote("line\n\"quoted\"\\tail"));
    }

    @Test
    public void keepAliveSessionDoesNotExpire() throws Exception {
        FakeHost host = new FakeHost();
        try (DesktopCtlEngine engine = new DesktopCtlEngine(
                host,
                temporaryFolder.newFolder("keep-alive-cache"),
                "test",
                30L)) {
            assertEquals(0, engine.execute(
                    "desktopctl session start --keep-alive").getExitCode());
            Thread.sleep(100L);

            assertEquals(0, host.stopCount);
            assertEquals(0, engine.execute("desktopctl version").getExitCode());
        }
        assertEquals(1, host.stopCount);
    }

    private DesktopCtlEngine createEngine(FakeHost host) throws IOException {
        return new DesktopCtlEngine(
                host,
                temporaryFolder.newFolder(),
                "0.11.0-test",
                2_000L);
    }

    private static void writeBytes(File outputFile) throws IOException {
        try (FileOutputStream output = new FileOutputStream(outputFile)) {
            output.write("test".getBytes(StandardCharsets.US_ASCII));
        }
    }

    private static final class FakeHost implements DesktopCtlHost {
        private volatile boolean ready;
        private volatile int stopCount;
        private int startCount;
        private int pointerX;
        private int pointerY;
        private int clickCount;
        private List<String> hotkey = Collections.emptyList();
        private String text = "";
        private TextInputMode textInputMode;
        private List<DesktopCtlApplication> applications = Collections.emptyList();
        private String openedApplication = "";

        @Override
        public DesktopCtlOperationResult startSession(boolean keepAlive) {
            ++startCount;
            ready = true;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult stopSession() {
            ++stopCount;
            ready = false;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public boolean isSessionReady() {
            return ready;
        }

        @Override
        public DesktopCtlOperationResult movePointer(int x, int y) {
            pointerX = x;
            pointerY = y;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult movePointerRelative(int deltaX, int deltaY) {
            pointerX += deltaX;
            pointerY += deltaY;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult clickMouse(MouseButton button, int count) {
            clickCount = count;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult setMouseButton(MouseButton button, ButtonAction action) {
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
            hotkey = keys;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlOperationResult typeText(String value, TextInputMode mode) {
            text = value;
            textInputMode = mode;
            return DesktopCtlOperationResult.success("");
        }

        @Override
        public DesktopCtlScreenshotResult captureScreen(
                File outputFile,
                ScreenshotFormat format,
                int jpegQuality) {
            try {
                writeBytes(outputFile);
                return new DesktopCtlScreenshotResult(
                        DesktopCtlOperationResult.success(""),
                        1080,
                        1920,
                        1.0f,
                        0);
            } catch (IOException exception) {
                return new DesktopCtlScreenshotResult(
                        DesktopCtlOperationResult.failure(exception.getMessage()),
                        0,
                        0,
                        1.0f,
                        0);
            }
        }

        @Override
        public DesktopCtlApplicationListResult listApplications() {
            return new DesktopCtlApplicationListResult(
                    DesktopCtlOperationResult.success(""),
                    applications);
        }

        @Override
        public DesktopCtlOperationResult openApplication(String idOrName) {
            openedApplication = idOrName;
            return DesktopCtlOperationResult.success("");
        }
    }
}
