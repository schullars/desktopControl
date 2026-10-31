package com.schullars.desktopctl;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

final class CommandDispatcher {
    private final DesktopCtlHost host;
    private final SessionManager sessionManager;
    private final ScreenshotStore screenshotStore;
    private final String version;

    CommandDispatcher(
            DesktopCtlHost host,
            SessionManager sessionManager,
            ScreenshotStore screenshotStore,
            String version) {
        this.host = host;
        this.sessionManager = sessionManager;
        this.screenshotStore = screenshotStore;
        this.version = version;
    }

    DesktopCtlCommandResult execute(List<String> arguments) {
        if (arguments.isEmpty() || !"desktopctl".equals(lower(arguments.get(0)))) {
            return DesktopCtlCommandResult.invalid(
                    "Android virtual commands must start with desktopctl.");
        }
        if (arguments.size() < 2) {
            return DesktopCtlCommandResult.invalid(
                    "A command is required. Run desktopctl -H for help.");
        }

        String command = lower(arguments.get(1));
        if (isHelp(command)) {
            return DesktopCtlCommandResult.success(
                    HelpText.create(version, screenshotStore.getScreenshotDirectory().getPath()));
        }
        if ("session".equals(command)) {
            return runSession(arguments);
        }
        if (!sessionManager.recordActivity()) {
            return DesktopCtlCommandResult.failure(
                    "Session is not started. Run \"desktopctl session start\" before using other commands.");
        }

        switch (command) {
            case "--version":
            case "version":
                return DesktopCtlCommandResult.success("desktopctl " + version);
            case "mouse":
                return runMouse(arguments);
            case "keyboard":
                return runKeyboard(arguments);
            case "screenshot":
                return runScreenshot(arguments);
            case "app":
                return runApplication(arguments);
            case "window":
                return DesktopCtlCommandResult.failure(
                        "Android does not support global window listing, activation, or window capture.");
            default:
                return DesktopCtlCommandResult.invalid(
                        "Unknown command: " + arguments.get(1));
        }
    }

    private DesktopCtlCommandResult runSession(List<String> arguments) {
        if (arguments.size() > 4) {
            return DesktopCtlCommandResult.invalid(
                    "Session accepts start or stop and the optional --keep-alive flag.");
        }
        String action = arguments.size() >= 3 ? lower(arguments.get(2)) : "start";
        boolean keepAlive = arguments.size() == 4
                && "--keep-alive".equals(arguments.get(3));
        if (arguments.size() == 4 && !keepAlive) {
            return DesktopCtlCommandResult.invalid(
                    "The only supported session option is --keep-alive.");
        }
        if ("start".equals(action)) {
            return fromOperation(sessionManager.start(keepAlive));
        }
        if ("stop".equals(action)) {
            if (keepAlive) {
                return DesktopCtlCommandResult.invalid(
                        "Session stop does not accept --keep-alive.");
            }
            return fromOperation(sessionManager.stop());
        }
        return DesktopCtlCommandResult.invalid("Session action must be start or stop.");
    }

