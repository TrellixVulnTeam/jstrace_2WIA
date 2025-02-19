// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.util.SparseArray;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Manages multiple {@link TabModelSelector} instances, each owned by different {@link Activity}s.
 */
public class TabWindowManager implements ActivityStateListener {
    /**
     * An index that represents the invalid state (i.e. when the window wasn't found in the list.
     */
    public static final int INVALID_WINDOW_INDEX = -1;

    /** The maximum number of simultaneous TabModelSelector instances in this Application. */
    public static final int MAX_SIMULTANEOUS_SELECTORS = 3;

    /**
     * A factory interface for building a {@link TabModelSelector} instance.
     */
    public interface TabModelSelectorFactory {
        /**
         * Builds a {@link TabModelSelector}.
         * @param activity      A {@link ChromeActivity} instance.
         * @param windowAndroid A {@link WindowAndroid} instance that should connect to
         *                      {@code activity}.
         * @param selectorIndex The index of the {@link TabModelSelector}.
         * @return              A new {@link TabModelSelector} instance.
         */
        TabModelSelector buildSelector(ChromeActivity activity, WindowAndroid windowAndroid,
                int selectorIndex);
    }

    /** The singleton reference. */
    private static TabWindowManager sInstance;

    private TabModelSelectorFactory mSelectorFactory = new DefaultTabModelSelectorFactory();

    private List<TabModelSelector> mSelectors = new ArrayList<TabModelSelector>();

    private Map<Activity, TabModelSelector> mAssignments =
            new HashMap<Activity, TabModelSelector>();

    /**
     * @return The singleton instance of {@link TabWindowManager}.
     */
    public static TabWindowManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new TabWindowManager();
        return sInstance;
    }

    /**
     * Called to request a {@link TabModelSelector} based on {@code index}.  Note that the
     * {@link TabModelSelector} returned might not actually be the one related to {@code index}
     * and {@link #getIndexForWindow(Activity)} should be called to grab the actual index if
     * required.
     * @param activity An instance of {@link ChromeActivity}.  Must be the same {@link Activity} as
     *                 the one referenced by {@code window}.
     * @param window   A {@link WindowAndroid} as an instance.  The {@link TabModelSelector} that is
     *                 created is bound to the {@link Activity} stored inside this window.
     * @param index    The index of the requested {@link TabModelSelector}.  Not guaranteed to be
     *                 the index of the {@link TabModelSelector} returned.
     * @return         A {@link TabModelSelector} index, or {@code null} if there are too many
     *                 {@link TabModelSelector}s already built.
     */
    public TabModelSelector requestSelector(ChromeActivity activity, WindowAndroid window,
            int index) {
        if (mAssignments.get(activity) != null) {
            return mAssignments.get(activity);
        }

        if (index < 0 || index >= mSelectors.size()) index = 0;

        if (mSelectors.get(index) != null) {
            for (int i = 0; i < mSelectors.size(); i++) {
                if (mSelectors.get(i) == null) {
                    index = i;
                    break;
                }
            }
        }

        // Too many activities going at once.
        if (mSelectors.get(index) != null) return null;

        TabModelSelector selector = mSelectorFactory.buildSelector(activity, window, index);
        mSelectors.set(index, selector);
        mAssignments.put(activity, selector);

        return selector;
    }

    /**
     * Finds the current index of the {@link TabModelSelector} bound to {@code window}.
     * @param activity The {@link Activity} to find the index of the {@link TabModelSelector}
     *                 for.  This uses the underlying {@link Activity} stored in the
     *                 {@link WindowAndroid}.
     * @return         The index of the {@link TabModelSelector} or {@link #INVALID_WINDOW_INDEX} if
     *                 not found.
     */
    public int getIndexForWindow(Activity activity) {
        if (activity == null) return INVALID_WINDOW_INDEX;
        TabModelSelector selector = mAssignments.get(activity);
        if (selector == null) return INVALID_WINDOW_INDEX;
        int index = mSelectors.indexOf(selector);
        return index == -1 ? INVALID_WINDOW_INDEX : index;
    }

    /**
     * @return The number of {@link TabModelSelector}s currently assigned to {@link Activity}s.
     */
    public int getNumberOfAssignedTabModelSelectors() {
        return mAssignments.size();
    }

    /**
     * @return The total number of incognito tabs across all tab model selectors.
     */
    public int getIncognitoTabCount() {
        int count = 0;
        for (int i = 0; i < mSelectors.size(); i++) {
            if (mSelectors.get(i) != null) {
                count += mSelectors.get(i).getModel(true).getCount();
            }
        }

        // Count tabs that are moving between activities (e.g. a tab that was recently reparented
        // and hasn't been attached to its new activity yet).
        SparseArray<AsyncTabParams> asyncTabParams = AsyncTabParamsManager.getAsyncTabParams();
        for (int i = 0; i < asyncTabParams.size(); i++) {
            Tab tab = asyncTabParams.valueAt(i).getTabToReparent();
            if (tab != null && tab.isIncognito()) count++;
        }
        return count;
    }

    /**
     * @param tabId The ID of the tab in question.
     * @return Whether the given tab exists in any currently loaded selector.
     */
    public boolean tabExistsInAnySelector(int tabId) {
        for (int i = 0; i < mSelectors.size(); i++) {
            TabModelSelector selector = mSelectors.get(i);
            if (selector != null) {
                if (TabModelUtils.getTabById(selector.getModel(false), tabId) != null
                        || TabModelUtils.getTabById(selector.getModel(true), tabId) != null) {
                    return true;
                }
            }
        }

        // Account for tabs that were recently reparented and haven't been attached to new
        // activities yet.
        return AsyncTabParamsManager.hasParamsForTabId(tabId);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.DESTROYED && mAssignments.containsKey(activity)) {
            int index = mSelectors.indexOf(mAssignments.remove(activity));
            if (index >= 0) mSelectors.set(index, null);
            // TODO(dtrainor): Move TabModelSelector#destroy() calls here.
        }
    }

    /**
     * Allows overriding the default {@link TabModelSelectorFactory} with another one.  Typically
     * for testing.
     * @param factory A {@link TabModelSelectorFactory} instance.
     */
    @VisibleForTesting
    public void setTabModelSelectorFactory(TabModelSelectorFactory factory) {
        mSelectorFactory = factory;
    }

    private TabWindowManager() {
        ApplicationStatus.registerStateListenerForAllActivities(this);

        for (int i = 0; i < MAX_SIMULTANEOUS_SELECTORS; i++) mSelectors.add(null);
    }

    private static class DefaultTabModelSelectorFactory implements TabModelSelectorFactory {
        @Override
        public TabModelSelector buildSelector(ChromeActivity activity, WindowAndroid windowAndroid,
                int selectorIndex) {
            assert activity == windowAndroid.getActivity().get();
            return new TabModelSelectorImpl(activity, selectorIndex, windowAndroid);
        }
    }
}
