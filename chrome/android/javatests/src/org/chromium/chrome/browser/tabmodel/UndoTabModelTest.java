// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.annotation.TargetApi;
import android.os.Build;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtilsTest;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeRestriction;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests undo and restoring of tabs in a {@link TabModel}.
 */
public class UndoTabModelTest extends ChromeTabbedActivityTestBase {
    private static final Tab[] EMPTY = new Tab[] { };
    private static final String TEST_URL_0 = UrlUtils.encodeHtmlDataUri("<html>test_url_0.</html>");
    private static final String TEST_URL_1 = UrlUtils.encodeHtmlDataUri("<html>test_url_1.</html>");

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void checkState(
            final TabModel model, final Tab[] tabsList, final Tab selectedTab,
            final Tab[] closingTabs, final Tab[] fullTabsList,
            final Tab fullSelectedTab) {
        // Keeping these checks on the test thread so the stacks are useful for identifying
        // failures.

        // Check the selected tab.
        assertEquals("Wrong selected tab", selectedTab, TabModelUtils.getCurrentTab(model));

        // Check the list of tabs.
        assertEquals("Incorrect number of tabs", tabsList.length, model.getCount());
        for (int i = 0; i < tabsList.length; i++) {
            assertEquals("Unexpected tab at " + i, tabsList[i].getId(), model.getTabAt(i).getId());
        }

        // Check the list of tabs we expect to be closing.
        for (int i = 0; i < closingTabs.length; i++) {
            int id = closingTabs[i].getId();
            assertTrue("Tab " + id + " not in closing list", model.isClosurePending(id));
        }

        TabList fullModel = model.getComprehensiveModel();

        // Check the comprehensive selected tab.
        assertEquals("Wrong selected tab", fullSelectedTab, TabModelUtils.getCurrentTab(fullModel));

        // Check the comprehensive list of tabs.
        assertEquals("Incorrect number of tabs", fullTabsList.length, fullModel.getCount());
        for (int i = 0; i < fullModel.getCount(); i++) {
            int id = fullModel.getTabAt(i).getId();
            assertEquals("Unexpected tab at " + i, fullTabsList[i].getId(), id);
        }
    }