    private DesktopCtlCommandResult runMouse(List<String> arguments) {
        if (arguments.size() < 3) {
            return DesktopCtlCommandResult.invalid("Missing mouse action.");
        }
        String action = lower(arguments.get(2));
        if ("move".equals(action) || "move-relative".equals(action)) {
            if (arguments.size() != 5) {
                return DesktopCtlCommandResult.invalid(
                        "Mouse move requires two integer coordinates.");
            }
            Integer x = parseInteger(arguments.get(3));
            Integer y = parseInteger(arguments.get(4));
            if (x == null || y == null) {
                return DesktopCtlCommandResult.invalid(
                        "Mouse coordinates must be valid 32-bit integers.");
            }
            DesktopCtlOperationResult operation = "move".equals(action)
                    ? host.movePointer(x, y)
                    : host.movePointerRelative(x, y);
            return fromOperation(operation);
        }
        if ("click".equals(action)) {
            if (arguments.size() != 4 && arguments.size() != 5) {
                return DesktopCtlCommandResult.invalid(
                        "Mouse click requires BUTTON and optional COUNT.");
            }
            DesktopCtlHost.MouseButton button = parseMouseButton(arguments.get(3));
            if (button == null) {
                return invalidMouseButton();
            }
            int count = 1;
            if (arguments.size() == 5) {
                Integer parsedCount = parseInteger(arguments.get(4));
                if (parsedCount == null || parsedCount < 1 || parsedCount > 100) {
                    return DesktopCtlCommandResult.invalid(
                            "COUNT must be between 1 and 100.");
                }
                count = parsedCount;
            }
            return fromOperation(host.clickMouse(button, count));
        }
        if ("down".equals(action) || "up".equals(action)) {
            if (arguments.size() != 4) {
                return DesktopCtlCommandResult.invalid(
                        "Mouse down/up requires exactly one BUTTON.");
            }
            DesktopCtlHost.MouseButton button = parseMouseButton(arguments.get(3));
            if (button == null) {
                return invalidMouseButton();
            }
            DesktopCtlHost.ButtonAction buttonAction = "down".equals(action)
                    ? DesktopCtlHost.ButtonAction.DOWN
                    : DesktopCtlHost.ButtonAction.UP;
            return fromOperation(host.setMouseButton(button, buttonAction));
        }
        if ("wheel".equals(action)) {
            if (arguments.size() != 4 && arguments.size() != 5) {
                return DesktopCtlCommandResult.invalid(
                        "Mouse wheel requires DELTA and optional direction.");
            }
            Integer delta = parseInteger(arguments.get(3));
            if (delta == null) {
                return DesktopCtlCommandResult.invalid(
                        "DELTA must be a valid 32-bit integer.");
            }
            DesktopCtlHost.ScrollAxis axis = DesktopCtlHost.ScrollAxis.VERTICAL;
            if (arguments.size() == 5) {
                String axisName = lower(arguments.get(4));
                if ("horizontal".equals(axisName)) {
                    axis = DesktopCtlHost.ScrollAxis.HORIZONTAL;
                } else if (!"vertical".equals(axisName)) {
                    return DesktopCtlCommandResult.invalid(
                            "Wheel direction must be vertical or horizontal.");
                }
            }
            return fromOperation(host.scroll(delta, axis));
        }
        return DesktopCtlCommandResult.invalid("Unknown mouse action: " + arguments.get(2));
    }

    private DesktopCtlCommandResult runKeyboard(List<String> arguments) {
        if (arguments.size() < 3) {
            return DesktopCtlCommandResult.invalid("Missing keyboard action.");
        }
        String action = lower(arguments.get(2));
        if ("press".equals(action) || "down".equals(action) || "up".equals(action)) {
            if (arguments.size() != 4) {
                return DesktopCtlCommandResult.invalid(
                        "Keyboard press/down/up requires exactly one KEY.");
            }
            DesktopCtlHost.KeyAction keyAction = DesktopCtlHost.KeyAction.PRESS;
            if ("down".equals(action)) {
                keyAction = DesktopCtlHost.KeyAction.DOWN;
            } else if ("up".equals(action)) {
                keyAction = DesktopCtlHost.KeyAction.UP;
            }
            return fromOperation(host.sendKey(arguments.get(3), keyAction));
        }
        if ("hotkey".equals(action)) {
            if (arguments.size() < 4) {
                return DesktopCtlCommandResult.invalid(
                        "Keyboard hotkey requires at least one KEY.");
            }
            List<String> keys = new ArrayList<>();
            for (int index = 3; index < arguments.size(); ++index) {
                String[] members = arguments.get(index).split("\\+", -1);
                for (String member : members) {
                    if (member.isEmpty()) {
                        return DesktopCtlCommandResult.invalid(
                                "Hotkey expressions cannot contain an empty key.");
                    }
                    keys.add(member);
                }
            }
            return fromOperation(host.sendHotkey(keys));
        }
        if ("type".equals(action)
                || "type-physical".equals(action)
                || "type-unicode".equals(action)) {
            if (arguments.size() < 4) {
                return DesktopCtlCommandResult.invalid(
                        "Keyboard type/type-physical/type-unicode requires TEXT.");
            }
            String text = join(arguments, 3);
            DesktopCtlHost.TextInputMode mode = DesktopCtlHost.TextInputMode.HYBRID;
            if ("type-physical".equals(action)) {
                mode = DesktopCtlHost.TextInputMode.PHYSICAL;
            } else if ("type-unicode".equals(action)) {
                mode = DesktopCtlHost.TextInputMode.UNICODE;
            }
            return fromOperation(host.typeText(text, mode));
        }
        return DesktopCtlCommandResult.invalid(
                "Unknown keyboard action: " + arguments.get(2));
    }

