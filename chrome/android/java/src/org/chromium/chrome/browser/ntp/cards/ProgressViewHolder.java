// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.view.LayoutInflater;

import org.chromium.chrome.R;

/**
 * ViewHolder for an item of type {@link NewTabPageListItem#VIEW_TYPE_PROGRESS}.
 * Adds a {@link ProgressIndicatorView} to the recycler view.
 */
public class ProgressViewHolder extends NewTabPageViewHolder {
    private final ProgressIndicatorView mProgressIndicator;
    private ProgressListItem mListItem;

    public ProgressViewHolder(final NewTabPageRecyclerView recyclerView) {
        super(LayoutInflater.from(recyclerView.getContext())
                        .inflate(R.layout.new_tab_page_progress_indicator, recyclerView, false));
        mProgressIndicator = (ProgressIndicatorView) itemView.findViewById(R.id.snippets_progress);
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem item) {
        mListItem = (ProgressListItem) item;
        updateDisplay();
    }

    public void updateDisplay() {
        if (mListItem.isVisible()) {
            mProgressIndicator.showDelayed();
        } else {
            mProgressIndicator.hide();
        }
    }
}
