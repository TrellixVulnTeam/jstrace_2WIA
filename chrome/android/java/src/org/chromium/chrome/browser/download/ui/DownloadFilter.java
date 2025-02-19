// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.UrlConstants;

/**
 * A class holding constants and convenience methods about filters and their corresponding
 * resources.
 */
class DownloadFilter {
    static final int FILTER_ALL = 0;
    static final int FILTER_PAGE = 1;
    static final int FILTER_VIDEO = 2;
    static final int FILTER_AUDIO = 3;
    static final int FILTER_IMAGE = 4;
    static final int FILTER_DOCUMENT = 5;
    static final int FILTER_OTHER = 6;

    /**
     * Icons and labels for each filter in the menu.
     *
     * Changing the ordering of these items requires changing the FILTER_* values in
     * {@link DownloadHistoryAdapter}.
     */
    static final int[][] FILTER_LIST = new int[][] {
        {R.drawable.ic_get_app_white_24dp, R.string.download_manager_ui_all_downloads},
        {R.drawable.ic_drive_site_white_24dp, R.string.download_manager_ui_pages},
        {R.drawable.ic_play_arrow_white_24dp, R.string.download_manager_ui_video},
        {R.drawable.ic_music_note_white_24dp, R.string.download_manager_ui_audio},
        {R.drawable.ic_image_white_24dp, R.string.download_manager_ui_images},
        {R.drawable.ic_drive_text_white_24dp, R.string.download_manager_ui_documents},
        {R.drawable.ic_drive_file_white_24dp, R.string.download_manager_ui_other}
    };

    private static final String TAG = "download_ui";

    /**
     * @return The number of filters that exist.
     */
    static int getFilterCount() {
        return FILTER_LIST.length;
    }

    /**
     * @return The drawable id representing the given filter.
     */
    static int getDrawableForFilter(int filter) {
        return FILTER_LIST[filter][0];
    }

    /**
     * @return The resource id of the title representing the given filter.
     */
    static int getStringIdForFilter(int filter) {
        return FILTER_LIST[filter][1];
    }

    /**
     * @return The URL representing the filter.
     */
    static String getUrlForFilter(int filter) {
        if (filter == FILTER_ALL) {
            return UrlConstants.DOWNLOADS_URL;
        }
        return UrlConstants.DOWNLOADS_FILTER_URL + filter;
    }

    /**
     * @return The filter that the given URL represents.
     */
    static int getFilterFromUrl(String url) {
        if (TextUtils.isEmpty(url) || UrlConstants.DOWNLOADS_HOST.equals(url)) return FILTER_ALL;
        int result = FILTER_ALL;
        if (url.startsWith(UrlConstants.DOWNLOADS_FILTER_URL)) {
            try {
                result = Integer
                        .parseInt(url.substring(UrlConstants.DOWNLOADS_FILTER_URL.length()));
            } catch (NumberFormatException e) {
                Log.e(TAG, "Url parsing failed.");
            }
        }
        return result;
    }
}
