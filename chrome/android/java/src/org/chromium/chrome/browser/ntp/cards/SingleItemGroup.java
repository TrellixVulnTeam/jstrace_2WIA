// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import java.util.Collections;
import java.util.List;

/**
 * A single item that represents itself as a group.
 */
public abstract class SingleItemGroup implements ItemGroup, NewTabPageListItem {
    private final List<NewTabPageListItem> mItems =
            Collections.<NewTabPageListItem>singletonList(this);

    @Override
    public List<NewTabPageListItem> getItems() {
        return mItems;
    }
}
