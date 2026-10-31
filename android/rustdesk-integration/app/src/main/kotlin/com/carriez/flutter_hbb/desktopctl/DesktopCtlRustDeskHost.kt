package com.carriez.flutter_hbb.desktopctl

import android.annotation.SuppressLint
import android.content.Context
import android.os.Build
import android.view.WindowManager
import com.carriez.flutter_hbb.InputService
import com.carriez.flutter_hbb.LEFT_DOWN
import com.carriez.flutter_hbb.LEFT_UP
import com.carriez.flutter_hbb.MOUSE_TYPE_WHEEL
import com.carriez.flutter_hbb.MainService
import com.carriez.flutter_hbb.RIGHT_UP
import com.carriez.flutter_hbb.SCREEN_INFO
import com.carriez.flutter_hbb.WHEEL_BUTTON_DOWN
import com.carriez.flutter_hbb.WHEEL_BUTTON_UP
import com.schullars.desktopctl.AndroidApplicationCatalog
import com.schullars.desktopctl.AndroidImageWriter
import com.schullars.desktopctl.DesktopCtlApplicationListResult
import com.schullars.desktopctl.DesktopCtlHost
import com.schullars.desktopctl.DesktopCtlOperationResult
import com.schullars.desktopctl.DesktopCtlOverlayController
import com.schullars.desktopctl.DesktopCtlScreenshotResult
import hbb.MessageOuterClass.ControlKey
import hbb.MessageOuterClass.KeyEvent
import hbb.MessageOuterClass.KeyboardMode
import java.io.File
import java.util.Locale

