package com.schullars.desktopctl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Contains the result and immutable entries from application discovery.
 */
public final class DesktopCtlApplicationListResult {
    private final DesktopCtlOperationResult operation;
    private final List<DesktopCtlApplication> applications;

    /**
     * Creates an application-list result.
     *
     * @param operation discovery status.
     * @param applications launchable applications; ignored on failure.
     */
    public DesktopCtlApplicationListResult(
            DesktopCtlOperationResult operation,
            List<DesktopCtlApplication> applications) {
        this.operation = operation;
        this.applications = Collections.unmodifiableList(new ArrayList<>(applications));
    }

    /**
     * Returns the discovery status.
     *
     * @return a non-null operation result.
     */
    public DesktopCtlOperationResult getOperation() {
        return operation;
    }

    /**
     * Returns immutable launchable application entries.
     *
     * @return an immutable list, which may be empty.
     */
    public List<DesktopCtlApplication> getApplications() {
        return applications;
    }
}
