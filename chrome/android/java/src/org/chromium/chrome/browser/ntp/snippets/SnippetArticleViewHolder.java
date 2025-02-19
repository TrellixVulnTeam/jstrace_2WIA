// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.media.ThumbnailUtils;
import android.support.v4.text.BidiFormatter;
import android.text.format.DateUtils;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewTreeObserver;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.favicon.FaviconHelper.IconAvailabilityCallback;
import org.chromium.chrome.browser.ntp.DisplayStyleObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.NewTabPageView.NewTabPageManager;
import org.chromium.chrome.browser.ntp.UiConfig;
import org.chromium.chrome.browser.ntp.cards.CardViewHolder;
import org.chromium.chrome.browser.ntp.cards.DisplayStyleObserverAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageListItem;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.variations.VariationsAssociatedData;

import java.net.URI;
import java.net.URISyntaxException;

/**
 * A class that represents the view for a single card snippet.
 */
public class SnippetArticleViewHolder extends CardViewHolder
        implements MenuItem.OnMenuItemClickListener {
    private static final String TAG = "NtpSnippets";
    private static final String PUBLISHER_FORMAT_STRING = "%s - %s";
    private static final int FADE_IN_ANIMATION_TIME_MS = 300;
    private static final int[] FAVICON_SERVICE_SUPPORTED_SIZES = {16, 24, 32, 48, 64};
    private static final String FAVICON_SERVICE_FORMAT =
            "https://s2.googleusercontent.com/s2/favicons?domain=%s&src=chrome_newtab_mobile&sz=%d&alt=404";

    // The variation parameter to fetch the value from the favicon service.
    private static final String PARAMETER_FAVICON_SERVICE_NAME = "favicons_fetch_from_service";
    private static final String PARAMETER_DISABLED_VALUE = "off";

    // ContextMenu item ids. These must be unique.
    private static final int ID_OPEN_IN_NEW_WINDOW = 0;
    private static final int ID_OPEN_IN_NEW_TAB = 1;
    private static final int ID_OPEN_IN_INCOGNITO_TAB = 2;
    private static final int ID_SAVE_FOR_OFFLINE = 3;
    private static final int ID_REMOVE = 4;

    private final NewTabPageManager mNewTabPageManager;
    private final SnippetsSource mSnippetsSource;
    private final TextView mHeadlineTextView;
    private final TextView mPublisherTextView;
    private final TextView mArticleSnippetTextView;
    private final ImageView mThumbnailView;

    private FetchImageCallback mImageCallback;
    private SnippetArticleListItem mArticle;
    private ViewTreeObserver.OnPreDrawListener mPreDrawObserver;
    private int mPublisherFaviconSizePx;

    private final boolean mUseFaviconService;

    /**
     * Constructs a SnippetCardItemView item used to display snippets
     *
     * @param parent The ViewGroup that is going to contain the newly created view.
     * @param manager The NTPManager object used to open an article
     * @param snippetsSource The SnippetsBridge used to retrieve the snippet thumbnails.
     * @param uiConfig The NTP UI configuration object used to adjust the article UI.
     */
    public SnippetArticleViewHolder(NewTabPageRecyclerView parent, NewTabPageManager manager,
            SnippetsSource snippetsSource, UiConfig uiConfig) {
        super(R.layout.new_tab_page_snippets_card, parent, uiConfig);

        mNewTabPageManager = manager;
        mSnippetsSource = snippetsSource;
        mThumbnailView = (ImageView) itemView.findViewById(R.id.article_thumbnail);
        mHeadlineTextView = (TextView) itemView.findViewById(R.id.article_headline);
        mPublisherTextView = (TextView) itemView.findViewById(R.id.article_publisher);
        mArticleSnippetTextView = (TextView) itemView.findViewById(R.id.article_snippet);

        mPreDrawObserver = new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                if (mArticle != null && !mArticle.impressionTracked()) {
                    Rect r = new Rect(0, 0, itemView.getWidth(), itemView.getHeight());
                    itemView.getParent().getChildVisibleRect(itemView, r, null);
                    // Track impression if at least one third of the snippet is shown.
                    if (r.height() >= itemView.getHeight() / 3) mArticle.trackImpression();
                }
                // Proceed with the current drawing pass.
                return true;
            }
        };

        // Listen to onPreDraw only if this view is potentially visible (attached to the window).
        itemView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                itemView.getViewTreeObserver().addOnPreDrawListener(mPreDrawObserver);
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                itemView.getViewTreeObserver().removeOnPreDrawListener(mPreDrawObserver);
            }
        });

        new DisplayStyleObserverAdapter(itemView, uiConfig, new DisplayStyleObserver() {
            @Override
            public void onDisplayStyleChanged(@UiConfig.DisplayStyle int newDisplayStyle) {
                if (newDisplayStyle == UiConfig.DISPLAY_STYLE_NARROW) {
                    mHeadlineTextView.setMaxLines(4);
                    mArticleSnippetTextView.setVisibility(View.GONE);
                } else {
                    mHeadlineTextView.setMaxLines(2);
                    mArticleSnippetTextView.setVisibility(View.VISIBLE);
                }
            }
        });

        mUseFaviconService =
                !PARAMETER_DISABLED_VALUE.equals(VariationsAssociatedData.getVariationParamValue(
                        NewTabPage.FIELD_TRIAL_NAME, PARAMETER_FAVICON_SERVICE_NAME));
    }

    @Override
    public void onCardTapped() {
        mNewTabPageManager.openSnippet(mArticle.mUrl);
        mArticle.trackClick();
    }

    @Override
    protected void createContextMenu(ContextMenu menu) {
        RecordHistogram.recordSparseSlowlyHistogram(
                "NewTabPage.Snippets.CardLongPressed", mArticle.mPosition);
        mArticle.recordAgeAndScore("NewTabPage.Snippets.CardLongPressed");

        // Create a context menu akin to the one shown for MostVisitedItems.
        if (mNewTabPageManager.isOpenInNewWindowEnabled()) {
            addContextMenuItem(menu, ID_OPEN_IN_NEW_WINDOW, R.string.contextmenu_open_in_new_tab);
        }

        addContextMenuItem(menu, ID_OPEN_IN_NEW_TAB, R.string.contextmenu_open_in_new_tab);

        if (mNewTabPageManager.isOpenInIncognitoEnabled()) {
            addContextMenuItem(menu, ID_OPEN_IN_INCOGNITO_TAB,
                    R.string.contextmenu_open_in_incognito_tab);
        }

        if (OfflinePageBridge.isBackgroundLoadingEnabled()
                && OfflinePageBridge.canSavePage(mArticle.mUrl)) {
            addContextMenuItem(menu, ID_SAVE_FOR_OFFLINE,
                    R.string.contextmenu_save_offline);
        }

        addContextMenuItem(menu, ID_REMOVE, R.string.remove);
    }

    /**
     * Convenience method to reduce multi-line function call to single line.
     */
    private void addContextMenuItem(ContextMenu menu, int id, int resourceId) {
        menu.add(Menu.NONE, id, Menu.NONE, resourceId).setOnMenuItemClickListener(this);
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        // The UMA is used to compare how the user views the article linked from a snippet.
        switch (item.getItemId()) {
            case ID_OPEN_IN_NEW_WINDOW:
                NewTabPageUma.recordOpenSnippetMethod(
                        NewTabPageUma.OPEN_SNIPPET_METHODS_NEW_WINDOW);
                mNewTabPageManager.openUrlInNewWindow(mArticle.mUrl);
                return true;
            case ID_OPEN_IN_NEW_TAB:
                NewTabPageUma.recordOpenSnippetMethod(
                        NewTabPageUma.OPEN_SNIPPET_METHODS_NEW_TAB);
                mNewTabPageManager.openUrlInNewTab(mArticle.mUrl, false);
                return true;
            case ID_OPEN_IN_INCOGNITO_TAB:
                NewTabPageUma.recordOpenSnippetMethod(
                        NewTabPageUma.OPEN_SNIPPET_METHODS_INCOGNITO);
                mNewTabPageManager.openUrlInNewTab(mArticle.mUrl, true);
                return true;
            case ID_SAVE_FOR_OFFLINE:
                NewTabPageUma.recordOpenSnippetMethod(
                        NewTabPageUma.OPEN_SNIPPET_METHODS_SAVE_FOR_OFFLINE);
                OfflinePageBridge bridge =
                        OfflinePageBridge.getForProfile(Profile.getLastUsedProfile());
                bridge.savePageLaterForDownload(mArticle.mUrl);
                return true;
            case ID_REMOVE:
                assert isDismissable() : "Context menu should not be shown for peeking card.";
                // UMA is recorded during dismissal.
                dismiss();
                return true;
            default:
                return false;
        }
    }

    @Override
    public void onBindViewHolder(NewTabPageListItem article) {
        super.onBindViewHolder(article);

        mArticle = (SnippetArticleListItem) article;

        mHeadlineTextView.setText(mArticle.mTitle);

        // We format the publisher here so that having a publisher name in an RTL language doesn't
        // mess up the formatting on an LTR device and vice versa.
        String publisherAttribution = String.format(PUBLISHER_FORMAT_STRING,
                BidiFormatter.getInstance().unicodeWrap(mArticle.mPublisher),
                DateUtils.getRelativeTimeSpanString(mArticle.mPublishTimestampMilliseconds,
                        System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS));
        mPublisherTextView.setText(publisherAttribution);

        // The favicon of the publisher should match the textview height.
        int widthSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mPublisherTextView.measure(widthSpec, heightSpec);
        mPublisherFaviconSizePx = mPublisherTextView.getMeasuredHeight();

        mArticleSnippetTextView.setText(mArticle.mPreviewText);

        // If there's still a pending thumbnail fetch, cancel it.
        cancelImageFetch();

        // If the article has a thumbnail already, reuse it. Otherwise start a fetch.
        if (mArticle.getThumbnailBitmap() != null) {
            mThumbnailView.setImageBitmap(mArticle.getThumbnailBitmap());
        } else {
            mThumbnailView.setImageResource(R.drawable.ic_snippet_thumbnail_placeholder);
            mImageCallback = new FetchImageCallback(this, mArticle);
            mSnippetsSource.fetchSnippetImage(mArticle, mImageCallback);
        }

        // Set the favicon of the publisher.
        try {
            fetchFaviconFromLocalCache(new URI(mArticle.mUrl), true);
        } catch (URISyntaxException e) {
            setDefaultFaviconOnView();
        }
    }

    private static class FetchImageCallback extends Callback<Bitmap> {
        private SnippetArticleViewHolder mViewHolder;
        private final SnippetArticleListItem mSnippet;

        public FetchImageCallback(
                SnippetArticleViewHolder viewHolder, SnippetArticleListItem snippet) {
            mViewHolder = viewHolder;
            mSnippet = snippet;
        }

        @Override
        public void onResult(Bitmap image) {
            if (mViewHolder == null) return;
            mViewHolder.fadeThumbnailIn(mSnippet, image);
        }

        public void cancel() {
            // TODO(treib): Pass the "cancel" on to the actual image fetcher.
            mViewHolder = null;
        }
    }

    private void cancelImageFetch() {
        if (mImageCallback != null) {
            mImageCallback.cancel();
            mImageCallback = null;
        }
    }

    private void fadeThumbnailIn(SnippetArticleListItem snippet, Bitmap thumbnail) {
        mImageCallback = null;
        if (thumbnail == null) return; // Nothing to do, we keep the placeholder.

        // We need to crop and scale the downloaded bitmap, as the ImageView we set it on won't be
        // able to do so when using a TransitionDrawable (as opposed to the straight bitmap).
        // That's a limitation of TransitionDrawable, which doesn't handle layers of varying sizes.
        Resources res = mThumbnailView.getResources();
        int targetSize = res.getDimensionPixelSize(R.dimen.snippets_thumbnail_size);
        Bitmap scaledThumbnail = ThumbnailUtils.extractThumbnail(
                thumbnail, targetSize, targetSize, ThumbnailUtils.OPTIONS_RECYCLE_INPUT);

        // Store the bitmap to skip the download task next time we display this snippet.
        snippet.setThumbnailBitmap(scaledThumbnail);

        // Cross-fade between the placeholder and the thumbnail.
        Drawable[] layers = {mThumbnailView.getDrawable(),
                new BitmapDrawable(mThumbnailView.getResources(), scaledThumbnail)};
        TransitionDrawable transitionDrawable = new TransitionDrawable(layers);
        mThumbnailView.setImageDrawable(transitionDrawable);
        transitionDrawable.startTransition(FADE_IN_ANIMATION_TIME_MS);
    }

    private void fetchFaviconFromLocalCache(final URI snippetUri, final boolean fallbackToService) {
        mNewTabPageManager.getLocalFaviconImageForURL(
                getSnippetDomain(snippetUri), mPublisherFaviconSizePx, new FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap image, String iconUrl) {
                        if (image == null && fallbackToService) {
                            fetchFaviconFromService(snippetUri);
                            return;
                        }
                        setFaviconOnView(image);
                    }
                });
    }

    private void fetchFaviconFromService(final URI snippetUri) {
        // Show the default favicon immediately.
        setDefaultFaviconOnView();

        if (!mUseFaviconService) return;
        int sizePx = getFaviconServiceSupportedSize();
        if (sizePx == 0) return;

        // Replace the default icon by another one from the service when it is fetched.
        mNewTabPageManager.ensureIconIsAvailable(
                getSnippetDomain(snippetUri), // Store to the cache for the whole domain.
                String.format(FAVICON_SERVICE_FORMAT, snippetUri.getHost(), sizePx),
                /*useLargeIcon=*/false, /*isTemporary=*/true, new IconAvailabilityCallback() {
                    @Override
                    public void onIconAvailabilityChecked(boolean newlyAvailable) {
                        if (!newlyAvailable) return;
                        // The download succeeded, the favicon is in the cache; fetch it.
                        fetchFaviconFromLocalCache(snippetUri, /*fallbackToService=*/false);
                    }
                });
    }

    private int getFaviconServiceSupportedSize() {
        // Take the smallest size larger than mFaviconSizePx.
        for (int size : FAVICON_SERVICE_SUPPORTED_SIZES) {
            if (size > mPublisherFaviconSizePx) return size;
        }
        // Or at least the largest available size (unless too small).
        int largestSize =
                FAVICON_SERVICE_SUPPORTED_SIZES[FAVICON_SERVICE_SUPPORTED_SIZES.length - 1];
        if (mPublisherFaviconSizePx <= largestSize * 1.5) return largestSize;
        return 0;
    }

    private String getSnippetDomain(URI snippetUri) {
        return String.format("%s://%s", snippetUri.getScheme(), snippetUri.getHost());
    }

    private void setDefaultFaviconOnView() {
        setFaviconOnView(ApiCompatibilityUtils.getDrawable(
                mPublisherTextView.getContext().getResources(), R.drawable.default_favicon));
    }

    private void setFaviconOnView(Bitmap image) {
        setFaviconOnView(new BitmapDrawable(mPublisherTextView.getContext().getResources(), image));
    }

    private void setFaviconOnView(Drawable drawable) {
        drawable.setBounds(0, 0, mPublisherFaviconSizePx, mPublisherFaviconSizePx);
        ApiCompatibilityUtils.setCompoundDrawablesRelative(
                mPublisherTextView, drawable, null, null, null);
        mPublisherTextView.setVisibility(View.VISIBLE);
    }

    @Override
    public boolean isDismissable() {
        return !isPeeking();
    }
}
