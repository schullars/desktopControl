package com.schullars.desktopctl;

import java.util.Objects;

/**
 * Identifies one launchable Android application.
 */
public final class DesktopCtlApplication {
    private final String id;
    private final String name;

    /**
     * Creates an immutable application description.
     *
     * @param id stable Android package name.
     * @param name display label shown to the user.
     */
    public DesktopCtlApplication(String id, String name) {
        this.id = Objects.requireNonNull(id, "id");
        this.name = Objects.requireNonNull(name, "name");
    }

    /**
     * Returns the stable Android package name.
     *
     * @return a non-empty package name.
     */
    public String getId() {
        return id;
    }

    /**
     * Returns the application display label.
     *
     * @return a non-null label.
     */
    public String getName() {
        return name;
    }
}