    private void createTabOnUiThread(final ChromeTabCreator tabCreator) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                tabCreator.createNewTab(new LoadUrlParams("about:blank"),
                        TabLaunchType.FROM_CHROME_UI, null);
            }
        });
    }

    private void selectTabOnUiThread(final TabModel model, final Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER);
            }
        });
    }

    private void closeTabOnUiThread(
            final TabModel model, final Tab tab, final boolean undoable)
            throws InterruptedException {
        // Check preconditions.
        assertFalse(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertFalse(model.isClosurePending(tab.getId()));
        assertNotNull(TabModelUtils.getTabById(model, tab.getId()));

        final CallbackHelper didReceivePendingClosureHelper = new CallbackHelper();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabPendingClosure(Tab tab) {
                didReceivePendingClosureHelper.notifyCalled();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Take action.
                model.closeTab(tab, true, false, undoable);
            }
        });

        boolean didUndo = undoable && model.supportsPendingClosures();

        // Make sure the TabModel throws a tabPendingClosure callback if necessary.
        if (didUndo) {
            try {
                didReceivePendingClosureHelper.waitForCallback(0);
            } catch (TimeoutException e) {
                fail();
            }
        }

        // Check post conditions
        assertEquals(didUndo, model.isClosurePending(tab.getId()));
        assertNull(TabModelUtils.getTabById(model, tab.getId()));
        assertTrue(tab.isClosing());
        assertEquals(didUndo, tab.isInitialized());
    }

    private void closeAllTabsOnUiThread(final TabModel model) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                model.closeAllTabs();
            }
        });
    }

    private void moveTabOnUiThread(final TabModel model, final Tab tab, final int newIndex) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                model.moveTab(tab.getId(), newIndex);
            }
        });
    }

    private void cancelTabClosureOnUiThread(final TabModel model, final Tab tab)
            throws InterruptedException {
        // Check preconditions.
        assertTrue(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertTrue(model.isClosurePending(tab.getId()));
        assertNull(TabModelUtils.getTabById(model, tab.getId()));

        final CallbackHelper didReceiveClosureCancelledHelper = new CallbackHelper();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabClosureUndone(Tab tab) {
                didReceiveClosureCancelledHelper.notifyCalled();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Take action.
                model.cancelTabClosure(tab.getId());
            }
        });

        // Make sure the TabModel throws a tabClosureUndone.
        try {
            didReceiveClosureCancelledHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }

        // Check post conditions.
        assertFalse(model.isClosurePending(tab.getId()));
        assertNotNull(TabModelUtils.getTabById(model, tab.getId()));
        assertFalse(tab.isClosing());
        assertTrue(tab.isInitialized());
    }

    private void cancelAllTabClosuresOnUiThread(final TabModel model, final Tab[] expectedToClose)
            throws InterruptedException {
        final CallbackHelper tabClosureUndoneHelper = new CallbackHelper();

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertTrue(tab.isInitialized());
            assertTrue(model.isClosurePending(tab.getId()));
            assertNull(TabModelUtils.getTabById(model, tab.getId()));

            // Make sure that this TabModel throws the right events.
            model.addObserver(new EmptyTabModelObserver() {
                @Override
                public void tabClosureUndone(Tab currentTab) {
                    tabClosureUndoneHelper.notifyCalled();
                }
            });
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < expectedToClose.length; i++) {
                    Tab tab = expectedToClose[i];
                    model.cancelTabClosure(tab.getId());
                }
            }
        });

        try {
            tabClosureUndoneHelper.waitForCallback(0, expectedToClose.length);
        } catch (TimeoutException e) {
            fail();
        }

        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            assertFalse(model.isClosurePending(tab.getId()));
            assertNotNull(TabModelUtils.getTabById(model, tab.getId()));
            assertFalse(tab.isClosing());
            assertTrue(tab.isInitialized());
        }
    }

    private void commitTabClosureOnUiThread(final TabModel model, final Tab tab)
            throws InterruptedException {
        // Check preconditions.
        assertTrue(tab.isClosing());
        assertTrue(tab.isInitialized());
        assertTrue(model.isClosurePending(tab.getId()));
        assertNull(TabModelUtils.getTabById(model, tab.getId()));

        final CallbackHelper didReceiveClosureCommittedHelper = new CallbackHelper();
        model.addObserver(new EmptyTabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                didReceiveClosureCommittedHelper.notifyCalled();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Take action.
                model.commitTabClosure(tab.getId());
            }
        });

        // Make sure the TabModel throws a tabClosureCommitted.
        try {
            didReceiveClosureCommittedHelper.waitForCallback(0);
        } catch (TimeoutException e) {
            fail();
        }

        // Check post conditions
        assertFalse(model.isClosurePending(tab.getId()));
        assertNull(TabModelUtils.getTabById(model, tab.getId()));
        assertTrue(tab.isClosing());
        assertFalse(tab.isInitialized());
    }

    private void commitAllTabClosuresOnUiThread(final TabModel model, Tab[] expectedToClose)
            throws InterruptedException {
        final CallbackHelper tabClosureCommittedHelper = new CallbackHelper();

        for (int i = 0; i < expectedToClose.length; i++) {
            Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertTrue(tab.isInitialized());
            assertTrue(model.isClosurePending(tab.getId()));

            // Make sure that this TabModel throws the right events.
            model.addObserver(new EmptyTabModelObserver() {
                @Override
                public void tabClosureCommitted(Tab currentTab) {
                    tabClosureCommittedHelper.notifyCalled();
                }
            });
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                model.commitAllTabClosures();
            }
        });

        try {
            tabClosureCommittedHelper.waitForCallback(0, expectedToClose.length);
        } catch (TimeoutException e) {
            fail();
        }
        for (int i = 0; i < expectedToClose.length; i++) {
            final Tab tab = expectedToClose[i];
            assertTrue(tab.isClosing());
            assertFalse(tab.isInitialized());
            assertFalse(model.isClosurePending(tab.getId()));
        }
    }

    private void saveStateOnUiThread(final TabModelSelector selector) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ((TabModelSelectorImpl) selector).saveState();
            }
        });

        for (int i = 0; i < selector.getModels().size(); i++) {
            TabList tabs = selector.getModelAt(i).getComprehensiveModel();
            for (int j = 0; j < tabs.getCount(); j++) {
                assertFalse(tabs.isClosurePending(tabs.getTabAt(j).getId()));
            }
        }
    }

    private void openMostRecentlyClosedTabOnUiThread(final TabModelSelector selector) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                selector.getCurrentModel().openMostRecentlyClosedTab();
            }
        });
    }

    // Helper class that notifies after the tab is closed, and a tab restore service entry has been
    // created in tab restore service.
    private static class TabClosedObserver extends EmptyTabModelObserver {
        private CallbackHelper mTabClosedCallback;

        public TabClosedObserver(CallbackHelper closedCallback) {
            mTabClosedCallback = closedCallback;
        }

        @Override
        public void didCloseTab(int tabId, boolean incognito) {
            mTabClosedCallback.notifyCalled();
        }
    }

    /**
     * Test undo with a single tab with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0s ]             -                 [ 0s ]
     * 2.  CloseTab(0, allow undo)    -                  [ 0 ]             [ 0s ]
     * 3.  CancelClose(0)             [ 0s ]             -                 [ 0s ]
     * 4.  CloseTab(0, allow undo)    -                  [ 0 ]             [ 0s ]
     * 5.  CommitClose(0)             -                  -                 -
     * 6.  CreateTab(0)               [ 0s ]             -                 [ 0s ]
     * 7.  CloseTab(0, allow undo)    -                  [ 0 ]             [ 0s ]
     * 8.  CommitAllClose             -                  -                 -
     * 9.  CreateTab(0)               [ 0s ]             -                 [ 0s ]
     * 10. CloseTab(0, disallow undo) -                  -                 -
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testSingleTab() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);

        Tab tab0 = model.getTabAt(0);

        Tab[] fullList = new Tab[] { tab0 };

        // 1.
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0 }, fullList, tab0);

        // 3.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 4.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0 }, fullList, tab0);

        // 5.
        commitTabClosureOnUiThread(model, tab0);
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);

        // 6.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 7.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0 }, fullList, tab0);

        // 8.
        commitAllTabClosuresOnUiThread(model, new Tab[] { tab0 });
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);

        // 9.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 10.
        closeTabOnUiThread(model, tab0, false);
        fullList = EMPTY;
        checkState(model, EMPTY, null, EMPTY, fullList, null);
        assertTrue(tab0.isClosing());
        assertFalse(tab0.isInitialized());
    }

    /**
     * Test undo with two tabs with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1s ]           -                 [ 0 1s ]
     * 2.  CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 0 1s ]
     * 3.  CancelClose(0)             [ 0 1s ]           -                 [ 0 1s ]
     * 4.  CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 0 1s ]
     * 5.  CloseTab(1, allow undo)    -                  [ 1 0 ]           [ 0s 1 ]
     * 6.  CancelClose(1)             [ 1s ]             [ 0 ]             [ 0 1s ]
     * 7.  CancelClose(0)             [ 0 1s ]           -                 [ 0 1s ]
     * 8.  CloseTab(1, allow undo)    [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 9.  CloseTab(0, allow undo)    -                  [ 0 1 ]           [ 0s 1 ]
     * 10. CancelClose(1)             [ 1s ]             [ 0 ]             [ 0 1s ]
     * 11. CancelClose(0)             [ 0 1s ]           -                 [ 0 1s ]
     * 12. CloseTab(1, allow undo)    [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 13. CloseTab(0, allow undo)    -                  [ 0 1 ]           [ 0s 1 ]
     * 14. CancelClose(0)             [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 15. CloseTab(0, allow undo)    -                  [ 0 1 ]           [ 0s 1 ]
     * 16. CancelClose(0)             [ 0s ]             [ 1 ]             [ 0s 1 ]
     * 17. CancelClose(1)             [ 0s 1 ]           -                 [ 0s 1 ]
     * 18. CloseTab(0, disallow undo) [ 1s ]             -                 [ 1s ]
     * 19. CreateTab(0)               [ 1 0s ]           -                 [ 1 0s ]
     * 20. CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 1s 0 ]
     * 21. CommitClose(0)             [ 1s ]             -                 [ 1s ]
     * 22. CreateTab(0)               [ 1 0s ]           -                 [ 1 0s ]
     * 23. CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 1s 0 ]
     * 24. CloseTab(1, allow undo)    -                  [ 1 0 ]           [ 1s 0 ]
     * 25. CommitAllClose             -                  -                 -
     *
     * @throws InterruptedException
     */
    @MediumTest
    // TODO(jbudorick): Replace with DisableIf when it supports filtering by device type.
    // Flaky on tablets, crbug.com/620014.
    @Restriction(ChromeRestriction.RESTRICTION_TYPE_PHONE)
    public void testTwoTabs() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);

        Tab[] fullList = new Tab[] { tab0, tab1 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 3.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 4.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 5.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 7.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 8.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 9.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 10.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 11.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 12.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 13.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 14.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 15.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1 }, fullList, tab0);

        // 16.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1 }, fullList, tab0);

        // 17.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, EMPTY, fullList, tab0);

        // 18.
        closeTabOnUiThread(model, tab0, false);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);

        // 19.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(1);
        fullList = new Tab[] { tab1, tab0 };
        checkState(model, new Tab[] { tab1, tab0 }, tab0, EMPTY, fullList, tab0);

        // 20.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 21.
        commitTabClosureOnUiThread(model, tab0);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);

        // 22.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(1);
        fullList = new Tab[] { tab1, tab0 };
        checkState(model, new Tab[] { tab1, tab0 }, tab0, EMPTY, fullList, tab0);

        // 23.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0 }, fullList, tab1);

        // 24.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] { tab1, tab0 }, fullList, tab1);

        // 25.
        commitAllTabClosuresOnUiThread(model, new Tab[] { tab1, tab0 });
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    /**
     * Test restoring in the same order of closing with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(0, allow undo)    [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(1, allow undo)    [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(2, allow undo)    [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 5.  CloseTab(3, allow undo)    -                  [ 3 2 1 0 ]       [ 0s 1 2 3 ]
     * 6.  CancelClose(3)             [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 7.  CancelClose(2)             [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 8.  CancelClose(1)             [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 9.  CancelClose(0)             [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 10. SelectTab(3)               [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 11. CloseTab(3, allow undo)    [ 0 1 2s ]         [ 3 ]             [ 0 1 2s 3 ]
     * 12. CloseTab(2, allow undo)    [ 0 1s ]           [ 2 3 ]           [ 0 1s 2 3 ]
     * 13. CloseTab(1, allow undo)    [ 0s ]             [ 1 2 3 ]         [ 0s 1 2 3 ]
     * 14. CloseTab(0, allow undo)    -                  [ 0 1 2 3 ]       [ 0s 1 2 3 ]
     * 15. CancelClose(0)             [ 0s ]             [ 1 2 3 ]         [ 0s 1 2 3 ]
     * 16. CancelClose(1)             [ 0s 1 ]           [ 2 3 ]           [ 0s 1 2 3 ]
     * 17. CancelClose(2)             [ 0s 1 2 ]         [ 3 ]             [ 0s 1 2 3 ]
     * 18. CancelClose(3)             [ 0s 1 2 3 ]       -                 [ 0s 1 2 3 ]
     * 19. CloseTab(2, allow undo)    [ 0s 1 3 ]         [ 2 ]             [ 0s 1 2 3 ]
     * 20. CloseTab(0, allow undo)    [ 1s 3 ]           [ 0 2 ]           [ 0 1s 2 3 ]
     * 21. CloseTab(3, allow undo)    [ 1s ]             [ 3 0 2 ]         [ 0 1s 2 3 ]
     * 22. CancelClose(3)             [ 1s 3 ]           [ 0 2 ]           [ 0 1s 2 3 ]
     * 23. CancelClose(0)             [ 0 1s 3 ]         [ 2 ]             [ 0 1s 2 3 ]
     * 24. CancelClose(2)             [ 0 1s 2 3 ]       -                 [ 0 1s 2 3 ]
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testInOrderRestore() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        final Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 5.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] { tab3, tab2, tab1, tab0 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 7.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 8.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 9.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 10.
        selectTabOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 11.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab2, new Tab[] { tab3 },
                fullList, tab2);

        // 12.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, new Tab[] { tab2, tab3 },
                fullList, tab1);

        // 13.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1, tab2, tab3 },
                fullList, tab0);

        // 14.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1, tab2, tab3 }, fullList, tab0);

        // 15.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1, tab2, tab3 },
                fullList, tab0);

        // 16.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, new Tab[] { tab2, tab3 },
                fullList, tab0);

        // 17.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab0, new Tab[] { tab3 },
                fullList, tab0);

        // 18.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab0, EMPTY, fullList, tab0);

        // 19.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1, tab3 }, tab0, new Tab[] { tab2 },
                fullList, tab0);

        // 20.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab3 }, tab1, new Tab[] { tab0, tab2 },
                fullList, tab1);

        // 21.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab3, tab0, tab2 },
                fullList, tab1);

        // 22.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab1, tab3 }, tab1, new Tab[] { tab0, tab2 },
                fullList, tab1);

        // 23.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab3 }, tab1, new Tab[] { tab2 },
                fullList, tab1);

        // 24.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab1, EMPTY, fullList, tab1);
    }


    /**
     * Test restoring in the reverse of closing with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(0, allow undo)    [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(1, allow undo)    [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(2, allow undo)    [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 5.  CloseTab(3, allow undo)    -                  [ 3 2 1 0 ]       [ 0s 1 2 3 ]
     * 6.  CancelClose(0)             [ 0s ]             [ 3 2 1 ]         [ 0s 1 2 3 ]
     * 7.  CancelClose(1)             [ 0s 1 ]           [ 3 2 ]           [ 0s 1 2 3 ]
     * 8.  CancelClose(2)             [ 0s 1 2 ]         [ 3 ]             [ 0s 1 2 3 ]
     * 9.  CancelClose(3)             [ 0s 1 2 3 ]       -                 [ 0s 1 2 3 ]
     * 10. CloseTab(3, allow undo)    [ 0s 1 2 ]         [ 3 ]             [ 0s 1 2 3 ]
     * 11. CloseTab(2, allow undo)    [ 0s 1 ]           [ 2 3 ]           [ 0s 1 2 3 ]
     * 12. CloseTab(1, allow undo)    [ 0s ]             [ 1 2 3 ]         [ 0s 1 2 3 ]
     * 13. CloseTab(0, allow undo)    -                  [ 0 1 2 3 ]       [ 0s 1 2 3 ]
     * 14. CancelClose(3)             [ 3s ]             [ 0 1 2 ]         [ 0 1 2 3s ]
     * 15. CancelClose(2)             [ 2 3s ]           [ 0 1 ]           [ 0 1 2 3s ]
     * 16. CancelClose(1)             [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 17. CancelClose(0)             [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 18. SelectTab(3)               [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 19. CloseTab(2, allow undo)    [ 0 1 3s ]         [ 2 ]             [ 0 1 2 3s ]
     * 20. CloseTab(0, allow undo)    [ 1 3s ]           [ 0 2 ]           [ 0 1 2 3s ]
     * 21. CloseTab(3, allow undo)    [ 1s ]             [ 3 0 2 ]         [ 0 1s 2 3 ]
     * 22. CancelClose(2)             [ 1s 2 ]           [ 3 0 ]           [ 0 1s 2 3 ]
     * 23. CancelClose(0)             [ 0 1s 2 ]         [ 3 ]             [ 0 1s 2 3 ]
     * 24. CancelClose(3)             [ 0 1s 2 3 ]       -                 [ 0 1s 2 3 ]
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testReverseOrderRestore() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 5.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] { tab3, tab2, tab1, tab0 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab3, tab2, tab1 },
                fullList, tab0);

        // 7.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, new Tab[] { tab3, tab2 },
                fullList, tab0);

        // 8.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab0, new Tab[] { tab3 },
                fullList, tab0);

        // 9.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab0, EMPTY, fullList, tab0);

        // 10.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab0, new Tab[] { tab3 },
                fullList, tab0);

        // 11.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1 }, tab0, new Tab[] { tab2, tab3 },
                fullList, tab0);

        // 12.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0 }, tab0, new Tab[] { tab1, tab2, tab3 },
                fullList, tab0);

        // 13.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, EMPTY, null, new Tab[] { tab0, tab1, tab2, tab3 }, fullList, tab0);

        // 14.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab0, tab1, tab2 },
                fullList, tab3);

        // 15.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab0, tab1 },
                fullList, tab3);

        // 16.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 17.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 18.
        selectTabOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 19.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1, tab3 }, tab3, new Tab[] { tab2 },
                fullList, tab3);

        // 20.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab3 }, tab3, new Tab[] { tab0, tab2 },
                fullList, tab3);

        // 21.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab3, tab0, tab2 },
                fullList, tab1);

        // 22.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab1, tab2 }, tab1, new Tab[] { tab3, tab0 },
                fullList, tab1);

        // 23.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab1, new Tab[] { tab3 },
                fullList, tab1);

        // 24.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab1, EMPTY, fullList, tab1);
    }

    /**
     * Test restoring out of order with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(0, allow undo)    [ 1 2 3s ]         [ 0 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(1, allow undo)    [ 2 3s ]           [ 1 0 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(2, allow undo)    [ 3s ]             [ 2 1 0 ]         [ 0 1 2 3s ]
     * 5.  CloseTab(3, allow undo)    -                  [ 3 2 1 0 ]       [ 0s 1 2 3 ]
     * 6.  CancelClose(2)             [ 2s ]             [ 3 1 0 ]         [ 0 1 2s 3 ]
     * 7.  CancelClose(1)             [ 1 2s ]           [ 3 0 ]           [ 0 1 2s 3 ]
     * 8.  CancelClose(3)             [ 1 2s 3 ]         [ 0 ]             [ 0 1 2s 3 ]
     * 9.  CancelClose(0)             [ 0 1 2s 3 ]       -                 [ 0 1 2s 3 ]
     * 10. CloseTab(1, allow undo)    [ 0 2s 3 ]         [ 1 ]             [ 0 1 2s 3 ]
     * 11. CancelClose(1)             [ 0 1 2s 3 ]       -                 [ 0 1 2s 3 ]
     * 12. CloseTab(3, disallow undo) [ 0 1 2s ]         -                 [ 0 1 2s ]
     * 13. CloseTab(1, allow undo)    [ 0 2s ]           [ 1 ]             [ 0 1 2s ]
     * 14. CloseTab(0, allow undo)    [ 2s ]             [ 0 1 ]           [ 0 1 2s ]
     * 15. CommitClose(0)             [ 2s ]             [ 1 ]             [ 1 2s ]
     * 16. CancelClose(1)             [ 1 2s ]           -                 [ 1 2s ]
     * 17. CloseTab(2, disallow undo) [ 1s ]             -                 [ 1s ]
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testOutOfOrder1() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab3, new Tab[] { tab0 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab2, tab3 }, tab3, new Tab[] { tab1, tab0 },
                fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab2, tab1, tab0 },
                fullList, tab3);

        // 5.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, EMPTY, null, new Tab[] { tab3, tab2, tab1, tab0 }, fullList, tab0);

        // 6.
        cancelTabClosureOnUiThread(model, tab2);
        checkState(model, new Tab[] { tab2 }, tab2, new Tab[] { tab3, tab1, tab0 },
                fullList, tab2);

        // 7.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2 }, tab2, new Tab[] { tab3, tab0 },
                fullList, tab2);

        // 8.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab1, tab2, tab3 }, tab2, new Tab[] { tab0 },
                fullList, tab2);

        // 9.
        cancelTabClosureOnUiThread(model, tab0);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab2, EMPTY, fullList, tab2);

        // 10.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab2, new Tab[] { tab1 },
                fullList, tab2);

        // 11.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab2, EMPTY, fullList, tab2);

        // 12.
        closeTabOnUiThread(model, tab3, false);
        fullList = new Tab[] { tab0, tab1, tab2 };
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab2, EMPTY, fullList, tab2);

        // 13.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2 }, tab2, new Tab[] { tab1 }, fullList,
                tab2);

        // 14.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab2 }, tab2, new Tab[] { tab0, tab1 }, fullList,
                tab2);

        // 15.
        commitTabClosureOnUiThread(model, tab0);
        fullList = new Tab[] { tab1, tab2 };
        checkState(model, new Tab[] { tab2 }, tab2, new Tab[] { tab1 }, fullList, tab2);

        // 16.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab2 }, tab2, EMPTY, fullList, tab2);

        // 17.
        closeTabOnUiThread(model, tab2, false);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 },  tab1, EMPTY, fullList, tab1);
    }

    /**
     * Test restoring out of order with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(3, allow undo)    [ 0 2s ]           [ 3 1 ]           [ 0 1 2s 3 ]
     * 4.  CancelClose(1)             [ 0 1 2s ]         [ 3 ]             [ 0 1 2s 3 ]
     * 5.  CloseTab(2, allow undo)    [ 0 1s ]           [ 2 3 ]           [ 0 1s 2 3 ]
     * 6.  CloseTab(0, allow undo)    [ 1s ]             [ 0 2 3 ]         [ 0 1s 2 3 ]
     * 7.  CommitClose(0)             [ 1s ]             [ 2 3 ]           [ 1s 2 3 ]
     * 8.  CancelClose(3)             [ 1s 3 ]           [ 2 ]             [ 1s 2 3 ]
     * 9.  CloseTab(1, allow undo)    [ 3s ]             [ 1 2 ]           [ 1 2 3s ]
     * 10. CommitClose(2)             [ 3s ]             [ 1 ]             [ 1 3s ]
     * 11. CancelClose(1)             [ 1 3s ]           -                 [ 1 3s ]
     * 12. CloseTab(3, allow undo)    [ 1s ]             [ 3 ]             [ 1s 3 ]
     * 13. CloseTab(1, allow undo)    -                  [ 1 3 ]           [ 1s 3 ]
     * 14. CommitAll                  -                  -                 -
     *
     * @throws InterruptedException
     */
    @MediumTest
    @FlakyTest(message = "crbug.com/592969")
    public void testOutOfOrder2() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, new Tab[] { tab1 },
                fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab0, tab2 }, tab2, new Tab[] { tab3, tab1 },
                fullList, tab2);

        // 4.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab0, tab1, tab2 }, tab2, new Tab[] { tab3 },
                fullList, tab2);

        // 5.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab1 }, tab1, new Tab[] { tab2, tab3 },
                fullList, tab1);

        // 6.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab0, tab2, tab3 },
                fullList, tab1);

        // 7.
        commitTabClosureOnUiThread(model, tab0);
        fullList = new Tab[] { tab1, tab2, tab3 };
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab2, tab3 }, fullList,
                tab1);

        // 8.
        cancelTabClosureOnUiThread(model, tab3);
        checkState(model, new Tab[] { tab1, tab3 }, tab1, new Tab[] { tab2 }, fullList,
                tab1);

        // 9.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab1, tab2 }, fullList,
                tab3);

        // 10.
        commitTabClosureOnUiThread(model, tab2);
        fullList = new Tab[] { tab1, tab3 };
        checkState(model, new Tab[] { tab3 }, tab3, new Tab[] { tab1 }, fullList, tab3);

        // 11.
        cancelTabClosureOnUiThread(model, tab1);
        checkState(model, new Tab[] { tab1, tab3 }, tab3, EMPTY, fullList, tab3);

        // 12.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab1 }, tab1, new Tab[] { tab3 }, fullList, tab1);

        // 13.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, EMPTY, null, new Tab[] { tab1, tab3 }, fullList, tab1);

        // 14.
        commitAllTabClosuresOnUiThread(model, new Tab[] { tab1, tab3 });
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
    }

    /**
     * Test undo {@link TabModel#closeAllTabs()} with the following actions/expected states:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0  1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0  1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0  1 2 3s ]
     * 4.  CloseAll                   -                  [ 0 3 2 1 ]       [ 0s 1 2 3  ]
     * 5.  CancelAllClose             [ 0 1 2 3s ]       -                 [ 0  1 2 3s ]
     * 6.  CloseAll                   -                  [ 0 1 2 3 ]       [ 0s 1 2 3  ]
     * 7.  CommitAllClose             -                  -                 -
     * 8.  CreateTab(0)               [ 0s ]             -                 [ 0s ]
     * 9.  CloseAll                   -                  [ 0 ]             [ 0s ]
     *
     * @throws InterruptedException
     */
    @MediumTest
    @DisabledTest(message = "crbug.com/633607")
    public void testCloseAll() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, fullList, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, new Tab[] { tab1 }, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, new Tab[] { tab1, tab2 }, fullList, tab3);

        // 4.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, fullList, fullList, tab0);

        // 5.
        cancelAllTabClosuresOnUiThread(model, fullList);
        checkState(model, fullList, tab0, EMPTY, fullList, tab0);

        // 6.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, fullList, fullList, tab0);

        // 7.
        commitAllTabClosuresOnUiThread(model, fullList);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());

        // 8.
        createTabOnUiThread(tabCreator);
        tab0 = model.getTabAt(0);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);

        // 9.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, fullList, fullList, tab0);
        assertTrue(tab0.isClosing());
        assertTrue(tab0.isInitialized());
    }

    /**
     * Test {@link TabModel#closeTab(Tab)} when not allowing a close commits all pending
     * closes:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0 1 2 3s ]
     * 4.  CloseTab(3, disallow undo) [ 0s ]             -                 [ 0s ]
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testCloseTab() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, EMPTY, fullList, tab3);

        // 4.
        closeTabOnUiThread(model, tab3, false);
        fullList = new Tab[] { tab0 };
        checkState(model, new Tab[] { tab0 }, tab0, EMPTY, fullList, tab0);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());
    }

    /**
     * Test {@link TabModel#moveTab(int, int)} commits all pending closes:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0 1 2 3s ]
     * 4.  MoveTab(0, 2)              [ 3s 0 ]           -                 [ 3s 0 ]
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testMoveTab() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, EMPTY, fullList, tab3);

        // 4.
        moveTabOnUiThread(model, tab0, 2);
        fullList = new Tab[] { tab3, tab0 };
        checkState(model, new Tab[] { tab3, tab0 }, tab3, EMPTY, fullList, tab3);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab1.isInitialized());
    }

    /**
     * Test adding a {@link Tab} to a {@link TabModel} commits all pending closes:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         [ 1 ]             [ 0 1 2 3s ]
     * 3.  CloseTab(2, allow undo)    [ 0 3s ]           [ 2 1 ]           [ 0 1 2 3s ]
     * 4.  CreateTab(4)               [ 0 3 4s ]         -                 [ 0 3 4s ]
     * 5.  CloseTab(0, allow undo)    [ 3 4s ]           [ 0 ]             [ 0 3 4s ]
     * 6.  CloseTab(3, allow undo)    [ 4s ]             [ 3 0 ]           [ 0 3 4s ]
     * 7.  CloseTab(4, allow undo)    -                  [ 4 3 0 ]         [ 0s 3 4 ]
     * 8.  CreateTab(5)               [ 5s ]             -                 [ 5s ]
     * @throws InterruptedException
     */
    @MediumTest
    public void testAddTab() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 2.
        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);

        // 3.
        closeTabOnUiThread(model, tab2, true);
        checkState(model, new Tab[] { tab0, tab3 }, tab3, EMPTY, fullList, tab3);

        // 4.
        createTabOnUiThread(tabCreator);
        Tab tab4 = model.getTabAt(2);
        fullList = new Tab[] { tab0, tab3, tab4 };
        checkState(model, new Tab[] { tab0, tab3, tab4 }, tab4, EMPTY, fullList, tab4);
        assertTrue(tab1.isClosing());
        assertTrue(tab2.isClosing());
        assertFalse(tab1.isInitialized());
        assertFalse(tab2.isInitialized());

        // 5.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab3, tab4 }, tab4, new Tab[] { tab0 }, fullList,
                tab4);

        // 6.
        closeTabOnUiThread(model, tab3, true);
        checkState(model, new Tab[] { tab4 }, tab4, new Tab[] { tab3, tab0 }, fullList,
                tab4);

        // 7.
        closeTabOnUiThread(model, tab4, true);
        checkState(model, EMPTY, null, new Tab[] { tab4, tab3, tab0 }, fullList, tab0);

        // 8.
        createTabOnUiThread(tabCreator);
        Tab tab5 = model.getTabAt(0);
        fullList = new Tab[] { tab5 };
        checkState(model, new Tab[] { tab5 }, tab5, EMPTY, fullList, tab5);
        assertTrue(tab0.isClosing());
        assertTrue(tab3.isClosing());
        assertTrue(tab4.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab3.isInitialized());
        assertFalse(tab4.isInitialized());
    }

    /**
     * Test a {@link TabModel} where undo is not supported:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1 2 3s ]       -                 [ 0 1 2 3s ]
     * 2.  CloseTab(1, allow undo)    [ 0 2 3s ]         -                 [ 0 2 3s ]
     * 3.  CloseAll                   -                  -                 -
     *
     * @throws InterruptedException
     */
    @MediumTest
    public void testUndoNotSupported() throws InterruptedException {
        TabModel model = getActivity().getTabModelSelector().getModel(true);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(true);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab tab2 = model.getTabAt(2);
        Tab tab3 = model.getTabAt(3);

        Tab[] fullList = new Tab[] { tab0, tab1, tab2, tab3 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1, tab2, tab3 }, tab3, EMPTY, fullList, tab3);
        assertFalse(model.supportsPendingClosures());

        // 2.
        closeTabOnUiThread(model, tab1, true);
        fullList = new Tab[] { tab0, tab2, tab3 };
        checkState(model, new Tab[] { tab0, tab2, tab3 }, tab3, EMPTY, fullList, tab3);
        assertTrue(tab1.isClosing());
        assertFalse(tab1.isInitialized());

        // 3.
        closeAllTabsOnUiThread(model);
        checkState(model, EMPTY, null, EMPTY, EMPTY, null);
        assertTrue(tab0.isClosing());
        assertTrue(tab2.isClosing());
        assertTrue(tab3.isClosing());
        assertFalse(tab0.isInitialized());
        assertFalse(tab2.isInitialized());
        assertFalse(tab3.isInitialized());
    }

    /**
     * Test calling {@link TabModelSelectorImpl#saveState()} commits all pending closures:
     *     Action                     Model List         Close List        Comprehensive List
     * 1.  Initial State              [ 0 1s ]           -                 [ 0 1s ]
     * 2.  CloseTab(0, allow undo)    [ 1s ]             [ 0 ]             [ 0 1s ]
     * 3.  SaveState                  [ 1s ]             -                 [ 1s ]
     */
    @MediumTest
    public void testSaveStateCommitsUndos() throws InterruptedException {
        TabModelSelector selector = getActivity().getTabModelSelector();
        TabModel model = selector.getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);
        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);

        Tab[] fullList = new Tab[] { tab0, tab1 };

        // 1.
        checkState(model, new Tab[] { tab0, tab1 }, tab1, EMPTY, fullList, tab1);

        // 2.
        closeTabOnUiThread(model, tab0, true);
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);

        // 3.
        saveStateOnUiThread(selector);
        fullList = new Tab[] { tab1 };
        checkState(model, new Tab[] { tab1 }, tab1, EMPTY, fullList, tab1);
        assertTrue(tab0.isClosing());
        assertFalse(tab0.isInitialized());
    }

    /**
     * Test opening recently closed tabs using the rewound list in Java.
     * @throws InterruptedException
     */
    @MediumTest
    public void testOpenRecentlyClosedTab() throws InterruptedException {
        TabModelSelector selector = getActivity().getTabModelSelector();
        TabModel model = selector.getModel(false);
        ChromeTabCreator tabCreator = getActivity().getTabCreator(false);

        createTabOnUiThread(tabCreator);

        Tab tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        Tab[] allTabs = new Tab[]{tab0, tab1};

        closeTabOnUiThread(model, tab1, true);
        checkState(model, new Tab[]{tab0}, tab0, new Tab[]{tab1}, allTabs, tab0);

        // Ensure tab recovery, and reuse of {@link Tab} objects in Java.
        openMostRecentlyClosedTabOnUiThread(selector);
        checkState(model, allTabs, tab0, EMPTY, allTabs, tab0);
    }

    /**
     * Test opening recently closed tab using native tab restore service.
     * @throws InterruptedException
     */
    @MediumTest
    public void testOpenRecentlyClosedTabNative() throws InterruptedException {
        final TabModelSelector selector = getActivity().getTabModelSelector();
        final TabModel model = selector.getModel(false);

        // Create new tab and wait until it's loaded.
        // Native can only successfully recover the tab after a page load has finished and
        // it has navigation history.
        ChromeTabUtils.fullyLoadUrlInNewTab(getInstrumentation(), getActivity(), TEST_URL_0, false);

        // Close the tab, and commit pending closure.
        assertEquals(model.getCount(), 2);
        closeTabOnUiThread(model, model.getTabAt(1), false);
        assertEquals(1, model.getCount());
        Tab tab0 = model.getTabAt(0);
        Tab[] tabs = new Tab[]{tab0};
        checkState(model, tabs, tab0, EMPTY, tabs, tab0);

        // Recover the page.
        openMostRecentlyClosedTabOnUiThread(selector);

        assertEquals(2, model.getCount());
        tab0 = model.getTabAt(0);
        Tab tab1 = model.getTabAt(1);
        tabs = new Tab[]{tab0, tab1};
        assertEquals(TEST_URL_0, tab1.getUrl());
        checkState(model, tabs, tab0, EMPTY, tabs, tab0);
    }

    /**
     * Test opening recently closed tab when we have multiple windows.
     * |  Action                    |   Result
     * 1. Create second window.     |
     * 2. Open tab in window 1.     |
     * 3. Open tab in window 2.     |
     * 4. Close tab in window 1.    |
     * 5. Close tab in window 2.    |
     * 6. Restore tab.              | Tab restored in window 2.
     * 7. Restore tab.              | Tab restored in window 1.
     * @throws InterruptedException
     */
    @MediumTest
    @MinAndroidSdkLevel(24)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testOpenRecentlyClosedTabMultiWindow() throws InterruptedException {
        final ChromeTabbedActivity2 secondActivity =
                MultiWindowUtilsTest.createSecondChromeTabbedActivity(getActivity());

        // Wait for the second window to be fully initialized.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return secondActivity.getTabModelSelector().isTabStateInitialized();
            }
        });
        // First window context.
        final TabModelSelector firstSelector = getActivity().getTabModelSelector();
        final TabModel firstModel = firstSelector.getModel(false);

        // Second window context.
        final TabModel secondModel = secondActivity.getTabModelSelector().getModel(false);

        // Create tabs.
        ChromeTabUtils.fullyLoadUrlInNewTab(getInstrumentation(), getActivity(), TEST_URL_0, false);
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), secondActivity, TEST_URL_1, false);

        assertEquals("Unexpected number of tabs in first window.", 2, firstModel.getCount());
        assertEquals("Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Close one tab in the first window.
        closeTabOnUiThread(firstModel, firstModel.getTabAt(1), false);
        assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        assertEquals("Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Close one tab in the second window.
        closeTabOnUiThread(secondModel, secondModel.getTabAt(1), false);
        assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        assertEquals("Unexpected number of tabs in second window.", 1, secondModel.getCount());

        // Restore one tab.
        openMostRecentlyClosedTabOnUiThread(firstSelector);
        assertEquals("Unexpected number of tabs in first window.", 1, firstModel.getCount());
        assertEquals("Unexpected number of tabs in second window.", 2, secondModel.getCount());

        // Restore one more tab.
        openMostRecentlyClosedTabOnUiThread(firstSelector);

        // Check final states of both windows.
        Tab firstModelTab = firstModel.getTabAt(0);
        Tab secondModelTab = secondModel.getTabAt(0);
        Tab[] firstWindowTabs = new Tab[]{firstModelTab, firstModel.getTabAt(1)};
        Tab[] secondWindowTabs = new Tab[]{secondModelTab, secondModel.getTabAt(1)};
        checkState(firstModel, firstWindowTabs, firstModelTab, EMPTY, firstWindowTabs,
                firstModelTab);
        checkState(secondModel, secondWindowTabs, secondModelTab, EMPTY, secondWindowTabs,
                secondModelTab);
        assertEquals(TEST_URL_0, firstWindowTabs[1].getUrl());
        assertEquals(TEST_URL_1, secondWindowTabs[1].getUrl());

        secondActivity.finishAndRemoveTask();
    }

    /**
     * Test restoring closed tab from a closed window.
     * |  Action                    |   Result
     * 1. Create second window.     |
     * 2. Open tab in window 2.     |
     * 3. Close tab in window 2.    |
     * 4. Close second window.      |
     * 5. Restore tab.              | Tab restored in first window.
     * @throws InterruptedException
     */
    @MediumTest
    @MinAndroidSdkLevel(24)
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public void testOpenRecentlyClosedTabMultiWindowFallback() throws InterruptedException {
        final ChromeTabbedActivity2 secondActivity =
                MultiWindowUtilsTest.createSecondChromeTabbedActivity(getActivity());
        // Wait for the second window to be fully initialized.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return secondActivity.getTabModelSelector().isTabStateInitialized();
            }
        });

        // First window context.
        final TabModelSelector firstSelector = getActivity().getTabModelSelector();
        final TabModel firstModel = firstSelector.getModel(false);

        // Second window context.
        final TabModel secondModel = secondActivity.getTabModelSelector().getModel(false);

        // Create tab on second window.
        ChromeTabUtils.fullyLoadUrlInNewTab(
                getInstrumentation(), secondActivity, TEST_URL_1, false);
        assertEquals("Window 2 should have 2 tab.", 2, secondModel.getCount());

        // Close tab in second window, wait until tab restore service history is created.
        CallbackHelper closedCallback = new CallbackHelper();
        secondModel.addObserver(new TabClosedObserver(closedCallback));
        closeTabOnUiThread(secondModel, secondModel.getTabAt(1), false);

        try {
            closedCallback.waitForCallback(0);
        } catch (TimeoutException | InterruptedException e) {
            fail("Failed to close the tab on the second window.");
        }

        assertEquals("Window 2 should have 1 tab.", 1, secondModel.getCount());

        // Closed the second window. Must wait until it's totally closed.
        int numExpectedActivities = ApplicationStatus.getRunningActivities().size() - 1;
        secondActivity.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(Criteria.equals(numExpectedActivities, new Callable<Integer>() {
            @Override
            public Integer call() {
                return ApplicationStatus.getRunningActivities().size();
            }
        }));
        assertEquals("Window 1 should have 1 tab.", 1, firstModel.getCount());

        // Restore closed tab from second window. It should be created in first window.
        openMostRecentlyClosedTabOnUiThread(firstSelector);
        assertEquals("Closed tab in second window should be restored in the first window.", 2,
                firstModel.getCount());
        Tab tab0 = firstModel.getTabAt(0);
        Tab tab1 = firstModel.getTabAt(1);
        Tab[] firstWindowTabs = new Tab[]{tab0, tab1};
        checkState(firstModel, firstWindowTabs, tab0, EMPTY, firstWindowTabs, tab0);
        assertEquals(TEST_URL_1, tab1.getUrl());
    }
}
