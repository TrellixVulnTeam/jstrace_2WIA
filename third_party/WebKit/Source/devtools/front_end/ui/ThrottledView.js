// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {WebInspector.SimpleView}
 * @param {string} title
 * @param {boolean=} isWebComponent
 */
WebInspector.ThrottledView = function(title, isWebComponent)
{
    WebInspector.SimpleView.call(this, title, isWebComponent);
    this._updateThrottler = new WebInspector.Throttler(100);
    this._updateWhenVisible = false;
}

WebInspector.ThrottledView.prototype = {
    /**
     * @protected
     * @return {!Promise.<?>}
     */
    doUpdate: function()
    {
        return Promise.resolve();
    },

    update: function()
    {
        this._updateWhenVisible = !this.isShowing();
        if (this._updateWhenVisible)
            return;
        this._updateThrottler.schedule(innerUpdate.bind(this));

        /**
         * @this {WebInspector.ThrottledView}
         * @return {!Promise.<?>}
         */
        function innerUpdate()
        {
            if (this.isShowing()) {
                return this.doUpdate();
            } else {
                this._updateWhenVisible = true;
                return Promise.resolve();
            }
        }
    },

    /**
     * @override
     */
    wasShown: function()
    {
        WebInspector.SimpleView.prototype.wasShown.call(this);
        if (this._updateWhenVisible)
            this.update();
    },

    __proto__: WebInspector.SimpleView.prototype
}
