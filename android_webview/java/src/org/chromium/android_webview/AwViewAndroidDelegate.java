// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.VisibleForTesting;
import org.chromium.content.browser.RenderCoordinates;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.gfx.DeviceDisplayInfo;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for WebView.
 */
public class AwViewAndroidDelegate extends ViewAndroidDelegate {
    /**
     * The current container view. This view can be updated with
     * {@link #updateCurrentContainerView()}.
     */
    private ViewGroup mContainerView;

    /**
     * List of anchor views stored in the order in which they were acquired mapped
     * to their position.
     */
    private final Map<View, Position> mAnchorViews = new LinkedHashMap<>();

    private final RenderCoordinates mRenderCoordinates;

    /**
     * Represents the position of an anchor view.
     */
    @VisibleForTesting
    private static class Position {
        public final float mX;
        public final float mY;
        public final float mWidth;
        public final float mHeight;
        public final int mLeftMargin;
        public final int mTopMargin;

        public Position(float x, float y, float width, float height, int leftMargin,
                int topMargin) {
            mX = x;
            mY = y;
            mWidth = width;
            mHeight = height;
            mLeftMargin = leftMargin;
            mTopMargin = topMargin;
        }
    }

    AwViewAndroidDelegate(ViewGroup containerView, RenderCoordinates renderCoordinates) {
        mContainerView = containerView;
        mRenderCoordinates = renderCoordinates;
    }

    @Override
    public View acquireView() {
        ViewGroup containerView = getContainerView();
        if (containerView == null) return null;
        View anchorView = new View(containerView.getContext());
        containerView.addView(anchorView);
        // |mAnchorViews| will be updated with the right view position in |setViewPosition|.
        mAnchorViews.put(anchorView, null);
        return anchorView;
    }

    @Override
    public void removeView(View anchorView) {
        mAnchorViews.remove(anchorView);
        ViewGroup containerView = getContainerView();
        if (containerView != null) {
            containerView.removeView(anchorView);
        }
    }

    /**
    * Updates the current container view to which this class delegates. Existing anchor views
    * are transferred from the old to the new container view.
    */
    public void updateCurrentContainerView(ViewGroup containerView) {
        ViewGroup oldContainerView = getContainerView();
        mContainerView = containerView;
        for (Entry<View, Position> entry : mAnchorViews.entrySet()) {
            View anchorView = entry.getKey();
            Position position = entry.getValue();
            if (oldContainerView != null) {
                oldContainerView.removeView(anchorView);
            }
            containerView.addView(anchorView);
            if (position != null) {
                float scale = (float) DeviceDisplayInfo.create(containerView.getContext())
                        .getDIPScale();
                setViewPosition(anchorView, position.mX, position.mY,
                        position.mWidth, position.mHeight, scale,
                        position.mLeftMargin, position.mTopMargin);
            }
        }
    }

    @SuppressWarnings("deprecation")  // AbsoluteLayout
    @Override
    public void setViewPosition(View anchorView, float x, float y, float width, float height,
            float scale, int leftMargin, int topMargin) {
        ViewGroup containerView = getContainerView();
        if (!mAnchorViews.containsKey(anchorView) || containerView == null) return;

        mAnchorViews.put(anchorView, new Position(x, y, width, height, leftMargin, topMargin));

        if (containerView instanceof FrameLayout) {
            super.setViewPosition(anchorView, x, y, width, height, scale, leftMargin, topMargin);
            return;
        }
        // This fixes the offset due to a difference in
        // scrolling model of WebView vs. Chrome.
        // TODO(sgurun) fix this to use mContainerViewAtCreation.getScroll[X/Y]()
        // as it naturally accounts for scroll differences between
        // these models.
        leftMargin += mRenderCoordinates.getScrollXPixInt();
        topMargin += mRenderCoordinates.getScrollYPixInt();

        int scaledWidth = Math.round(width * scale);
        int scaledHeight = Math.round(height * scale);
        android.widget.AbsoluteLayout.LayoutParams lp =
                new android.widget.AbsoluteLayout.LayoutParams(
                    scaledWidth, scaledHeight, leftMargin, topMargin);
        anchorView.setLayoutParams(lp);
    }

    @Override
    public ViewGroup getContainerView() {
        return mContainerView;
    }
}
