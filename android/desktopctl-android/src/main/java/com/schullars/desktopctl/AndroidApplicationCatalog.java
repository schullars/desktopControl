package com.schullars.desktopctl;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Discovers and launches Android applications that expose launcher activities.
 */
public final class AndroidApplicationCatalog {
    private final Context applicationContext;
    private final PackageManager packageManager;

    /**
     * Creates a catalog bound to the host application.
     *
     * @param context any context from the host application.
     */
    public AndroidApplicationCatalog(Context context) {
        applicationContext = context.getApplicationContext();
        packageManager = applicationContext.getPackageManager();
    }

    /**
     * Lists applications with launcher activities visible to the host package.
     *
     * @return applications sorted by display label and package name.
     */
    public DesktopCtlApplicationListResult listApplications() {
        try {
            Intent query = new Intent(Intent.ACTION_MAIN);
            query.addCategory(Intent.CATEGORY_LAUNCHER);
            List<ResolveInfo> resolved;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                resolved = packageManager.queryIntentActivities(
                        query,
                        PackageManager.ResolveInfoFlags.of(0L));
            } else {
                resolved = packageManager.queryIntentActivities(query, 0);
            }

            Map<String, DesktopCtlApplication> uniqueApplications = new LinkedHashMap<>();
            for (ResolveInfo resolveInfo : resolved) {
                if (resolveInfo.activityInfo == null || resolveInfo.activityInfo.packageName == null) {
                    continue;
                }
                String packageName = resolveInfo.activityInfo.packageName;
                CharSequence label = resolveInfo.loadLabel(packageManager);
                String displayName = label == null || label.length() == 0
                        ? packageName
                        : label.toString();
                uniqueApplications.put(
                        packageName,
                        new DesktopCtlApplication(packageName, displayName));
            }

            List<DesktopCtlApplication> applications = new ArrayList<>(
                    uniqueApplications.values());
            Collections.sort(applications, new Comparator<DesktopCtlApplication>() {
                @Override
                public int compare(
                        DesktopCtlApplication left,
                        DesktopCtlApplication right) {
                    int nameOrder = left.getName().toLowerCase(Locale.ROOT).compareTo(
                            right.getName().toLowerCase(Locale.ROOT));
                    return nameOrder != 0 ? nameOrder : left.getId().compareTo(right.getId());
                }
            });
            return new DesktopCtlApplicationListResult(
                    DesktopCtlOperationResult.success(""),
                    applications);
        } catch (RuntimeException exception) {
            return new DesktopCtlApplicationListResult(
                    DesktopCtlOperationResult.failure(
                            "Unable to enumerate Android applications: " + exception.getMessage()),
                    Collections.<DesktopCtlApplication>emptyList());
        }
    }

    /**
     * Launches an application by package name or unique exact display label.
     *
     * @param idOrName package name or exact display label returned by listApplications.
     * @return success after Android accepts the launcher intent.
     */
    public DesktopCtlOperationResult openApplication(String idOrName) {
        DesktopCtlApplicationListResult listing = listApplications();
        if (!listing.getOperation().isSuccessful()) {
            return listing.getOperation();
        }

        DesktopCtlApplication selected = null;
        for (DesktopCtlApplication application : listing.getApplications()) {
            if (application.getId().equals(idOrName)) {
                selected = application;
                break;
            }
        }
        if (selected == null) {
            for (DesktopCtlApplication application : listing.getApplications()) {
                if (!application.getName().equalsIgnoreCase(idOrName)) {
                    continue;
                }
                if (selected != null) {
                    return DesktopCtlOperationResult.failure(
                            "The Android application name is ambiguous; use its package name.");
                }
                selected = application;
            }
        }
        if (selected == null) {
            return DesktopCtlOperationResult.failure(
                    "No launchable Android application matches: " + idOrName);
        }

        Intent launchIntent = packageManager.getLaunchIntentForPackage(selected.getId());
        if (launchIntent == null) {
            return DesktopCtlOperationResult.failure(
                    "The Android application has no launchable activity: " + selected.getId());
        }
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            applicationContext.startActivity(launchIntent);
            return DesktopCtlOperationResult.success(
                    "Application launch requested: " + selected.getId());
        } catch (RuntimeException exception) {
            return DesktopCtlOperationResult.failure(
                    "Android rejected the application launch: " + exception.getMessage());
        }
    }
}
