// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.StrictMode;
import android.text.TextUtils;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import org.chromium.base.ContentUriUtils;

import java.io.File;
import java.io.IOException;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Utility functions for common Android UI tasks.
 * This class is not supposed to be instantiated.
 */
public class UiUtils {
    private static final String TAG = "UiUtils";

    private static final int KEYBOARD_RETRY_ATTEMPTS = 10;
    private static final long KEYBOARD_RETRY_DELAY_MS = 100;

    public static final String EXTERNAL_IMAGE_FILE_PATH = "browser-images";
    // Keep this variable in sync with the value defined in file_paths.xml.
    public static final String IMAGE_FILE_PATH = "images";

    /**
     * Guards this class from being instantiated.
     */
    private UiUtils() {
    }

    /** The minimum size of the bottom margin below the app to detect a keyboard. */
    private static final float KEYBOARD_DETECT_BOTTOM_THRESHOLD_DP = 100;

    /** A delegate that allows disabling keyboard visibility detection. */
    private static KeyboardShowingDelegate sKeyboardShowingDelegate;

    /**
     * A delegate that can be implemented to override whether or not keyboard detection will be
     * used.
     */
    public interface KeyboardShowingDelegate {
        /**
         * Will be called to determine whether or not to detect if the keyboard is visible.
         * @param context A {@link Context} instance.
         * @param view    A {@link View}.
         * @return        Whether or not the keyboard check should be disabled.
         */
        boolean disableKeyboardCheck(Context context, View view);
    }

    /**
     * Allows setting a delegate to override the default software keyboard visibility detection.
     * @param delegate A {@link KeyboardShowingDelegate} instance.
     */
    public static void setKeyboardShowingDelegate(KeyboardShowingDelegate delegate) {
        sKeyboardShowingDelegate = delegate;
    }

    /**
     * Shows the software keyboard if necessary.
     * @param view The currently focused {@link View}, which would receive soft keyboard input.
     */
    public static void showKeyboard(final View view) {
        final Handler handler = new Handler();
        final AtomicInteger attempt = new AtomicInteger();
        Runnable openRunnable = new Runnable() {
            @Override
            public void run() {
                // Not passing InputMethodManager.SHOW_IMPLICIT as it does not trigger the
                // keyboard in landscape mode.
                InputMethodManager imm =
                        (InputMethodManager) view.getContext().getSystemService(
                                Context.INPUT_METHOD_SERVICE);
                // Third-party touches disk on showSoftInput call. http://crbug.com/619824,
                // http://crbug.com/635118
                StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
                StrictMode.allowThreadDiskWrites();
                try {
                    imm.showSoftInput(view, 0);
                } catch (IllegalArgumentException e) {
                    if (attempt.incrementAndGet() <= KEYBOARD_RETRY_ATTEMPTS) {
                        handler.postDelayed(this, KEYBOARD_RETRY_DELAY_MS);
                    } else {
                        Log.e(TAG, "Unable to open keyboard.  Giving up.", e);
                    }
                } finally {
                    StrictMode.setThreadPolicy(oldPolicy);
                }
            }
        };
        openRunnable.run();
    }

    /**
     * Hides the keyboard.
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    public static boolean hideKeyboard(View view) {
        InputMethodManager imm =
                (InputMethodManager) view.getContext().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    /**
     * Detects whether or not the keyboard is showing.  This is a best guess as there is no
     * standardized/foolproof way to do this.
     * @param context A {@link Context} instance.
     * @param view    A {@link View}.
     * @return        Whether or not the software keyboard is visible and taking up screen space.
     */
    public static boolean isKeyboardShowing(Context context, View view) {
        if (sKeyboardShowingDelegate != null
                && sKeyboardShowingDelegate.disableKeyboardCheck(context, view)) {
            return false;
        }

        View rootView = view.getRootView();
        if (rootView == null) return false;
        Rect appRect = new Rect();
        rootView.getWindowVisibleDisplayFrame(appRect);

        final float density = context.getResources().getDisplayMetrics().density;
        final float bottomMarginDp = Math.abs(rootView.getHeight() - appRect.height()) / density;
        return bottomMarginDp > KEYBOARD_DETECT_BOTTOM_THRESHOLD_DP;
    }

    /**
     * Gets the set of locales supported by the current enabled Input Methods.
     * @param context A {@link Context} instance.
     * @return A possibly-empty {@link Set} of locale strings.
     */
    public static Set<String> getIMELocales(Context context) {
        LinkedHashSet<String> locales = new LinkedHashSet<String>();
        InputMethodManager imManager =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        List<InputMethodInfo> enabledMethods = imManager.getEnabledInputMethodList();
        for (int i = 0; i < enabledMethods.size(); i++) {
            List<InputMethodSubtype> subtypes =
                    imManager.getEnabledInputMethodSubtypeList(enabledMethods.get(i), true);
            if (subtypes == null) continue;
            for (int j = 0; j < subtypes.size(); j++) {
                String locale = subtypes.get(j).getLocale();
                if (!TextUtils.isEmpty(locale)) locales.add(locale);
            }
        }
        return locales;
    }

    /**
     * Inserts a {@link View} into a {@link ViewGroup} after directly before a given {@View}.
     * @param container The {@link View} to add newView to.
     * @param newView The new {@link View} to add.
     * @param existingView The {@link View} to insert the newView before.
     * @return The index where newView was inserted, or -1 if it was not inserted.
     */
    public static int insertBefore(ViewGroup container, View newView, View existingView) {
        return insertView(container, newView, existingView, false);
    }