    private DesktopCtlCommandResult runScreenshot(List<String> arguments) {
        if (arguments.size() == 2) {
            return screenshotStore.capture(null);
        }
        String action = lower(arguments.get(2));
        if ("full".equals(action)) {
            if (arguments.size() != 3 && arguments.size() != 4) {
                return DesktopCtlCommandResult.invalid(
                        "Screenshot full accepts only an optional OUTPUT_IMAGE.");
            }
            return screenshotStore.capture(arguments.size() == 4 ? arguments.get(3) : null);
        }
        if ("region".equals(action) || "window".equals(action)) {
            return DesktopCtlCommandResult.failure(
                    "Android supports full-screen RustDesk MediaProjection capture only.");
        }
        if (arguments.size() == 3) {
            return screenshotStore.capture(arguments.get(2));
        }
        return DesktopCtlCommandResult.invalid(
                "Invalid screenshot command. Run desktopctl -H for examples.");
    }

    private DesktopCtlCommandResult runApplication(List<String> arguments) {
        if (arguments.size() < 3) {
            return DesktopCtlCommandResult.invalid("Missing app action.");
        }
        String action = lower(arguments.get(2));
        if ("list".equals(action)) {
            if (arguments.size() != 3
                    && (arguments.size() != 4 || !"--json".equals(arguments.get(3)))) {
                return DesktopCtlCommandResult.invalid(
                        "App list accepts only the optional --json flag.");
            }
            DesktopCtlApplicationListResult result = host.listApplications();
            if (result == null || result.getOperation() == null) {
                return DesktopCtlCommandResult.failure(
                        "The Android host returned no application-list result.");
            }
            if (!result.getOperation().isSuccessful()) {
                return DesktopCtlCommandResult.failure(result.getOperation().getMessage());
            }
            StringBuilder json = new StringBuilder("{\"applications\":[");
            for (int index = 0; index < result.getApplications().size(); ++index) {
                DesktopCtlApplication application = result.getApplications().get(index);
                if (index > 0) {
                    json.append(',');
                }
                json.append("{\"id\":")
                        .append(JsonCodec.quote(application.getId()))
                        .append(",\"name\":")
                        .append(JsonCodec.quote(application.getName()))
                        .append('}');
            }
            json.append("]}");
            return DesktopCtlCommandResult.success(json.toString());
        }
        if ("open".equals(action)) {
            if (arguments.size() < 4) {
                return DesktopCtlCommandResult.invalid(
                        "App open requires an application package or exact name.");
            }
            return fromOperation(host.openApplication(join(arguments, 3)));
        }
        return DesktopCtlCommandResult.invalid("Unknown app action: " + arguments.get(2));
    }

    private static DesktopCtlCommandResult fromOperation(DesktopCtlOperationResult operation) {
        if (operation == null) {
            return DesktopCtlCommandResult.failure(
                    "The Android host returned no operation result.");
        }
        if (!operation.isSuccessful()) {
            return DesktopCtlCommandResult.failure(operation.getMessage());
        }
        return DesktopCtlCommandResult.success(operation.getMessage());
    }

    private static DesktopCtlCommandResult invalidMouseButton() {
        return DesktopCtlCommandResult.invalid("BUTTON must be left, right, or middle.");
    }

    private static DesktopCtlHost.MouseButton parseMouseButton(String value) {
        switch (lower(value)) {
            case "left":
                return DesktopCtlHost.MouseButton.LEFT;
            case "right":
                return DesktopCtlHost.MouseButton.RIGHT;
            case "middle":
                return DesktopCtlHost.MouseButton.MIDDLE;
            default:
                return null;
        }
    }

    private static Integer parseInteger(String value) {
        try {
            return Integer.valueOf(value);
        } catch (NumberFormatException exception) {
            return null;
        }
    }

    private static String join(List<String> values, int startIndex) {
        StringBuilder result = new StringBuilder();
        for (int index = startIndex; index < values.size(); ++index) {
            if (index > startIndex) {
                result.append(' ');
            }
            result.append(values.get(index));
        }
        return result.toString();
    }

    private static boolean isHelp(String command) {
        return "-h".equals(command)
                || "--help".equals(command)
                || "help".equals(command);
    }

    private static String lower(String value) {
        return value.toLowerCase(Locale.ROOT);
    }
}
