package com.carriez.flutter_hbb.desktopctl

import androidx.annotation.Keep
import com.carriez.flutter_hbb.MainService
import com.schullars.desktopctl.DesktopCtlEngine
import org.json.JSONObject

class DesktopCtlRustDeskBridge(
    private val mainService: MainService,
    version: String
) : AutoCloseable {
    private val frameCapture = RustDeskFrameCapture()
    private val engine = DesktopCtlEngine(
        DesktopCtlRustDeskHost(mainService, frameCapture),
        mainService.cacheDir,
        version
    )

    @Keep
    @Synchronized
    fun execute(commandLine: String): String {
        val result = engine.execute(commandLine)
        return JSONObject()
            .put("exitCode", result.exitCode)
            .put("stdout", result.standardOutput)
            .put("stderr", result.standardError)
            .toString()
    }

    fun onImage(image: android.media.Image) {
        frameCapture.onImage(
            image,
            com.carriez.flutter_hbb.SCREEN_INFO.width,
            com.carriez.flutter_hbb.SCREEN_INFO.height
        )
    }

    @Synchronized
    override fun close() {
        frameCapture.cancel()
        engine.close()
    }
}
