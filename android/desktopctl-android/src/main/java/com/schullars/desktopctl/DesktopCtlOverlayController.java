package com.schullars.desktopctl;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;

import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;

/**
 * Owns the noninteractive green accessibility overlay used while DesktopCTL is active.
 */
public final class DesktopCtlOverlayController implements AutoCloseable {
    private static final long MAIN_THREAD_TIMEOUT_SECONDS = 5L;
    private final Context context;
    private final WindowManager windowManager;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private volatile OverlayView overlayView;

    /**
     * Creates an overlay controller for an active Android accessibility-service context.
     *
     * @param context active accessibility-service context used to add the overlay window.
     */
    public DesktopCtlOverlayController(Context context) {
        this.context = context;
        this.windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
    }

    /**
     * Adds a click-through translucent green border and pointer indicator.
     *
     * @return success only after the overlay window is attached.
     */
    public DesktopCtlOperationResult start() {
        return runOnMain(() -> {
            if (overlayView != null) {
                return DesktopCtlOperationResult.success("Green Android activity indicator is ready.");
            }
            OverlayView view = new OverlayView(context);
            WindowManager.LayoutParams parameters = new WindowManager.LayoutParams(
                    WindowManager.LayoutParams.MATCH_PARENT,
                    WindowManager.LayoutParams.MATCH_PARENT,
                    WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY,
                    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                            | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                            | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                            | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                            | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                    PixelFormat.TRANSLUCENT);
            parameters.gravity = Gravity.TOP | Gravity.START;
            try {
                windowManager.addView(view, parameters);
                overlayView = view;
                return DesktopCtlOperationResult.success(
                        "Green Android activity indicator is ready.");
            } catch (RuntimeException exception) {
                return DesktopCtlOperationResult.failure(
                        "Unable to show the Android activity indicator: " + exception.getMessage());
            }
        });
    }

    /**
     * Updates the visual pointer position in injected Android screen coordinates.
     *
     * @param x horizontal injected coordinate.
     * @param y vertical injected coordinate.
     */
    public void movePointer(int x, int y) {
        mainHandler.post(() -> {
            if (overlayView != null) {
                overlayView.movePointer(x, y);
            }
        });
    }

    /**
     * Reports whether the overlay window is attached.
     *
     * @return true while the indicator is visible.
     */
    public boolean isRunning() {
        return overlayView != null;
    }

    /**
     * Removes the indicator window.
     *
     * @return the removal result.
     */
    public DesktopCtlOperationResult stop() {
        return runOnMain(() -> {
            if (overlayView == null) {
                return DesktopCtlOperationResult.success("");
            }
            OverlayView view = overlayView;
            overlayView = null;
            try {
                windowManager.removeViewImmediate(view);
                return DesktopCtlOperationResult.success("");
            } catch (RuntimeException exception) {
                return DesktopCtlOperationResult.failure(
                        "Unable to remove the Android activity indicator: " + exception.getMessage());
            }
        });
    }

    @Override
    public void close() {
        stop();
    }

    private DesktopCtlOperationResult runOnMain(Callable<DesktopCtlOperationResult> operation) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            try {
                return operation.call();
            } catch (Exception exception) {
                return DesktopCtlOperationResult.failure(
                        "Android overlay operation failed: " + exception.getMessage());
            }
        }
        FutureTask<DesktopCtlOperationResult> task = new FutureTask<>(operation);
        if (!mainHandler.post(task)) {
            return DesktopCtlOperationResult.failure(
                    "The Android main thread rejected the overlay operation.");
        }
        try {
            return task.get(MAIN_THREAD_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (Exception exception) {
            task.cancel(false);
            return DesktopCtlOperationResult.failure(
                    "The Android overlay operation timed out or failed: " + exception.getMessage());
        }
    }

    private static final class OverlayView extends View {
        private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint borderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint pointerPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint pointerCenterPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final float borderWidth;
        private final float glowWidth;
        private final float pointerRadius;
        private float pointerX;
        private float pointerY;
        private boolean pointerVisible;

        OverlayView(Context context) {
            super(context);
            float density = context.getResources().getDisplayMetrics().density;
            borderWidth = Math.max(2.0f, 2.0f * density);
            glowWidth = Math.max(6.0f, 7.0f * density);
            pointerRadius = Math.max(11.0f, 13.0f * density);

            glowPaint.setStyle(Paint.Style.STROKE);
            glowPaint.setStrokeWidth(glowWidth);
            glowPaint.setColor(Color.argb(28, 0, 255, 105));
            borderPaint.setStyle(Paint.Style.STROKE);
            borderPaint.setStrokeWidth(borderWidth);
            borderPaint.setColor(Color.argb(90, 0, 255, 105));
            pointerPaint.setStyle(Paint.Style.STROKE);
            pointerPaint.setStrokeWidth(borderWidth);
            pointerPaint.setColor(Color.argb(180, 0, 255, 105));
            pointerCenterPaint.setStyle(Paint.Style.FILL);
            pointerCenterPaint.setColor(Color.argb(210, 0, 255, 105));
        }

        void movePointer(int x, int y) {
            pointerX = x;
            pointerY = y;
            pointerVisible = true;
            invalidate();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            float glowInset = glowWidth / 2.0f;
            float borderInset = borderWidth / 2.0f;
            canvas.drawRect(
                    glowInset,
                    glowInset,
                    getWidth() - glowInset,
                    getHeight() - glowInset,
                    glowPaint);
            canvas.drawRect(
                    borderInset,
                    borderInset,
                    getWidth() - borderInset,
                    getHeight() - borderInset,
                    borderPaint);
            if (pointerVisible) {
                float boundedX = Math.max(0.0f, Math.min(getWidth(), pointerX));
                float boundedY = Math.max(0.0f, Math.min(getHeight(), pointerY));
                canvas.drawCircle(boundedX, boundedY, pointerRadius, pointerPaint);
                canvas.drawCircle(boundedX, boundedY, borderWidth, pointerCenterPaint);
            }
        }
    }
}