@SuppressLint("NewApi")
internal class DesktopCtlRustDeskHost(
    private val mainService: MainService,
    private val frameCapture: RustDeskFrameCapture
) : DesktopCtlHost {
    private val applicationCatalog = AndroidApplicationCatalog(mainService)
    private var overlayController: DesktopCtlOverlayController? = null
    private var pointerX = 0
    private var pointerY = 0

    override fun startSession(keepAlive: Boolean): DesktopCtlOperationResult {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            return failure("Android DesktopCTL input requires Android 7.0 or newer.")
        }
        if (!MainService.isStart) {
            return failure(
                "RustDesk screen capture is not active. Start screen sharing before DesktopCTL."
            )
        }
        val inputService = InputService.ctx
            ?: return failure(
                "RustDesk Input accessibility is not active. Enable it before DesktopCTL."
            )
        val controller = overlayController ?: DesktopCtlOverlayController(inputService)
        val result = controller.start()
        if (result.isSuccessful) {
            overlayController = controller
        }
        return result
    }

    override fun stopSession(): DesktopCtlOperationResult {
        frameCapture.cancel()
        val result = overlayController?.stop() ?: success("")
        overlayController = null
        return result
    }

    override fun isSessionReady(): Boolean =
        MainService.isStart && InputService.isOpen && overlayController?.isRunning == true

    override fun movePointer(x: Int, y: Int): DesktopCtlOperationResult {
        val inputService = inputService() ?: return inputUnavailable()
        pointerX = x.coerceIn(0, maximumCommandX())
        pointerY = y.coerceIn(0, maximumCommandY())
        inputService.onMouseInput(0, pointerX, pointerY)
        moveIndicator()
        return success("")
    }

    override fun movePointerRelative(deltaX: Int, deltaY: Int): DesktopCtlOperationResult =
        movePointer(pointerX + deltaX, pointerY + deltaY)

    override fun clickMouse(
        button: DesktopCtlHost.MouseButton,
        count: Int
    ): DesktopCtlOperationResult {
        val inputService = inputService() ?: return inputUnavailable()
        repeat(count) {
            inputService.onMouseInput(0, pointerX, pointerY)
            when (button) {
                DesktopCtlHost.MouseButton.LEFT -> {
                    inputService.onMouseInput(LEFT_DOWN, pointerX, pointerY)
                    inputService.onMouseInput(LEFT_UP, pointerX, pointerY)
                }
                DesktopCtlHost.MouseButton.RIGHT ->
                    inputService.onMouseInput(RIGHT_UP, pointerX, pointerY)
                DesktopCtlHost.MouseButton.MIDDLE -> {
                    inputService.onMouseInput(WHEEL_BUTTON_DOWN, pointerX, pointerY)
                    inputService.onMouseInput(WHEEL_BUTTON_UP, pointerX, pointerY)
                }
            }
        }
        moveIndicator()
        return success("")
    }

    override fun setMouseButton(
        button: DesktopCtlHost.MouseButton,
        action: DesktopCtlHost.ButtonAction
    ): DesktopCtlOperationResult {
        val inputService = inputService() ?: return inputUnavailable()
        val mask = when (button) {
            DesktopCtlHost.MouseButton.LEFT ->
                if (action == DesktopCtlHost.ButtonAction.DOWN) LEFT_DOWN else LEFT_UP
            DesktopCtlHost.MouseButton.RIGHT -> {
                if (action == DesktopCtlHost.ButtonAction.DOWN) {
                    return failure(
                        "Android right-button down is unavailable; use mouse click right."
                    )
                }
                RIGHT_UP
            }
            DesktopCtlHost.MouseButton.MIDDLE ->
                if (action == DesktopCtlHost.ButtonAction.DOWN) {
                    WHEEL_BUTTON_DOWN
                } else {
                    WHEEL_BUTTON_UP
                }
        }
        inputService.onMouseInput(mask, pointerX, pointerY)
        return success("")
    }

    override fun scroll(
        delta: Int,
        axis: DesktopCtlHost.ScrollAxis
    ): DesktopCtlOperationResult {
        if (axis == DesktopCtlHost.ScrollAxis.HORIZONTAL) {
            return failure("Android accessibility does not provide portable horizontal scrolling.")
        }
        val inputService = inputService() ?: return inputUnavailable()
        inputService.onMouseInput(MOUSE_TYPE_WHEEL, 0, delta)
        return success("")
    }

    override fun sendKey(
        key: String,
        action: DesktopCtlHost.KeyAction
    ): DesktopCtlOperationResult {
        val inputService = inputService() ?: return inputUnavailable()
        val event = createKeyEvent(key, action, emptyList())
            ?: return failure("Unsupported Android key: $key")
        inputService.onKeyEvent(event.toByteArray())
        return success("")
    }

    override fun sendHotkey(keys: List<String>): DesktopCtlOperationResult {
        if (keys.isEmpty()) {
            return failure("An Android hotkey requires at least one key.")
        }
        val inputService = inputService() ?: return inputUnavailable()
        val modifiers = keys.dropLast(1).mapNotNull(::modifierFor)
        if (modifiers.size != keys.size - 1) {
            return failure("Only modifier keys may precede the final Android hotkey key.")
        }
        val event = createKeyEvent(
            keys.last(),
            DesktopCtlHost.KeyAction.PRESS,
            modifiers
        ) ?: return failure("Unsupported Android hotkey key: ${keys.last()}")
        inputService.onKeyEvent(event.toByteArray())
        return success("")
    }

    override fun typeText(
        text: String,
        mode: DesktopCtlHost.TextInputMode
    ): DesktopCtlOperationResult {
        if (mode == DesktopCtlHost.TextInputMode.PHYSICAL) {
            return failure(
                "Android does not expose portable physical scan-code text injection."
            )
        }
        val inputService = inputService() ?: return inputUnavailable()
        val event = KeyEvent.newBuilder()
            .setMode(KeyboardMode.Legacy)
            .setSeq(text)
            .setPress(true)
            .build()
        inputService.onKeyEvent(event.toByteArray())
        return success("")
    }

    override fun captureScreen(
        outputFile: File,
        format: DesktopCtlHost.ScreenshotFormat,
        jpegQuality: Int
    ): DesktopCtlScreenshotResult {
        if (!MainService.isStart) {
            return screenshotFailure("RustDesk screen capture is not active.")
        }
        val bitmap = try {
            frameCapture.capture(FRAME_TIMEOUT_MILLISECONDS)
        } catch (exception: InterruptedException) {
            Thread.currentThread().interrupt()
            null
        } ?: return screenshotFailure("Timed out waiting for a current RustDesk screen frame.")
        return try {
            val operation = AndroidImageWriter.write(bitmap, outputFile, format, jpegQuality)
            DesktopCtlScreenshotResult(
                operation,
                bitmap.width,
                bitmap.height,
                SCREEN_INFO.scale.toFloat(),
                rotationDegrees()
            )
        } finally {
            bitmap.recycle()
        }
    }

    override fun listApplications(): DesktopCtlApplicationListResult =
        applicationCatalog.listApplications()

    override fun openApplication(idOrName: String): DesktopCtlOperationResult =
        applicationCatalog.openApplication(idOrName)

    private fun inputService(): InputService? = InputService.ctx

    private fun moveIndicator() {
        overlayController?.movePointer(
            pointerX * SCREEN_INFO.scale,
            pointerY * SCREEN_INFO.scale
        )
    }

    private fun maximumCommandX(): Int = (SCREEN_INFO.width - 1).coerceAtLeast(0)

    private fun maximumCommandY(): Int = (SCREEN_INFO.height - 1).coerceAtLeast(0)

    private fun createKeyEvent(
        key: String,
        action: DesktopCtlHost.KeyAction,
        modifiers: List<ControlKey>
    ): KeyEvent? {
        val builder = KeyEvent.newBuilder().setMode(KeyboardMode.Map)
        val controlKey = controlKeyFor(key)
        if (controlKey != null) {
            builder.setControlKey(controlKey)
        } else {
            val keyCode = androidKeyCodeFor(key)
            if (keyCode == android.view.KeyEvent.KEYCODE_UNKNOWN) {
                return null
            }
            builder.setChr(keyCode)
        }
        builder.addAllModifiers(modifiers)
        when (action) {
            DesktopCtlHost.KeyAction.PRESS -> builder.setPress(true)
            DesktopCtlHost.KeyAction.DOWN -> builder.setDown(true)
            DesktopCtlHost.KeyAction.UP -> builder.setDown(false)
        }
        return builder.build()
    }

    private fun androidKeyCodeFor(key: String): Int {
        val normalized = key.uppercase(Locale.ROOT)
            .replace('-', '_')
            .replace(' ', '_')
        val alias = KEY_ALIASES[normalized] ?: normalized
        return android.view.KeyEvent.keyCodeFromString("KEYCODE_$alias")
    }

    private fun controlKeyFor(key: String): ControlKey? = when (normalize(key)) {
        "ALT" -> ControlKey.Alt
        "BACKSPACE" -> ControlKey.Backspace
        "CAPSLOCK" -> ControlKey.CapsLock
        "CTRL", "CONTROL" -> ControlKey.Control
        "DELETE", "DEL" -> ControlKey.Delete
        "DOWN", "DOWNARROW" -> ControlKey.DownArrow
        "END" -> ControlKey.End
        "ESC", "ESCAPE" -> ControlKey.Escape
        "HOME" -> ControlKey.Home
        "INSERT", "INS" -> ControlKey.Insert
        "LEFT", "LEFTARROW" -> ControlKey.LeftArrow
        "META", "WIN", "WINDOWS" -> ControlKey.Meta
        "NUMLOCK" -> ControlKey.NumLock
        "PAGEDOWN", "PGDN" -> ControlKey.PageDown
        "PAGEUP", "PGUP" -> ControlKey.PageUp
        "RETURN", "ENTER" -> ControlKey.Return
        "RIGHT", "RIGHTARROW" -> ControlKey.RightArrow
        "SHIFT" -> ControlKey.Shift
        "SPACE" -> ControlKey.Space
        "TAB" -> ControlKey.Tab
        "UP", "UPARROW" -> ControlKey.UpArrow
        "VOLUMEDOWN" -> ControlKey.VolumeDown
        "VOLUMEMUTE" -> ControlKey.VolumeMute
        "VOLUMEUP" -> ControlKey.VolumeUp
        "POWER" -> ControlKey.Power
        "BACK" -> null
        else -> functionKeyFor(key)
    }

    private fun functionKeyFor(key: String): ControlKey? = when (normalize(key)) {
        "F1" -> ControlKey.F1
        "F2" -> ControlKey.F2
        "F3" -> ControlKey.F3
        "F4" -> ControlKey.F4
        "F5" -> ControlKey.F5
        "F6" -> ControlKey.F6
        "F7" -> ControlKey.F7
        "F8" -> ControlKey.F8
        "F9" -> ControlKey.F9
        "F10" -> ControlKey.F10
        "F11" -> ControlKey.F11
        "F12" -> ControlKey.F12
        else -> null
    }

    private fun modifierFor(key: String): ControlKey? = when (normalize(key)) {
        "ALT" -> ControlKey.Alt
        "CTRL", "CONTROL" -> ControlKey.Control
        "META", "WIN", "WINDOWS" -> ControlKey.Meta
        "SHIFT" -> ControlKey.Shift
        else -> null
    }

    private fun normalize(value: String): String = value
        .uppercase(Locale.ROOT)
        .replace("_", "")
        .replace("-", "")
        .replace(" ", "")

    @Suppress("DEPRECATION")
    private fun rotationDegrees(): Int = when (
        (mainService.getSystemService(Context.WINDOW_SERVICE) as WindowManager)
            .defaultDisplay.rotation
    ) {
        android.view.Surface.ROTATION_90 -> 90
        android.view.Surface.ROTATION_180 -> 180
        android.view.Surface.ROTATION_270 -> 270
        else -> 0
    }

    private fun screenshotFailure(message: String) = DesktopCtlScreenshotResult(
        failure(message),
        0,
        0,
        SCREEN_INFO.scale.toFloat(),
        rotationDegrees()
    )

    private fun inputUnavailable() = failure(
        "RustDesk Input accessibility is not active."
    )

    private fun success(message: String) = DesktopCtlOperationResult.success(message)

    private fun failure(message: String) = DesktopCtlOperationResult.failure(message)

    private companion object {
        const val FRAME_TIMEOUT_MILLISECONDS = 3_000L
        val KEY_ALIASES = mapOf(
            "ESC" to "ESCAPE",
            "DEL" to "FORWARD_DEL",
            "PGUP" to "PAGE_UP",
            "PGDN" to "PAGE_DOWN",
            "CTRL" to "CTRL_LEFT",
            "WIN" to "META_LEFT"
        )
    }
}