    /**
     * Inserts a {@link View} into a {@link ViewGroup} after directly after a given {@View}.
     * @param container The {@link View} to add newView to.
     * @param newView The new {@link View} to add.
     * @param existingView The {@link View} to insert the newView after.
     * @return The index where newView was inserted, or -1 if it was not inserted.
     */
    public static int insertAfter(ViewGroup container, View newView, View existingView) {
        return insertView(container, newView, existingView, true);
    }

    private static int insertView(
            ViewGroup container, View newView, View existingView, boolean after) {
        // See if the view has already been added.
        int index = container.indexOfChild(newView);
        if (index >= 0) return index;

        // Find the location of the existing view.
        index = container.indexOfChild(existingView);
        if (index < 0) return -1;

        // Add the view.
        if (after) index++;
        container.addView(newView, index);
        return index;
    }

    /**
     * Generates a scaled screenshot of the given view.  The maximum size of the screenshot is
     * determined by maximumDimension.
     *
     * @param currentView      The view to generate a screenshot of.
     * @param maximumDimension The maximum width or height of the generated screenshot.  The bitmap
     *                         will be scaled to ensure the maximum width or height is equal to or
     *                         less than this.  Any value <= 0, will result in no scaling.
     * @param bitmapConfig     Bitmap config for the generated screenshot (ARGB_8888 or RGB_565).
     * @return The screen bitmap of the view or null if a problem was encountered.
     */
    public static Bitmap generateScaledScreenshot(
            View currentView, int maximumDimension, Bitmap.Config bitmapConfig) {
        Bitmap screenshot = null;
        boolean drawingCacheEnabled = currentView.isDrawingCacheEnabled();
        try {
            prepareViewHierarchyForScreenshot(currentView, true);
            if (!drawingCacheEnabled) currentView.setDrawingCacheEnabled(true);
            // Android has a maximum drawing cache size and if the drawing cache is bigger
            // than that, getDrawingCache() returns null.
            Bitmap originalBitmap = currentView.getDrawingCache();
            if (originalBitmap != null) {
                double originalHeight = originalBitmap.getHeight();
                double originalWidth = originalBitmap.getWidth();
                int newWidth = (int) originalWidth;
                int newHeight = (int) originalHeight;
                if (maximumDimension > 0) {
                    double scale = maximumDimension / Math.max(originalWidth, originalHeight);
                    newWidth = (int) Math.round(originalWidth * scale);
                    newHeight = (int) Math.round(originalHeight * scale);
                }
                Bitmap scaledScreenshot =
                        Bitmap.createScaledBitmap(originalBitmap, newWidth, newHeight, true);
                if (scaledScreenshot.getConfig() != bitmapConfig) {
                    screenshot = scaledScreenshot.copy(bitmapConfig, false);
                    scaledScreenshot.recycle();
                    scaledScreenshot = null;
                } else {
                    screenshot = scaledScreenshot;
                }
            } else if (currentView.getMeasuredHeight() > 0 && currentView.getMeasuredWidth() > 0) {
                double originalHeight = currentView.getMeasuredHeight();
                double originalWidth = currentView.getMeasuredWidth();
                int newWidth = (int) originalWidth;
                int newHeight = (int) originalHeight;
                if (maximumDimension > 0) {
                    double scale = maximumDimension / Math.max(originalWidth, originalHeight);
                    newWidth = (int) Math.round(originalWidth * scale);
                    newHeight = (int) Math.round(originalHeight * scale);
                }
                Bitmap bitmap = Bitmap.createBitmap(newWidth, newHeight, bitmapConfig);
                Canvas canvas = new Canvas(bitmap);
                canvas.scale((float) (newWidth / originalWidth),
                        (float) (newHeight / originalHeight));
                currentView.draw(canvas);
                screenshot = bitmap;
            }
        } catch (OutOfMemoryError e) {
            Log.d(TAG, "Unable to capture screenshot and scale it down." + e.getMessage());
        } finally {
            if (!drawingCacheEnabled) currentView.setDrawingCacheEnabled(false);
            prepareViewHierarchyForScreenshot(currentView, false);
        }
        return screenshot;
    }

    private static void prepareViewHierarchyForScreenshot(View view, boolean takingScreenshot) {
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                prepareViewHierarchyForScreenshot(viewGroup.getChildAt(i), takingScreenshot);
            }
        } else if (view instanceof SurfaceView) {
            view.setWillNotDraw(!takingScreenshot);
        }
    }

    /**
     * Get a directory for the image capture operation. For devices with JB MR2
     * or latter android versions, the directory is IMAGE_FILE_PATH directory.
     * For ICS devices, the directory is CAPTURE_IMAGE_DIRECTORY.
     *
     * @param context The application context.
     * @return directory for the captured image to be stored.
     */
    public static File getDirectoryForImageCapture(Context context) throws IOException {
        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/562173
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            File path;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                path = new File(context.getFilesDir(), IMAGE_FILE_PATH);
                if (!path.exists() && !path.mkdir()) {
                    throw new IOException("Folder cannot be created.");
                }
            } else {
                File externalDataDir =
                        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM);
                path = new File(externalDataDir.getAbsolutePath()
                                + File.separator
                                + EXTERNAL_IMAGE_FILE_PATH);
                if (!path.exists() && !path.mkdirs()) {
                    path = externalDataDir;
                }
            }
            return path;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Get a URI for |file| which has the image capture. This function assumes that path of |file|
     * is based on the result of UiUtils.getDirectoryForImageCapture().
     *
     * @param context The application context.
     * @param file image capture file.
     * @return URI for |file|.
     */
    public static Uri getUriForImageCaptureFile(Context context, File file) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2
                ? ContentUriUtils.getContentUriFromFile(context, file)
                : Uri.fromFile(file);
    }
}
